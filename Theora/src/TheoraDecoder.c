/*
 *  TheoraDecoder.c
 *
 *    Theora video decoder (ImageCodec) implementation.
 *
 *
 *  Copyright (c) 2006  Arek Korbik
 *
 *  This file is part of XiphQT, the Xiph QuickTime Components.
 *
 *  XiphQT is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  XiphQT is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with XiphQT; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *
 *  Last modified: $Id$
 *
 */

/*
 *  The implementation in this file is based on the 'ElectricImageCodec' and
 *  the 'ExampleIPBCodec' example QuickTime components.
 */

#if defined(__APPLE_CC__)
#include <Carbon/Carbon.h>
#include <QuickTime/QuickTime.h>
#else
#include <ConditionalMacros.h>
#include <Endian.h>
#include <QuickTimeComponents.h>
#include <ImageCodec.h>
#endif /* __APPLE_CC__ */

#include "decoder_types.h"
#include "theora_versions.h"

#include "data_types.h"
#include "TheoraDecoder.h"
#include "debug.h"


static OSStatus CopyPlanarYCbCr420ToChunkyYUV422(size_t width, size_t height, th_ycbcr_buffer pb, UInt8 *baseAddr_2vuy, long rowBytes_2vuy);
static OSStatus CopyPlanarYCbCr422ToChunkyYUV422(size_t width, size_t height, th_ycbcr_buffer pb, UInt8 *baseAddr_2vuy, long rowBytes_2vuy);
static OSStatus CopyPlanarYCbCr444ToChunkyYUV422(size_t width, size_t height, th_ycbcr_buffer pb, UInt8 *baseAddr_2vuy, long rowBytes_2vuy);
static OSErr CopyPlanarYCbCr422ToPlanarYUV422(th_ycbcr_buffer ycbcr, ICMDataProcRecordPtr dataProc, UInt8 *baseAddr, long stride, long width, long height);

// Setup required for ComponentDispatchHelper.c
#define IMAGECODEC_BASENAME() 		Theora_ImageCodec
#define IMAGECODEC_GLOBALS() 		Theora_Globals storage

#define CALLCOMPONENT_BASENAME()	IMAGECODEC_BASENAME()
#define	CALLCOMPONENT_GLOBALS()		IMAGECODEC_GLOBALS()

#define COMPONENT_UPP_PREFIX()		uppImageCodec
#define COMPONENT_DISPATCH_FILE		"TheoraDecoderDispatch.h"
#define COMPONENT_SELECT_PREFIX()  	kImageCodec

#define	GET_DELEGATE_COMPONENT()	(storage->delegateComponent)

#if defined(__APPLE_CC__)
#include <CoreServices/Components.k.h>
#include <QuickTime/ImageCodec.k.h>
#include <QuickTime/ComponentDispatchHelper.c>
#else
#include <Components.k.h>
#include <ImageCodec.k.h>
#include <ComponentDispatchHelper.c>
#endif /* __APPLE_CC__ */


OSErr init_theora_decoder(Theora_Globals glob, CodecDecompressParams *p)
{
    OSErr err = noErr;
    Handle ext;
    OggSerialNoAtom *atom;
    Byte *ptrheader, *mCookie;
    UInt32 mCookieSize;
    CookieAtomHeader *aheader;
    th_comment tc;
    ogg_packet op;
    int i = 0;

    if (glob->info_initialised) {
        dbg_printf("--:Theora:- Decoder already initialised, skipping...\n");
        return err;
    }

    err = GetImageDescriptionExtension(p->imageDescription, &ext, kSampleDescriptionExtensionTheora, 1);
    if (err != noErr) {
        dbg_printf("XXX GetImageDescriptionExtension() failed!\n");
        err = codecBadDataErr;
        return err;
    }

    mCookie = *ext;
    mCookieSize = GetHandleSize(ext);

    atom = (OggSerialNoAtom*)mCookie;
    ptrheader = mCookie + EndianU32_BtoN(atom->size);
    aheader = (CookieAtomHeader*)ptrheader;

    err = codecBadDataErr;
    // scan quickly through the cookie, check types and packet sizes
    if (EndianS32_BtoN(atom->type) != kCookieTypeOggSerialNo || (UInt32) (ptrheader - mCookie) > mCookieSize)
        return err;
    ptrheader += EndianU32_BtoN(aheader->size);
    if (EndianS32_BtoN(aheader->type) != kCookieTypeTheoraHeader || (UInt32) (ptrheader - mCookie) > mCookieSize)
        return err;
    aheader = (CookieAtomHeader*) ptrheader;
    ptrheader += EndianU32_BtoN(aheader->size);
    if (EndianS32_BtoN(aheader->type) != kCookieTypeTheoraComments || (UInt32) (ptrheader - mCookie) > mCookieSize)
        return err;
    aheader = (CookieAtomHeader*) ptrheader;
    ptrheader += EndianU32_BtoN(aheader->size);
    if (EndianS32_BtoN(aheader->type) != kCookieTypeTheoraCodebooks || (UInt32) (ptrheader - mCookie) > mCookieSize)
        return err;

    // all OK, back to the first theora packet
    aheader = (CookieAtomHeader*) (mCookie + EndianU32_BtoN(atom->size));

    th_info_init(&glob->ti);
    th_comment_init(&tc);
    glob->ts = NULL;

    op.b_o_s = 1;
    op.e_o_s = 0;
    op.granulepos = 0;
    op.packetno = 0;
    op.bytes = EndianU32_BtoN(aheader->size) - 2 * sizeof(long); // FIXME??
    op.packet = aheader->data;

    if (th_decode_headerin(&glob->ti, &tc, &glob->ts, &op) < 0) {

        if (glob->ts != NULL)
            th_setup_free (glob->ts);
        th_comment_clear(&tc);
        th_info_clear(&glob->ti);

        return err;
    }

    op.b_o_s = 0;

    while (i < 2) {
        aheader = (CookieAtomHeader*) ((Byte*) (aheader) + EndianU32_BtoN(aheader->size));
        op.packetno += 1;
        op.bytes = EndianU32_BtoN(aheader->size) - 2 * sizeof(long); // FIXME??
        op.packet = aheader->data;

        th_decode_headerin(&glob->ti, &tc, &glob->ts, &op);
        i++;
    }

    err = noErr;

    th_comment_clear(&tc);

    dbg_printf("--:Theora:- OK, managed to initialize the decoder somehow...\n");
    glob->info_initialised = true;

    return err;
}


pascal ComponentResult Theora_ImageCodecOpen(Theora_Globals glob, ComponentInstance self)
{
	ComponentResult err;

	glob = (Theora_Globals)NewPtrClear(sizeof(Theora_GlobalsRecord));
        dbg_printf("\n--:Theora:- CodecOpen(%08lx) called\n", (long)glob);
	if (err = MemError()) goto bail;

	SetComponentInstanceStorage(self, (Handle)glob);

	glob->self = self;
	glob->target = self;
	glob->wantedDestinationPixelTypeH = (OSType **)NewHandle(sizeof(OSType) * (kNumPixelFormatsSupported + 1));
	if (err = MemError()) goto bail;
	glob->drawBandUPP = NULL;
        glob->info_initialised = false;
        glob->last_frame = -1;

        glob->p_buffer = NewPtr(kPacketBufferAllocIncrement);
        glob->p_buffer_len = kPacketBufferAllocIncrement;
        glob->p_buffer_used = 0;

        // many of the functions are delegated actually
	err = OpenADefaultComponent(decompressorComponentType, kBaseCodecType, &glob->delegateComponent);
	if (err) goto bail;

	ComponentSetTarget(glob->delegateComponent, self);

bail:
	return err;
}

pascal ComponentResult Theora_ImageCodecClose(Theora_Globals glob, ComponentInstance self)
{
    dbg_printf("--:Theora:- CodecClose(%08lx) called\n\n", (long)glob);
	// Make sure to close the base component and dealocate our storage
	if (glob) {
		if (glob->delegateComponent) {
			CloseComponent(glob->delegateComponent);
		}
		if (glob->wantedDestinationPixelTypeH) {
			DisposeHandle((Handle)glob->wantedDestinationPixelTypeH);
		}
		if (glob->drawBandUPP) {
			DisposeImageCodecMPDrawBandUPP(glob->drawBandUPP);
		}

                if (glob->p_buffer) {
                    DisposePtr((Ptr) glob->p_buffer);
                    glob->p_buffer = NULL;
                }
		DisposePtr((Ptr)glob);
	}

	return noErr;
}

pascal ComponentResult Theora_ImageCodecVersion(Theora_Globals glob)
{
#pragma unused(glob)
    return kTheora_imdc_Version;
}

pascal ComponentResult Theora_ImageCodecTarget(Theora_Globals glob, ComponentInstance target)
{
    glob->target = target;
    return noErr;
}

pascal ComponentResult Theora_ImageCodecInitialize(Theora_Globals glob, ImageSubCodecDecompressCapabilities *cap)
{
    dbg_printf("--:Theora:- CodecInitalize(%08lx) called\n", (long)glob);

    cap->decompressRecordSize = sizeof(Theora_DecompressRecord);
    cap->canAsync = true;

    if (cap->recordSize > offsetof(ImageSubCodecDecompressCapabilities, baseCodecShouldCallDecodeBandForAllFrames) ) {
        cap->subCodecIsMultiBufferAware = true;
        cap->baseCodecShouldCallDecodeBandForAllFrames = true;
    }

    return noErr;
}

pascal ComponentResult Theora_ImageCodecPreflight(Theora_Globals glob, CodecDecompressParams *p)
{
    CodecCapabilities *capabilities = p->capabilities;
    OSTypePtr         formats = *glob->wantedDestinationPixelTypeH;
    OSErr             ret = noErr;

    dbg_printf("--:Theora:- CodecPreflight(%08lx) called (seqid: %08lx, frN: %8ld, first: %d, data1: %02x)\n",
               (long)glob, p->sequenceID, p->frameNumber, (p->conditionFlags & codecConditionFirstFrame) != 1, p->data[0]);
    dbg_printf("         :- image: %dx%d, pixform: %x\n", (**p->imageDescription).width, (**p->imageDescription).height, glob->ti.pixel_fmt);

    /* only decode full images at the moment */
    capabilities->bandMin = (**p->imageDescription).height;
    capabilities->bandInc = capabilities->bandMin;

    capabilities->wantedPixelSize  = 0;
    p->wantedDestinationPixelTypes = glob->wantedDestinationPixelTypeH;

    capabilities->extendWidth = 0;
    capabilities->extendHeight = 0;

    ret = init_theora_decoder(glob, p);

    if (ret == noErr) {
        *formats++  = k422YpCbCr8PixelFormat;
        if (glob->ti.pixel_fmt == TH_PF_420)
            *formats++  = kYUV420PixelFormat;
        *formats++	= 0;
    }

    return ret;
}

pascal ComponentResult Theora_ImageCodecBeginBand(Theora_Globals glob, CodecDecompressParams *p, ImageSubCodecDecompressRecord *drp, long flags)
{
#pragma unused(flags)
    long offsetH, offsetV;
    Theora_DecompressRecord *myDrp = (Theora_DecompressRecord *)drp->userDecompressRecord;

    dbg_printf("--:Theora:- CodecBeginBand(%08lx, %08lx, %08lx) called (seqid: %08lx, frN: %8ld, first: %d, data1: %02x) (pixF: '%4.4s')\n",
               (long)glob, (long)drp, (long)myDrp, p->sequenceID, p->frameNumber, (p->conditionFlags & codecConditionFirstFrame) != 1,
               p->data[0], &p->dstPixMap.pixelFormat);
    if (p->frameTime != NULL) {
        dbg_printf("--:Theora:-      BeginBand::frameTime: scale: %8ld, duration: %8ld, rate: %5ld.%05ld (vd: %8ld)\n",
                   p->frameTime->scale, p->frameTime->duration, p->frameTime->rate >> 16, p->frameTime->rate & 0xffff,
                   (p->frameTime->flags & icmFrameTimeHasVirtualStartTimeAndDuration) ? p->frameTime->virtualDuration : -1);
    }

#if 0
    switch (p->dstPixMap.pixelFormat) {
    case k422YpCbCr8PixelFormat:
        offsetH = (long)(p->dstRect.left - p->dstPixMap.bounds.left) * (long)(p->dstPixMap.pixelSize >> 3);
        offsetV = (long)(p->dstRect.top - p->dstPixMap.bounds.top) * (long)drp->rowBytes;

        drp->baseAddr = p->dstPixMap.baseAddr + offsetH + offsetV;
        break;

    case kYUV420PixelFormat:
        //drp->baseAddr = p->dstPixMap.baseAddr;
        break;

    default:
        //should not happen!
        return codecErr;
    }
#endif /* 0 */

    if ((*(unsigned char *)drp->codecData) & 0x40 == 0)
        drp->frameType = kCodecFrameTypeKey;
    else
        drp->frameType = kCodecFrameTypeDifference;

    myDrp->width = (**p->imageDescription).width;
    myDrp->height = (**p->imageDescription).height;
    myDrp->depth = (**p->imageDescription).depth;
    myDrp->dataSize = p->bufferSize;
    myDrp->frameNumber = p->frameNumber;
    myDrp->pixelFormat = p->dstPixMap.pixelFormat;
    myDrp->draw = 0;

    if (glob->last_frame < 0) {
        dbg_printf("--:Theora:-  calling theora_decode_init()...\n");
        glob->td = th_decode_alloc(&glob->ti, glob->ts);
        glob->last_frame = 0;
    }

    return noErr;
}

pascal ComponentResult Theora_ImageCodecDecodeBand(Theora_Globals glob, ImageSubCodecDecompressRecord *drp, unsigned long flags)
{
    OSErr err = noErr;
    Theora_DecompressRecord *myDrp = (Theora_DecompressRecord *)drp->userDecompressRecord;
    unsigned char *dataPtr = (unsigned char *)drp->codecData;
    ICMDataProcRecordPtr dataProc = drp->dataProcRecord.dataProc ? &drp->dataProcRecord : NULL;
    SInt32 dataAvailable = dataProc != NULL ? codecMinimumDataSize : -1;

    dbg_printf("--:Theora:-  CodecDecodeBand(%08lx, %08lx, %08lx) cald (                 frN: %8ld, dataProc: %8lx)\n",
               (long)glob, (long)drp, (long)myDrp, myDrp->frameNumber, (long)dataProc);

#if 0
    // TODO: implement using dataProc for loading data if not all available at once
    if (dataAvailable > -1) {
        unsigned char *tmpDataPtr = dataPtr;
        UInt32 bytesToLoad = myDrp->dataSize;
        while (dataAvailable > 0) {
            err = dataProc->dataProc((Ptr *)&dataPtr, dataNeeded, dataProc->dataRefCon);
            if (err == eofErr)
                err = noErr;
        }
    }
#endif /* 0 */

    
    {
        ogg_packet op;
        int terr;
        Boolean drop = false;
        Boolean continued = (glob->p_buffer_used > 0);
        UInt8 *data_buffer = dataPtr;
        UInt32 data_size = myDrp->dataSize;

        if (glob->last_frame + 1 != myDrp->frameNumber) {
            glob->p_buffer_used = 0;
            continued = false;
        }

        glob->last_frame = myDrp->frameNumber;

        if (myDrp->dataSize % 255 == 4) {
            // TODO: extend the checks below with frame duration checks
            if (!memcmp(dataPtr + myDrp->dataSize - 4, "OggS", 4)) {
                err = codecDroppedFrameErr;
                drop = true;
                if (myDrp->dataSize + glob->p_buffer_used > glob->p_buffer_len) {
                    // TODO: implement reallocation with expansion
                    err = codecErr;
                } else {
                    BlockMoveData(dataPtr, glob->p_buffer + glob->p_buffer_used, myDrp->dataSize - 4);
                    glob->p_buffer_used += myDrp->dataSize - 4;
                }
            }
        } else if (myDrp->dataSize == 1) {
            myDrp->dataSize = 0;
            myDrp->draw = 1;
        } else {
            myDrp->draw = 1;
        }

        if (!drop && continued) {
            /* this should be the last fragment */
            if (myDrp->dataSize > 0)
                BlockMoveData(dataPtr, glob->p_buffer + glob->p_buffer_used, myDrp->dataSize);
            data_size = myDrp->dataSize + glob->p_buffer_used;
            glob->p_buffer_used = 0;
            data_buffer = glob->p_buffer;
        }

        if (drop)
            err = codecDroppedFrameErr;

        if (!drop && myDrp->draw != 0) {
            op.b_o_s = 0;
            op.e_o_s = 0;
            op.granulepos = -1;
            op.packetno = myDrp->frameNumber + 3;
            op.bytes = data_size;
            op.packet = data_buffer;
            terr = th_decode_packetin(glob->td, &op, NULL);
            dbg_printf("--:Theora:-  theora_decode_packetin() = %d\n", terr);

            if (terr != 0) {
                myDrp->draw = 0;
                err = codecDroppedFrameErr;
            }
        }
    }

    
    return err;
}


pascal ComponentResult Theora_ImageCodecDrawBand(Theora_Globals glob, ImageSubCodecDecompressRecord *drp)
{
    OSErr err = noErr;
    Theora_DecompressRecord *myDrp = (Theora_DecompressRecord *)drp->userDecompressRecord;
    unsigned char *dataPtr = (unsigned char *)drp->codecData;
    ICMDataProcRecordPtr dataProc = drp->dataProcRecord.dataProc ? &drp->dataProcRecord : NULL;

    dbg_printf("--:Theora:-  CodecDrawBand(%08lx, %08lx, %08lx) called (                 frN: %8ld, dataProc: %8lx)\n",
               (long)glob, (long)drp, (long)myDrp, myDrp->frameNumber, (long)dataProc);

    if (myDrp->draw == 0)
        err = codecDroppedFrameErr;
    else {
        th_ycbcr_buffer ycbcrB;
        dbg_printf("--:Theora:-  calling theora_decode_YUVout()...\n");
        th_decode_ycbcr_out(glob->td, ycbcrB);
        if (myDrp->pixelFormat == k422YpCbCr8PixelFormat) {
            if (glob->ti.pixel_fmt == TH_PF_420) {
                err = CopyPlanarYCbCr420ToChunkyYUV422(myDrp->width, myDrp->height, ycbcrB, (UInt8 *)drp->baseAddr, drp->rowBytes);
            } else if (glob->ti.pixel_fmt == TH_PF_422) {
                err = CopyPlanarYCbCr422ToChunkyYUV422(myDrp->width, myDrp->height, ycbcrB, (UInt8 *)drp->baseAddr, drp->rowBytes);
            } else if (glob->ti.pixel_fmt == TH_PF_444) {
                err = CopyPlanarYCbCr444ToChunkyYUV422(myDrp->width, myDrp->height, ycbcrB, (UInt8 *)drp->baseAddr, drp->rowBytes);
            } else {
                dbg_printf("--:Theora:-  'What PLANET is this!?' (%d)\n", glob->ti.pixel_fmt);
                err = codecBadDataErr;
            }
        } else if (myDrp->pixelFormat == kYUV420PixelFormat) {
            err = CopyPlanarYCbCr422ToPlanarYUV422(ycbcrB, dataProc, (UInt8 *)drp->baseAddr, drp->rowBytes, myDrp->width, myDrp->height);
        }
    }

    //err = noErr;
    //err = codecBadDataErr;
    return err;
}

pascal ComponentResult Theora_ImageCodecEndBand(Theora_Globals glob, ImageSubCodecDecompressRecord *drp, OSErr result, long flags)
{
#pragma unused(glob, drp,result, flags)
    dbg_printf("--:Theora:-   CodecEndBand(%08lx, %08lx, %08lx, %08lx) called\n", (long)glob, (long)drp, (long)drp->userDecompressRecord, result);

    return noErr;
}

pascal ComponentResult Theora_ImageCodecQueueStarting(Theora_Globals glob)
{
#pragma unused(glob)
    dbg_printf("--:Theora:- CodecQueueStarting(%08lx) called\n", (long)glob);

    return noErr;
}

pascal ComponentResult Theora_ImageCodecQueueStopping(Theora_Globals glob)
{
#pragma unused(glob)
    dbg_printf("--:Theora:- CodecQueueStopping(%08lx) called\n", (long)glob);

    return noErr;
}

pascal ComponentResult Theora_ImageCodecGetCompressedImageSize(Theora_Globals glob, ImageDescriptionHandle desc,
                                                               Ptr data, long dataSize, ICMDataProcRecordPtr dataProc, long *size)
{
#pragma	unused(glob,dataSize,dataProc,desc)
    dbg_printf("--:Theora:- CodecGetCompressedImageSize(%08lx) called (dataSize: %8ld)\n", (long)glob, dataSize);

    if (size == NULL)
        return paramErr;

    //size = 0;
    //return noErr;
    return unimpErr;
}

pascal ComponentResult Theora_ImageCodecGetCodecInfo(Theora_Globals glob, CodecInfo *info)
{
    OSErr err = noErr;
    dbg_printf("--:Theora:- CodecGetCodecInfo(%08lx) called\n", (long)glob);

    if (info == NULL) {
        err = paramErr;
    }
    else {
        CodecInfo **tempCodecInfo;

        err = GetComponentResource((Component)glob->self, codecInfoResourceType, kTheoraDecoderResID, (Handle *)&tempCodecInfo);
        if (err == noErr) {
            *info = **tempCodecInfo;
            DisposeHandle((Handle)tempCodecInfo);
        }
    }

    return err;
}


#pragma mark-

#if TARGET_RT_BIG_ENDIAN
#define PACK_2VUY(Cb, Y1, Cr, Y2) ((UInt32) (((Cb) << 24) | ((Y1) << 16) | ((Cr) << 8) | (Y2)))
#else
#define PACK_2VUY(Cb, Y1, Cr, Y2) ((UInt32) ((Cb) | ((Y1) << 8) | ((Cr) << 16) | ((Y2) << 24)))
#endif /* TARGET_RT_BIG_ENDIAN */

OSStatus CopyPlanarYCbCr420ToChunkyYUV422(size_t width, size_t height, th_ycbcr_buffer pb, UInt8 *baseAddr_2vuy, long rowBytes_2vuy)
{
    dbg_printf("BLIT: Yw: %d, Yh: %d, Ys: %d;  w: %ld,  h: %ld; stride: %ld\n", pb[0].width, pb[0].height, pb[0].ystride, width, height, rowBytes_2vuy);
    dbg_printf("BLIT: Bw: %d, Bh: %d, Bs: %d; Rw: %d, Rh: %d;     Rs: %d\n", pb[1].width, pb[1].height, pb[1].ystride,
               pb[2].width, pb[2].height, pb[2].ystride);

    size_t x, y;
    const UInt8 *lineBase_Y  = pb[0].data;
    const UInt8 *lineBase_Cb = pb[1].data;
    const UInt8 *lineBase_Cr = pb[2].data;
    UInt8 *lineBase_2vuy = baseAddr_2vuy;
    for( y = 0; y < height; y += 2 ) {
        // Take two lines at a time.
        const UInt8 *pixelPtr_Y_top  = lineBase_Y;
        const UInt8 *pixelPtr_Y_bot  = lineBase_Y  + pb[0].ystride;
        const UInt8 *pixelPtr_Cb = lineBase_Cb;
        const UInt8 *pixelPtr_Cr = lineBase_Cr;
        UInt8 *pixelPtr_2vuy_top = lineBase_2vuy;
        UInt8 *pixelPtr_2vuy_bot = lineBase_2vuy + rowBytes_2vuy;
        for( x = 0; x < width; x += 2 ) {
            *((UInt32 *)pixelPtr_2vuy_top) = PACK_2VUY(*pixelPtr_Cb, *pixelPtr_Y_top, *pixelPtr_Cr, *(pixelPtr_Y_top + 1));
            pixelPtr_2vuy_top += 4;
            pixelPtr_Y_top += 2;

            *((UInt32 *)pixelPtr_2vuy_bot) = PACK_2VUY(*pixelPtr_Cb++, *pixelPtr_Y_bot, *pixelPtr_Cr++, *(pixelPtr_Y_bot + 1));
            pixelPtr_2vuy_bot += 4;
            pixelPtr_Y_bot += 2;
        }

        lineBase_Y += 2 * pb[0].ystride;
        lineBase_Cb += pb[1].ystride;
        lineBase_Cr += pb[2].ystride;
        lineBase_2vuy += 2 * rowBytes_2vuy;
    }
    return noErr;
}

OSStatus CopyPlanarYCbCr422ToChunkyYUV422(size_t width, size_t height, th_ycbcr_buffer pb, UInt8 *baseAddr_2vuy, long rowBytes_2vuy)
{
    dbg_printf("BLIT> Yw: %d, Yh: %d, Ys: %d;  w: %ld,  h: %ld; stride: %ld\n", pb[0].width, pb[0].height, pb[0].ystride, width, height, rowBytes_2vuy);
    dbg_printf("BLIT> Bw: %d, Bh: %d, Bs: %d; Rw: %d, Rh: %d;     Rs: %d\n", pb[1].width, pb[1].height, pb[1].ystride,
               pb[2].width, pb[2].height, pb[2].ystride);

    size_t x, y;
    const UInt8 *lineBase_Y  = pb[0].data;
    const UInt8 *lineBase_Cb = pb[1].data;
    const UInt8 *lineBase_Cr = pb[2].data;
    UInt8 *lineBase_2vuy = baseAddr_2vuy;
    for( y = 0; y < height; y += 2 ) {
        // Take two lines at a time.
        const UInt8 *pixelPtr_Y_top  = lineBase_Y;
        const UInt8 *pixelPtr_Y_bot  = lineBase_Y  + pb[0].ystride;
        const UInt8 *pixelPtr_Cb_top = lineBase_Cb;
        const UInt8 *pixelPtr_Cb_bot = lineBase_Cb + pb[1].ystride;
        const UInt8 *pixelPtr_Cr_top = lineBase_Cr;
        const UInt8 *pixelPtr_Cr_bot = lineBase_Cr + pb[2].ystride;
        UInt8 *pixelPtr_2vuy_top = lineBase_2vuy;
        UInt8 *pixelPtr_2vuy_bot = lineBase_2vuy + rowBytes_2vuy;
        for( x = 0; x < width; x += 2 ) {
            //*((UInt32 *)pixelPtr_2vuy_top) = (UInt32) ((*pixelPtr_Cb_top++) << 24 | ((*pixelPtr_Y_top)) << 16 | ((*pixelPtr_Cr_top++)) << 8 | ((*(pixelPtr_Y_top+1))));
            *((UInt32 *)pixelPtr_2vuy_top) = PACK_2VUY(*pixelPtr_Cb_top++, *pixelPtr_Y_top, *pixelPtr_Cr_top++, *(pixelPtr_Y_top + 1));
            pixelPtr_2vuy_top += 4;
            pixelPtr_Y_top += 2;

            //*((UInt32 *)pixelPtr_2vuy_bot) = (UInt32) ((*pixelPtr_Cb_bot++) << 24 | ((*pixelPtr_Y_bot)) << 16 | ((*pixelPtr_Cr_bot++)) << 8 | ((*(pixelPtr_Y_bot+1))));
            *((UInt32 *)pixelPtr_2vuy_bot) = PACK_2VUY(*pixelPtr_Cb_bot++, *pixelPtr_Y_bot, *pixelPtr_Cr_bot++, *(pixelPtr_Y_bot + 1));
            pixelPtr_2vuy_bot += 4;
            pixelPtr_Y_bot += 2;
        }

        lineBase_Y += 2 * pb[0].ystride;
        lineBase_Cb += 2 * pb[1].ystride;
        lineBase_Cr += 2 * pb[2].ystride;
        lineBase_2vuy += 2 * rowBytes_2vuy;
    }
    return noErr;
}


/* !!: At the moment this function does nice 'decimation' rather than subsampling!!(?)
   TODO: proper subsampling? */
OSStatus CopyPlanarYCbCr444ToChunkyYUV422(size_t width, size_t height, th_ycbcr_buffer pb, UInt8 *baseAddr_2vuy, long rowBytes_2vuy)
{
    dbg_printf("BLIT? Yw: %d, Yh: %d, Ys: %d;  w: %ld,  h: %ld; stride: %ld\n", pb[0].width, pb[0].height, pb[0].ystride, width, height, rowBytes_2vuy);
    dbg_printf("BLIT? Bw: %d, Bh: %d, Bs: %d; Rw: %d, Rh: %d;     Rs: %d\n", pb[1].width, pb[1].height, pb[1].ystride,
               pb[2].width, pb[2].height, pb[2].ystride);

    size_t x, y;
    const UInt8 *lineBase_Y  = pb[0].data;
    const UInt8 *lineBase_Cb = pb[1].data;
    const UInt8 *lineBase_Cr = pb[2].data;
    UInt8 *lineBase_2vuy = baseAddr_2vuy;
    for( y = 0; y < height; y += 2 ) {
        // Take two lines at a time.
        const UInt8 *pixelPtr_Y_top  = lineBase_Y;
        const UInt8 *pixelPtr_Y_bot  = lineBase_Y  + pb[0].ystride;
        const UInt8 *pixelPtr_Cb_top = lineBase_Cb;
        const UInt8 *pixelPtr_Cb_bot = lineBase_Cb + pb[1].ystride;
        const UInt8 *pixelPtr_Cr_top = lineBase_Cr;
        const UInt8 *pixelPtr_Cr_bot = lineBase_Cr + pb[2].ystride;
        UInt8 *pixelPtr_2vuy_top = lineBase_2vuy;
        UInt8 *pixelPtr_2vuy_bot = lineBase_2vuy + rowBytes_2vuy;
        for( x = 0; x < width; x += 2 ) {
            *((UInt32 *)pixelPtr_2vuy_top) = PACK_2VUY(*pixelPtr_Cb_top, *pixelPtr_Y_top, *pixelPtr_Cr_top, *(pixelPtr_Y_top + 1));
            pixelPtr_2vuy_top += 4;
            pixelPtr_Y_top += 2;
            pixelPtr_Cb_top += 2;
            pixelPtr_Cr_top += 2;

            *((UInt32 *)pixelPtr_2vuy_bot) = PACK_2VUY(*pixelPtr_Cb_bot, *pixelPtr_Y_bot, *pixelPtr_Cr_bot, *(pixelPtr_Y_bot + 1));
            pixelPtr_2vuy_bot += 4;
            pixelPtr_Y_bot += 2;
            pixelPtr_Cb_bot += 2;
            pixelPtr_Cr_bot += 2;
        }

        lineBase_Y += 2 * pb[0].ystride;
        lineBase_Cb += 2 * pb[1].ystride;
        lineBase_Cr += 2 * pb[2].ystride;
        lineBase_2vuy += 2 * rowBytes_2vuy;
    }
    return noErr;
}

/* Presently, This function assumes YCbCr 4:2:0 as input.
   TODO: take into account different subsampling types? */
OSErr CopyPlanarYCbCr422ToPlanarYUV422(th_ycbcr_buffer ycbcr, ICMDataProcRecordPtr dataProc, UInt8 *baseAddr, long stride, long width, long height)
{
    OSErr err = noErr;
    UInt8 *endOfScanLine, *dst_base, *src_base;
    PlanarPixmapInfoYUV420 *pinfo = (PlanarPixmapInfoYUV420 *) baseAddr;
    UInt32 lines;
    SInt32 dst_stride, src_stride;
    endOfScanLine = baseAddr + (width * 4);

    dbg_printf("BLIT= yw: %d, yh: %d, ys: %d; w: %ld, h: %ld; stride: %ld\n", ycbcr[0].width, ycbcr[0].height, ycbcr[0].ystride, width, height, stride);
    dbg_printf("BLIT= Bw: %d, Bh: %d, Bs: %d; Rw: %d, Rh: %d;     Rs: %d\n", ycbcr[1].width, ycbcr[1].height, ycbcr[1].ystride,
               ycbcr[2].width, ycbcr[2].height, ycbcr[2].ystride);

    lines = height;
    dst_base = baseAddr + pinfo->componentInfoY.offset;
    dst_stride = pinfo->componentInfoY.rowBytes;
    src_base = ycbcr[0].data;
    src_stride = ycbcr[0].ystride;
    while (lines-- > 0) {
        BlockMoveData(src_base, dst_base, width);
        src_base += src_stride;
        dst_base += dst_stride;
    }

    lines = height / 2;
    dst_base = baseAddr + pinfo->componentInfoCb.offset;
    dst_stride = pinfo->componentInfoCb.rowBytes;
    src_base = ycbcr[1].data;
    src_stride = ycbcr[1].ystride;
    while (lines-- > 0) {
        BlockMoveData(src_base, dst_base, width);
        src_base += src_stride;
        dst_base += dst_stride;
    }

    lines = height / 2;
    dst_base = baseAddr + pinfo->componentInfoCr.offset;
    dst_stride = pinfo->componentInfoCr.rowBytes;
    src_base = ycbcr[2].data;
    src_stride = ycbcr[2].ystride;
    while (lines-- > 0) {
        BlockMoveData(src_base, dst_base, width);
        src_base += src_stride;
        dst_base += dst_stride;
    }

    return err;
}
