/*
 *  CAOpusDecoder.cpp
 *
 *    CAOpusDecoder class implementation; the main part of the Vorbis
 *    codec functionality.
 *
 *
 *  Copyright (c) 2005-2006  Arek Korbik
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
 *  Last modified: $Id: CAOpusDecoder.cpp 12754 2007-03-14 03:51:23Z arek $
 *
 */


#include <Ogg/ogg.h>
#include <Opus/opus.h>

#include "CAOpusDecoder.h"

#include "CABundleLocker.h"

#include "vorbis_versions.h"
#include "fccs.h"
#include "data_types.h"

//#define NDEBUG
#include "debug.h"

#define DBG_STREAMDESC_FMT " [CASBD: sr=%lf, fmt=%4.4s, fl=%x, bpp=%d, fpp=%d, bpf=%d, ch=%d, bpc=%d]"
#define DBG_STREAMDESC_FILL(x) (x)->mSampleRate, reinterpret_cast<const char*> (&((x)->mFormatID)), \
        (unsigned int)(x)->mFormatFlags, (unsigned int)(x)->mBytesPerPacket, (unsigned int)(x)->mFramesPerPacket, (unsigned int)(x)->mBytesPerPacket, \
        (unsigned int)(x)->mChannelsPerFrame, (unsigned int)(x)->mBitsPerChannel


CAOpusDecoder::CAOpusDecoder(Boolean inSkipFormatsInitialization /* = false */) :
    mCookie(NULL), mCookieSize(0), mCompressionInitialized(false),
    mVorbisFPList(), mConsumedFPList(),
    mFullInPacketsZapped(0)
{
    if (inSkipFormatsInitialization)
        return;

    CAStreamBasicDescription theInputFormat(kAudioStreamAnyRate, kAudioFormatXiphOpus,
                                            kVorbisBytesPerPacket, kVorbisFramesPerPacket,
                                            kVorbisBytesPerFrame, kVorbisChannelsPerFrame,
                                            kVorbisBitsPerChannel, kVorbisFormatFlags);
    AddInputFormat(theInputFormat);

    mInputFormat.mSampleRate = 48000.0;
    mInputFormat.mFormatID = kAudioFormatXiphOpus;
    mInputFormat.mFormatFlags = kVorbisFormatFlags;
    mInputFormat.mBytesPerPacket = kVorbisBytesPerPacket;
    mInputFormat.mFramesPerPacket = kVorbisFramesPerPacket;
    mInputFormat.mBytesPerFrame = kVorbisBytesPerFrame;
    mInputFormat.mChannelsPerFrame = 2;
    mInputFormat.mBitsPerChannel = kVorbisBitsPerChannel;

    CAStreamBasicDescription theOutputFormat1(kAudioStreamAnyRate, kAudioFormatLinearPCM, 0, 1, 0, 0, 16,
                                              kAudioFormatFlagsNativeEndian |
                                              kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked);
    AddOutputFormat(theOutputFormat1);
    CAStreamBasicDescription theOutputFormat2(kAudioStreamAnyRate, kAudioFormatLinearPCM, 0, 1, 0, 0, 32,
                                              kAudioFormatFlagsNativeFloatPacked);
    AddOutputFormat(theOutputFormat2);

    mOutputFormat.mSampleRate = 44100.0;
    mOutputFormat.mFormatID = kAudioFormatLinearPCM;
    mOutputFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
    mOutputFormat.mBytesPerPacket = 8;
    mOutputFormat.mFramesPerPacket = 1;
    mOutputFormat.mBytesPerFrame = 8;
    mOutputFormat.mChannelsPerFrame = 2;
    mOutputFormat.mBitsPerChannel = 32;
}

CAOpusDecoder::~CAOpusDecoder()
{
    if (mCookie != NULL)
        delete[] mCookie;

    if (mCompressionInitialized) {
		opus_decoder_destroy(oDecoder);
    //    vorbis_block_clear(&mV_vb);
    //    vorbis_dsp_clear(&mV_vd);

    //    vorbis_info_clear(&mV_vi);
    }
}

void CAOpusDecoder::Initialize(const AudioStreamBasicDescription* inInputFormat,
                                 const AudioStreamBasicDescription* inOutputFormat,
                                 const void* inMagicCookie, UInt32 inMagicCookieByteSize)
{
    dbg_printf("[VD  ]  >> [%08lx] :: Initialize(%d, %d, %d)\n", (size_t) this, inInputFormat != NULL, inOutputFormat != NULL, inMagicCookieByteSize != 0);
    if (inInputFormat)
        dbg_printf("[VD  ]   > [%08lx] :: InputFormat :" DBG_STREAMDESC_FMT "\n", (size_t) this, DBG_STREAMDESC_FILL(inInputFormat));
    if (inOutputFormat)
        dbg_printf("[VD  ]   > [%08lx] :: OutputFormat:" DBG_STREAMDESC_FMT "\n", (size_t) this, DBG_STREAMDESC_FILL(inOutputFormat));

    if(inInputFormat != NULL) {
        SetCurrentInputFormat(*inInputFormat);
    }

    if(inOutputFormat != NULL) {
        SetCurrentOutputFormat(*inOutputFormat);
    }

    if ((mInputFormat.mSampleRate != mOutputFormat.mSampleRate) ||
        (mInputFormat.mChannelsPerFrame != mOutputFormat.mChannelsPerFrame)) {
        CODEC_THROW(kAudioCodecUnsupportedFormatError);
    }

    BDCInitialize(kVorbisDecoderBufferSize);

    //if (inMagicCookieByteSize == 0)
    //    CODEC_THROW(kAudioCodecUnsupportedFormatError);

    if (inMagicCookieByteSize != 0) {
        SetMagicCookie(inMagicCookie, inMagicCookieByteSize);
    }

    if (mCompressionInitialized)
        FixFormats();

    if (mCompressionInitialized)
        XCACodec::Initialize(inInputFormat, inOutputFormat, inMagicCookie, inMagicCookieByteSize);
    dbg_printf("[VD  ]  <  [%08lx] :: InputFormat :" DBG_STREAMDESC_FMT "\n", (size_t) this, DBG_STREAMDESC_FILL(&mInputFormat));
    dbg_printf("[VD  ]  <  [%08lx] :: OutputFormat:" DBG_STREAMDESC_FMT "\n", (size_t) this, DBG_STREAMDESC_FILL(&mOutputFormat));
    dbg_printf("[VD  ] <.. [%08lx] :: Initialize(%d, %d, %d)\n", (size_t) this, inInputFormat != NULL, inOutputFormat != NULL, inMagicCookieByteSize != 0);
}

void CAOpusDecoder::Uninitialize()
{
    dbg_printf("[VD  ]  >> [%08lx] :: Uninitialize()\n", (size_t) this);
    BDCUninitialize();

    XCACodec::Uninitialize();
    dbg_printf("[VD  ] <.. [%08lx] :: Uninitialize()\n", (size_t) this);
}

void CAOpusDecoder::GetProperty(AudioCodecPropertyID inPropertyID, UInt32& ioPropertyDataSize, void* outPropertyData)
{
    dbg_printf("[VD  ]  >> [%08lx] :: GetProperty('%4.4s', %d)\n", (size_t) this, reinterpret_cast<char*> (&inPropertyID), (unsigned int)ioPropertyDataSize);
    switch(inPropertyID)
    {
    case kAudioCodecPropertyRequiresPacketDescription: // 'pakd'
        if(ioPropertyDataSize == sizeof(UInt32)) {
            *reinterpret_cast<UInt32*>(outPropertyData) = 1;
        } else {
            CODEC_THROW(kAudioCodecBadPropertySizeError);
        }
        break;

    case kAudioCodecPropertyHasVariablePacketByteSizes: // 'vpk?'
        if(ioPropertyDataSize == sizeof(UInt32)) {
            *reinterpret_cast<UInt32*>(outPropertyData) = 1;
        } else {
            CODEC_THROW(kAudioCodecBadPropertySizeError);
        }
        break;

    case kAudioCodecPropertyPacketFrameSize: // 'pakf'
        if(ioPropertyDataSize == sizeof(UInt32))
        {
            /* The following line has been changed according to Apple engineers' suggestion
               I received via Steve Nicolai (in response to *my* bugreport, I think...).
                 (Why don't they just implement the VBR-VFPP properly? *sigh*)

               The original line is left here as I still believe that's how it should be
               implemented according to the QT docs. (And in case this workaround stops
               working again one wonderful morning!) */
            // *reinterpret_cast<UInt32*>(outPropertyData) = kVorbisFramesPerPacket;
            //*reinterpret_cast<UInt32*>(outPropertyData) = kVorbisFramesPerPacketReported;
            *reinterpret_cast<UInt32*>(outPropertyData) = kVorbisFramesPerPacket;
        } else {
            CODEC_THROW(kAudioCodecBadPropertySizeError);
        }
        break;

    case kAudioCodecPropertyMaximumPacketByteSize: // 'pakb'
        if(ioPropertyDataSize == sizeof(UInt32)) {
            *reinterpret_cast<UInt32*>(outPropertyData) = kVorbisFormatMaxBytesPerPacket;
        } else {
            CODEC_THROW(kAudioCodecBadPropertySizeError);
        }
        break;

    case kAudioCodecPropertyCurrentInputSampleRate: // 'cisr'
        if (ioPropertyDataSize == sizeof(Float64)) {
            *reinterpret_cast<Float64*>(outPropertyData) = mInputFormat.mSampleRate;
        } else if (ioPropertyDataSize == sizeof(UInt32)) {
            *reinterpret_cast<UInt32*>(outPropertyData) = mInputFormat.mSampleRate;
        } else {
            CODEC_THROW(kAudioCodecBadPropertySizeError);
        }
        break;

    case kAudioCodecPropertyCurrentOutputSampleRate: // 'cosr'
        if (ioPropertyDataSize == sizeof(Float64)) {
            *reinterpret_cast<Float64*>(outPropertyData) = mOutputFormat.mSampleRate;
        } else if (ioPropertyDataSize == sizeof(UInt32)) {
            *reinterpret_cast<UInt32*>(outPropertyData) = mOutputFormat.mSampleRate;
        } else {
            CODEC_THROW(kAudioCodecBadPropertySizeError);
        }
        break;

        //case kAudioCodecPropertyQualitySetting: ???
#if TARGET_OS_MAC
    case kAudioCodecPropertyNameCFString: // 'lnam'
        {
            if (ioPropertyDataSize != sizeof(CFStringRef)) CODEC_THROW(kAudioCodecBadPropertySizeError);

            CABundleLocker lock;
            CFStringRef name = CFCopyLocalizedStringFromTableInBundle(CFSTR("Xiph Opus decoder"), CFSTR("CodecNames"), GetCodecBundle(), CFSTR(""));
            *(CFStringRef*)outPropertyData = name;
            break;
        }

        //case kAudioCodecPropertyManufacturerCFString:
#endif
    default:
        ACBaseCodec::GetProperty(inPropertyID, ioPropertyDataSize, outPropertyData);
    }
    dbg_printf("[VD  ] <.. [%08lx] :: GetProperty('%4.4s')\n", (size_t) this, reinterpret_cast<char*> (&inPropertyID));
}

void CAOpusDecoder::GetPropertyInfo(AudioCodecPropertyID inPropertyID, UInt32& outPropertyDataSize, bool& outWritable)
{
    dbg_printf("[VD  ]  >> [%08lx] :: GetPropertyInfo('%4.4s')\n", (size_t) this, reinterpret_cast<char*> (&inPropertyID));
    switch(inPropertyID)
    {
    case kAudioCodecPropertyRequiresPacketDescription: // 'pakd'
        outPropertyDataSize = sizeof(UInt32);
        outWritable = false;
        break;

    case kAudioCodecPropertyHasVariablePacketByteSizes: // 'vpk?'
        outPropertyDataSize = sizeof(UInt32);
        outWritable = false;
        break;

    case kAudioCodecPropertyPacketFrameSize: // 'pakf'
        outPropertyDataSize = sizeof(UInt32);
        outWritable = false;
        break;

    case kAudioCodecPropertyMaximumPacketByteSize: // 'pakb'
        outPropertyDataSize = sizeof(UInt32);
        outWritable = false;
        break;

    case kAudioCodecPropertyCurrentInputSampleRate:  // 'cisr'
    case kAudioCodecPropertyCurrentOutputSampleRate: // 'cosr'
        outPropertyDataSize = sizeof(Float64);
        outWritable = false;
        break;

    default:
        ACBaseCodec::GetPropertyInfo(inPropertyID, outPropertyDataSize, outWritable);
        break;

    }
    dbg_printf("[VD  ] <.. [%08lx] :: GetPropertyInfo('%4.4s')\n", (size_t) this, reinterpret_cast<char*> (&inPropertyID));
}

void CAOpusDecoder::Reset()
{
    dbg_printf("[VD  ] >> [%08lx] :: Reset()\n", (size_t) this);
    BDCReset();

    XCACodec::Reset();
    dbg_printf("[VD  ] << [%08lx] :: Reset()\n", (size_t) this);
}

UInt32 CAOpusDecoder::GetVersion() const
{
    return kCAVorbis_adec_Version;
}


void CAOpusDecoder::SetCurrentInputFormat(const AudioStreamBasicDescription& inInputFormat)
{
    if (!mIsInitialized) {
        //	check to make sure the input format is legal
        if (inInputFormat.mFormatID != kAudioFormatXiphOpus) {
            dbg_printf("CAOpusDecoder::SetFormats: only support Xiph Opus for input\n");
            CODEC_THROW(kAudioCodecUnsupportedFormatError);
        }

        //	tell our base class about the new format
        XCACodec::SetCurrentInputFormat(inInputFormat);
    } else {
        CODEC_THROW(kAudioCodecStateError);
    }
}

void CAOpusDecoder::SetCurrentOutputFormat(const AudioStreamBasicDescription& inOutputFormat)
{
    if (!mIsInitialized) {
        //	check to make sure the output format is legal
        if ((inOutputFormat.mFormatID != kAudioFormatLinearPCM) ||
            !(((inOutputFormat.mFormatFlags == kAudioFormatFlagsNativeFloatPacked) &&
               (inOutputFormat.mBitsPerChannel == 32)) ||
              ((inOutputFormat.mFormatFlags == (kLinearPCMFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked)) &&
               (inOutputFormat.mBitsPerChannel == 16))))
        {
            dbg_printf("CAOpusDecoder::SetFormats: only supports either 16 bit native endian signed integer or 32 bit native endian Core Audio floats for output\n");
            CODEC_THROW(kAudioCodecUnsupportedFormatError);
        }

        //	tell our base class about the new format
        XCACodec::SetCurrentOutputFormat(inOutputFormat);
    } else {
        CODEC_THROW(kAudioCodecStateError);
    }
}

UInt32 CAOpusDecoder::GetMagicCookieByteSize() const
{
    return mCookieSize;
}

#define BlockMoveData(src, dest, size) memmove(dest, src, size)

void CAOpusDecoder::GetMagicCookie(void* outMagicCookieData, UInt32& ioMagicCookieDataByteSize) const
{
    if (mCookie != NULL) {
        ioMagicCookieDataByteSize = mCookieSize;
        BlockMoveData(mCookie, outMagicCookieData, ioMagicCookieDataByteSize);
    } else {
        ioMagicCookieDataByteSize = 0;
    }
}

void CAOpusDecoder::SetMagicCookie(const void* inMagicCookieData, UInt32 inMagicCookieDataByteSize)
{
    dbg_printf("[VD  ]  >> [%08lx] :: SetMagicCookie()\n", (size_t) this);
    if (mIsInitialized)
        CODEC_THROW(kAudioCodecStateError);

    SetCookie(inMagicCookieData, inMagicCookieDataByteSize);

    InitializeCompressionSettings();

    if (!mCompressionInitialized)
        CODEC_THROW(kAudioCodecUnsupportedFormatError);
    dbg_printf("[VD  ] <.. [%08lx] :: SetMagicCookie()\n", (size_t) this);
}

void CAOpusDecoder::SetCookie(const void* inMagicCookieData, UInt32 inMagicCookieDataByteSize)
{
    if (mCookie != NULL)
        delete[] mCookie;

    mCookieSize = inMagicCookieDataByteSize;
    if (inMagicCookieDataByteSize != 0) {
        mCookie = new Byte[inMagicCookieDataByteSize];
        BlockMoveData(inMagicCookieData, mCookie, inMagicCookieDataByteSize);
    } else {
        mCookie = NULL;
    }
}



void CAOpusDecoder::FixFormats()
{
    dbg_printf("[VD  ]  >> [%p] :: FixFormats()\n", this);
    //mInputFormat.mSampleRate = mV_vi.rate;
    //mInputFormat.mChannelsPerFrame = mV_vi.channels;
    mInputFormat.mBitsPerChannel = 0;
    mInputFormat.mBytesPerPacket = 0;
    mInputFormat.mFramesPerPacket = 0;

    //long long_blocksize = (reinterpret_cast<long *>(mV_vi.codec_setup))[1];
    //mInputFormat.mFramesPerPacket = long_blocksize;

    /*
      mInputFormat.mFramesPerPacket = 64;
      mInputFormat.mBytesPerPacket = mInputFormat.mChannelsPerFrame * 34;
      mInputFormat.mBytesPerFrame = 0;
    */
    dbg_printf("[VD  ] <.. [%08lx] :: FixFormats()\n", (size_t) this);
}


void CAOpusDecoder::InitializeCompressionSettings()
{
    if (mCookie == NULL)
        return;

    if (mCompressionInitialized) {
		opus_decoder_destroy(oDecoder);
        //vorbis_block_clear(&mV_vb);
        //vorbis_dsp_clear(&mV_vd);

        //vorbis_info_clear(&mV_vi);
    }

    mCompressionInitialized = false;

    Byte *ptrheader = mCookie;
    Byte *cend = mCookie + mCookieSize;
    CookieAtomHeader *aheader = reinterpret_cast<CookieAtomHeader*> (ptrheader);
    ogg_packet header, header_vc, header_cb;
    header.bytes = header_vc.bytes = header_cb.bytes = 0;

    while (ptrheader < cend) {
        aheader = reinterpret_cast<CookieAtomHeader*> (ptrheader);
        ptrheader += EndianU32_BtoN(aheader->size);
        if (ptrheader > cend || EndianU32_BtoN(aheader->size) <= 0)
            break;

        switch(EndianS32_BtoN(aheader->type)) {
        case kCookieTypeVorbisHeader:
            header.b_o_s = 1;
            header.e_o_s = 0;
            header.granulepos = 0;
            header.packetno = 0;
            header.bytes = EndianS32_BtoN(aheader->size) - 2 * sizeof(int);
            header.packet = aheader->data;
            break;

        case kCookieTypeVorbisComments:
            header_vc.b_o_s = 0;
            header_vc.e_o_s = 0;
            header_vc.granulepos = 0;
            header_vc.packetno = 1;
            header_vc.bytes = EndianS32_BtoN(aheader->size) - 2 * sizeof(int);
            header_vc.packet = aheader->data;
            break;

        case kCookieTypeVorbisCodebooks:
            header_cb.b_o_s = 0;
            header_cb.e_o_s = 0;
            header_cb.granulepos = 0;
            header_cb.packetno = 2;
            header_cb.bytes = EndianS32_BtoN(aheader->size) - 2 * sizeof(int);
            header_cb.packet = aheader->data;
            break;

        default:
            break;
        }
    }

    if (header.bytes == 0 || header_vc.bytes == 0 || header_cb.bytes == 0)
        return;

	oDecoder = opus_decoder_create(48000, 2, NULL);
    //vorbis_comment vc;

    //vorbis_info_init(&mV_vi);
    //vorbis_comment_init(&vc);

    //if (vorbis_synthesis_headerin(&mV_vi, &vc, &header) < 0) {
    //    vorbis_comment_clear(&vc);
    //    vorbis_info_clear(&mV_vi);
//
    //    return;
    //}

    //vorbis_synthesis_headerin(&mV_vi, &vc, &header_vc);
    //vorbis_synthesis_headerin(&mV_vi, &vc, &header_cb);

    //vorbis_synthesis_init(&mV_vd, &mV_vi);
    //vorbis_block_init(&mV_vd, &mV_vb);

    //vorbis_comment_clear(&vc);

    mCompressionInitialized = true;
}

#pragma mark BDC handling

void CAOpusDecoder::BDCInitialize(UInt32 inInputBufferByteSize)
{
    XCACodec::BDCInitialize(inInputBufferByteSize);
}

void CAOpusDecoder::BDCUninitialize()
{
    mVorbisFPList.clear();
    mConsumedFPList.clear();
    mFullInPacketsZapped = 0;

    XCACodec::BDCUninitialize();
}

void CAOpusDecoder::BDCReset()
{
    mVorbisFPList.clear();
    mConsumedFPList.clear();
    mFullInPacketsZapped = 0;

    //vorbis_synthesis_restart(&mV_vd);

    //vorbis_block_clear(&globals->vb);
    //vorbis_block_init(&globals->vd, &globals->vb);

    XCACodec::BDCReset();
}

void CAOpusDecoder::BDCReallocate(UInt32 inInputBufferByteSize)
{
    mVorbisFPList.clear();
    mConsumedFPList.clear();
    mFullInPacketsZapped = 0;

    XCACodec::BDCReallocate(inInputBufferByteSize);
}


void CAOpusDecoder::InPacket(const void* inInputData, const AudioStreamPacketDescription* inPacketDescription)
{
    const Byte * theData = static_cast<const Byte *> (inInputData) + inPacketDescription->mStartOffset;
    UInt32 size = inPacketDescription->mDataByteSize;
    mBDCBuffer.In(theData, size);
    mVorbisFPList.push_back(VorbisFramePacket(inPacketDescription->mVariableFramesInPacket, inPacketDescription->mDataByteSize));
}


UInt32 CAOpusDecoder::FramesReady() const
{
    //return mNumFrames; // TODO: definitely probably not right!!
    //return vorbis_synthesis_pcmout(const_cast<vorbis_dsp_state*> (&mV_vd), NULL);
	return 0;
}

Boolean CAOpusDecoder::GenerateFrames()
{
    Boolean ret = true;

    mBDCStatus = kBDCStatusOK;
	/*
    while (vorbis_synthesis_pcmout(&mV_vd, NULL) == 0 && !mVorbisFPList.empty()) {
        ogg_packet op;
        int vErr;
        Boolean gaap = false;
        VorbisFramePacket &sfp = mVorbisFPList.front();

        op.b_o_s = 0;
        op.e_o_s = 0;
        op.granulepos = -1;
        op.packetno = 0; // ??!
        op.bytes = sfp.bytes; // FIXME??
        op.packet = mBDCBuffer.GetData();

        if ((vErr = vorbis_synthesis(&mV_vb, &op)) == 0)
            vorbis_synthesis_blockin(&mV_vd, &mV_vb);
        else {
            if (!gaap) {
                mBDCStatus = kBDCStatusAbort;
                ret = false;
            }
        }

        mBDCBuffer.Zap(sfp.bytes);
        sfp.left = sfp.frames = vorbis_synthesis_pcmout(&mV_vd, NULL);
        mConsumedFPList.push_back(sfp);
        mVorbisFPList.erase(mVorbisFPList.begin());

        if (ret != true)
            break;
    }*/

    return ret;
}

void CAOpusDecoder::OutputFrames(void* outOutputData, UInt32 inNumberFrames, UInt32 inFramesOffset,
                                   AudioStreamPacketDescription* /* outPacketDescription */) const
{
	/*
    float **pcm;
    vorbis_synthesis_pcmout(const_cast<vorbis_dsp_state*> (&mV_vd), &pcm);  // ignoring the result, but should be (!!) at least inNumberFrames

    if ((mOutputFormat.mFormatFlags & kAudioFormatFlagsNativeFloatPacked) != 0) {
        for (SInt32 i = 0; i < mV_vi.channels; i++) {
            float* theOutputData = static_cast<float*> (outOutputData) + i + (inFramesOffset * mV_vi.channels);
            float* mono = pcm[i];
            for (UInt32 j = 0; j < inNumberFrames; j++) {
                *theOutputData = mono[j];
                theOutputData += mV_vi.channels;
            }
        }
    } else {
        for (SInt32 i = 0; i < mV_vi.channels; i++) {
            SInt16* theOutputData = static_cast<SInt16*> (outOutputData) + i + (inFramesOffset * mV_vi.channels);
            float* mono = pcm[i];
            for (UInt32 j = 0; j < inNumberFrames; j++) {
                SInt32 val = static_cast<SInt32> (mono[j] * 32767.f);

                if (val > 32767)
                    val = 32767;
                if (val < -32768)
                    val = -32768;

                *theOutputData = val;
                theOutputData += mV_vi.channels;
            }
        }
    }*/
}

void CAOpusDecoder::Zap(UInt32 inFrames)
{
    //vorbis_synthesis_read(&mV_vd, inFrames);

    mFullInPacketsZapped = 0;
    UInt32 frames = 0;
    VorbisFramePacketList::iterator cp = mConsumedFPList.begin();
    while (frames < inFrames && cp != mConsumedFPList.end()) {
        if (frames + cp->left <= inFrames) {
            frames += cp->left;
            mFullInPacketsZapped++;
            cp = mConsumedFPList.erase(cp);
        } else {
            cp->left -= inFrames - frames;
            break;
        }
    }
}

UInt32 CAOpusDecoder::InPacketsConsumed() const
{
    return mFullInPacketsZapped;
}
