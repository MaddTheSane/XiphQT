/*
 *  OggImport.c
 *
 *    The main part of the OggImport component.
 *
 *
 *  Copyright (c) 2005  Arek Korbik
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
 *  This file is based on the original importer code
 *  written by Steve Nicolai.
 *
 */

#include <QuickTime/QuickTime.h>

#include <Ogg/ogg.h>
//#include <Vorbis/codec.h>

#include "OggImport.h"

//#define NDEBUG

#include "debug.h"


#include "importer_types.h"

#include "common.h"
#include "rb.h"

//stream-type support functions
#include "stream_vorbis.h"
#include "stream_speex.h"

static stream_format_handle_funcs s_formats[] = {
#if defined(_HAVE__VORBIS_SUPPORT)
    HANDLE_FUNCTIONS__VORBIS,
#endif
#if defined(_HAVE__SPEEX_SUPPORT)
    HANDLE_FUNCTIONS__SPEEX,
#endif
    
    HANDLE_FUNCTIONS__NULL
};




//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//			Constants
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

enum {
	kDataBufferSize = 64 * 1024,
	kDataAsyncBufferSize = 16 * 1024,
    
    kDefaultChunkSize = 11000
};


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//			prototypes
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

/* sometimes I wonder if using ComponentDispatch.h is worth it.  This is
   one of those times.  MacOS functions are declared pascal, win32 are not.
 */
#if TARGET_OS_MAC
#define COMPONENTFUNC static pascal ComponentResult
#else
#define COMPONENTFUNC static ComponentResult
#endif

COMPONENTFUNC OggImportOpen(OggImportGlobalsPtr globals, ComponentInstance self);
COMPONENTFUNC OggImportClose(OggImportGlobalsPtr globals, ComponentInstance self);
COMPONENTFUNC OggImportVersion(OggImportGlobalsPtr globals);

COMPONENTFUNC OggImportSetOffsetAndLimit64(OggImportGlobalsPtr globals, const wide *offset,
										   const wide *limit);
COMPONENTFUNC OggImportSetOffsetAndLimit(OggImportGlobalsPtr globals, unsigned long offset,
										 unsigned long limit);

COMPONENTFUNC OggImportValidate(OggImportGlobalsPtr globals, 
								const FSSpec *         theFile,
								Handle                 theData,
								Boolean *              valid);
COMPONENTFUNC OggImportValidateDataRef(OggImportGlobalsPtr globals, 
									   Handle                 dataRef,
									   OSType                 dataRefType,
									   UInt8 *                valid);

COMPONENTFUNC OggImportSetChunkSize(OggImportGlobalsPtr globals, long chunkSize);

COMPONENTFUNC OggImportIdle(OggImportGlobalsPtr globals,
							long                   inFlags,
							long *                 outFlags);

COMPONENTFUNC OggImportFile(OggImportGlobalsPtr globals, const FSSpec *theFile,
							Movie theMovie, Track targetTrack, Track *usedTrack,
							TimeValue atTime, TimeValue *durationAdded, long inFlags, long *outFlags);
COMPONENTFUNC OggImportGetMIMETypeList(OggImportGlobalsPtr globals, QTAtomContainer *retMimeInfo);
COMPONENTFUNC OggImportDataRef(OggImportGlobalsPtr globals, Handle dataRef,
							   OSType dataRefType, Movie theMovie,
							   Track targetTrack, Track *usedTrack,
							   TimeValue atTime, TimeValue *durationAdded,
							   long inFlags, long *outFlags);
COMPONENTFUNC OggImportGetFileType(OggImportGlobalsPtr globals, OSType *fileType);
COMPONENTFUNC OggImportGetLoadState(OggImportGlobalsPtr globals, long *loadState);
COMPONENTFUNC OggImportGetMaxLoadedTime(OggImportGlobalsPtr globals, TimeValue *time);
COMPONENTFUNC OggImportEstimateCompletionTime(OggImportGlobalsPtr globals, TimeRecord *time);
COMPONENTFUNC OggImportSetDontBlock(OggImportGlobalsPtr globals, Boolean  dontBlock);
COMPONENTFUNC OggImportGetDontBlock(OggImportGlobalsPtr globals, Boolean  *willBlock);
COMPONENTFUNC OggImportSetIdleManager(OggImportGlobalsPtr globals, IdleManager im);
COMPONENTFUNC OggImportSetNewMovieFlags(OggImportGlobalsPtr globals, long flags);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//			Component Dispatcher
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#define CALLCOMPONENT_BASENAME() 		OggImport
#define CALLCOMPONENT_GLOBALS() 		OggImportGlobalsPtr storage

#define MOVIEIMPORT_BASENAME() 			CALLCOMPONENT_BASENAME()
#define MOVIEIMPORT_GLOBALS() 			CALLCOMPONENT_GLOBALS()

#define COMPONENT_UPP_SELECT_ROOT()		MovieImport
#define COMPONENT_DISPATCH_FILE			"OggImportDispatch.h"

#include <CoreServices/Components.k.h>
#include <QuickTime/QuickTimeComponents.k.h>
#include <QuickTime/ComponentDispatchHelper.c>


static ComponentResult DoRead(OggImportGlobalsPtr globals, Ptr buffer, SInt64 offset, long size)
{
    ComponentResult 	err;
    
	dprintf("---- DoRead() called\n");
    const wide wideOffset = SInt64ToWide(offset);
    
	dprintf("--->> READING: %lld [%ld] --> %lld\n", offset, size, offset + size);
	dprintf("----> READING: usingIdle: %d, dataCanDoAsyncRead: %d, canDoGetFileSizeAsync: %d, canDoGetFileSize64: %d\n",
			globals->usingIdle, globals->dataCanDoAsyncRead, globals->dataCanDoGetFileSizeAsync, globals->dataCanDoGetFileSize64);

    if (globals->usingIdle && globals->dataCanDoAsyncRead)
    {
		globals->dataRequested = true;
        err = DataHReadAsync(globals->dataReader, buffer, size, &wideOffset, 
							 globals->dataReadCompletion, (long) globals);
		dprintf("----: READ: %ld\n", err);
		err = QTIdleManagerSetNextIdleTimeNever(globals->idleManager);
		dprintf("----: Disabling Idles: %ld\n", err);
    }
    else
    {
		err = DataHScheduleData64(globals->dataReader, buffer, &wideOffset,
								  size, 0, NULL, NULL);
		if (err == noErr)
			rb_sync_reserved(&globals->dataRB);
    }
    globals->readError = err;
	
    return err;
}

static ComponentResult FillBuffer(OggImportGlobalsPtr globals)
{
    int	   dataLeft;
    SInt64 readDataOffset;

	dprintf("---- FillBuffer() called\n");
	dprintf("   - dataOffset: %lld, dataEndOffset: %lld\n", globals->dataOffset, globals->dataEndOffset);
	dprintf("   - dataOffset != -1: %d, dataOffset >= dataEndOffset: %d\n",
		   S64Compare(globals->dataOffset, S64Set(-1)) != 0,
		   S64Compare(globals->dataOffset, globals->dataEndOffset) >= 0);

    /* have we hit the end of file or our upper limit? */
	if (globals->sizeInitialised && S64Compare(globals->dataEndOffset, S64Set(-1)) != 0) {
		if (S64Compare(globals->dataOffset, S64Set(-1)) != 0 &&
			S64Compare(globals->dataOffset, globals->dataEndOffset) >= 0)
			return eofErr;
	}
        
	dprintf("--1- FillBuffer() called\n");
    /* can another page from the disk fit in the buffer? */
    if (globals->dataReadChunkSize > rb_space_available(&globals->dataRB))
        return -50;  ///@@@ page won't fit in buffer, they always should

	dprintf("--2- FillBuffer() called\n");
    readDataOffset = S64Add(globals->dataOffset, S64Set(rb_data_available(&globals->dataRB)));
    
    /* figure out how much data is left, and read either a chunk or what's left */
	if (globals->sizeInitialised && S64Compare(globals->dataEndOffset, S64Set(-1)) != 0) {
		dataLeft = S32Set(S64Subtract(globals->dataEndOffset, readDataOffset));
	} else {
        dataLeft = globals->dataReadChunkSize;
	}

    if (dataLeft > globals->dataReadChunkSize)
        dataLeft = globals->dataReadChunkSize;
    
    if (dataLeft == 0)
		return eofErr;

    return DoRead(globals, (Ptr) rb_reserve(&globals->dataRB, dataLeft), readDataOffset, dataLeft);
}

static OSErr CheckVorbisHeader(ogg_page *opg)
{
    OSErr			 err = noErr;

	ogg_stream_state os;
	ogg_packet       op;

	vorbis_info      vi;
	vorbis_comment   vc;

	ogg_stream_init(&os, ogg_page_serialno(opg));

    vorbis_info_init(&vi);
    vorbis_comment_init(&vc);

	if (ogg_stream_pagein(&os, opg) < 0)
		err = invalidMedia;
	else if (ogg_stream_packetout(&os, &op) != 1)
		err = invalidMedia;
	else if (vorbis_synthesis_headerin(&vi, &vc, &op) < 0)
		err = noSoundTrackInMovieErr;
	
	ogg_stream_clear(&os);

    vorbis_comment_clear(&vc);
    vorbis_info_clear(&vi);

    return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static ComponentResult OpenStream(OggImportGlobalsPtr globals, long serialno, ogg_page *opg, stream_format_handle_funcs *ff)
{
    ComponentResult err;
    
	if (globals->streamInfoHandle)
	{
		globals->streamCount++;
		SetHandleSize((Handle)globals->streamInfoHandle, sizeof(StreamInfo) * globals->streamCount);
	}
	else
	{
		globals->streamInfoHandle = (StreamInfo **)NewHandle(sizeof(StreamInfo));
		globals->streamCount = 1;
	}
	err = MemError();
    
	if (err == noErr)
	{
		StreamInfo *si = &(*globals->streamInfoHandle)[globals->streamCount - 1];
		si->serialno = serialno;
		si->timeLoaded = 0;

		ogg_stream_init(&si->os,serialno);

        si->sfhf = ff;

        if (ff->initialize != NULL)
            (*ff->initialize)(si); // check for error here and clean-up if not OK
		
		si->startTime = globals->startTime;
		si->soundDescExtension = NewHandle(0);
		
		si->MDmapping = NULL;
		si->UDmapping = NULL;

		globals->numTracksStarted++;
	}
	
    return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static StreamInfoPtr FindStream(OggImportGlobalsPtr globals, long serialno)
{
    int i;
    
    for (i = 0; i < globals->streamCount; i++)
    {
        if ((*globals->streamInfoHandle)[i].serialno == serialno)
        {
            return &(*globals->streamInfoHandle)[i];
        }
    }
    
    return NULL;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void CloseStream(OggImportGlobalsPtr globals, StreamInfoPtr si)
{
	ogg_stream_clear(&si->os);

    if (si->sfhf->clear != NULL)
        (*si->sfhf->clear)(si);

	if (si->MDmapping != NULL)
		CFRelease(si->MDmapping);
	if (si->UDmapping != NULL)
		CFRelease(si->UDmapping);
	
	DisposeHandle(si->soundDescExtension);
	DisposeHandle((Handle)si->sampleDesc);
	
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void CloseAllStreams(OggImportGlobalsPtr globals)
{
    int i;
    
    for (i = 0; i < globals->streamCount; i++)
    {
		StreamInfoPtr si = &(*globals->streamInfoHandle)[i];
		CloseStream(globals, si);
    }
	globals->streamCount = 0;
	SetHandleSize((Handle) globals->streamInfoHandle, 0);
}


static int InitialiseMetaDataMappings(StreamInfoPtr si) {
	CFBundleRef bundle;
	CFURLRef mdmurl;
	CFDataRef data;
	SInt32 ret = 0;
	CFStringRef errorString;
	SInt32 error = 0;
	CFDictionaryRef props;

	dprintf("--= IMDM()\n");
	if (si->MDmapping != NULL && si->UDmapping != NULL) {
		return 1;
	}
	
	//else? let's assume for now that they are both intialised or both are not initialised

	bundle = CFBundleGetBundleWithIdentifier(CFSTR(kOggVorbisBundleID));
	
	if (bundle == NULL)
		return 0;

	mdmurl = CFBundleCopyResourceURL(bundle, CFSTR("MetaDataConfig"), CFSTR("plist"), NULL);
	if (mdmurl != NULL) {
		if (CFURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault, mdmurl, &data, 
													 NULL, NULL, &error)) {
			props = (CFDictionaryRef) CFPropertyListCreateFromXMLData(kCFAllocatorDefault, data,
																	  kCFPropertyListImmutable, &errorString);
			if (props != NULL) {
				if (CFGetTypeID(props) == CFDictionaryGetTypeID()) {
					si->MDmapping = (CFDictionaryRef) CFDictionaryGetValue(props, CFSTR("Vorbis-to-MD"));
					if (si->MDmapping != NULL) {
						dprintf("----: MDmapping found\n");
						CFRetain(si->MDmapping);
						ret = 1;
					}
					si->UDmapping = (CFDictionaryRef) CFDictionaryGetValue(props, CFSTR("Vorbis-to-UD"));
					if (si->UDmapping != NULL) {
						dprintf("----: UDmapping found\n");
						CFRetain(si->UDmapping);
					} else
						ret = 0;
				}
				CFRelease(props);
			}
			CFRelease(data);
		}
		CFRelease(mdmurl);
	}

	return ret;
}

static int LookupTagUD(StreamInfoPtr si, const char *str, long *osType) {
	int ret = -1;
	long len;

	if (si->UDmapping == NULL)
    {
		if (!InitialiseMetaDataMappings(si))
			return -1;
    }
    
	len = strcspn(str, "=");

	if (len > 0) {
		CFStringRef tmpkstr = CFStringCreateWithBytes(NULL, str, len + 1, kCFStringEncodingUTF8, true);
		if (tmpkstr != NULL) {
			CFMutableStringRef keystr = CFStringCreateMutableCopy(NULL, len + 1, tmpkstr);
			if (keystr != NULL) {
				CFLocaleRef loc = CFLocaleCopyCurrent();
				CFStringUppercase(keystr, loc);
				CFRelease(loc);
				dprintf("--- luTud: %s [%s]\n", (char *)str, (char *)CFStringGetCStringPtr(keystr,kCFStringEncodingUTF8));
				if (CFDictionaryContainsKey(si->UDmapping, keystr)) {
					CFStringRef udkey = (CFStringRef) CFDictionaryGetValue(si->UDmapping, keystr);

					*osType = (CFStringGetCharacterAtIndex(udkey, 0) & 0xff) << 24 |
						(CFStringGetCharacterAtIndex(udkey, 1) & 0xff) << 16 |
						(CFStringGetCharacterAtIndex(udkey, 2) & 0xff) << 8 |
						(CFStringGetCharacterAtIndex(udkey, 3) & 0xff);

					dprintf("--- luTud: %s [%s]\n", (char *)str, (char *)keystr);
					ret = len + 1;
				}
				CFRelease(keystr);
			}
			CFRelease(tmpkstr);
		}
	}

	return ret;
}

static int LookupTagMD(StreamInfoPtr si, const char *str, long *osType) {
	int ret = -1;
	long len;

	if (si->MDmapping == NULL)
    {
		if (!InitialiseMetaDataMappings(si))
			return -1;
    }
    
	len = strcspn(str, "=");

	if (len > 0) {
		CFStringRef tmpkstr = CFStringCreateWithBytes(NULL, str, len + 1, kCFStringEncodingUTF8, true);
		if (tmpkstr != NULL) {
			CFMutableStringRef keystr = CFStringCreateMutableCopy(NULL, len + 1, tmpkstr);
			if (keystr != NULL) {
				CFLocaleRef loc = CFLocaleCopyCurrent();
				CFStringUppercase(keystr, loc);
				CFRelease(loc);
				dprintf("--- luTmd: %s [%s]\n", (char *)str, (char *)CFStringGetCStringPtr(keystr,kCFStringEncodingUTF8));
				if (CFDictionaryContainsKey(si->MDmapping, keystr)) {
					CFStringRef mdkey = (CFStringRef) CFDictionaryGetValue(si->MDmapping, keystr);

					*osType = (CFStringGetCharacterAtIndex(mdkey, 0) & 0xff) << 24 |
						(CFStringGetCharacterAtIndex(mdkey, 1) & 0xff) << 16 |
						(CFStringGetCharacterAtIndex(mdkey, 2) & 0xff) << 8 |
						(CFStringGetCharacterAtIndex(mdkey, 3) & 0xff);

					dprintf("--- luTmd: %s [%s]\n", (char *)str, (char *)keystr);
					ret = len + 1;
				}
				CFRelease(keystr);
			}
			CFRelease(tmpkstr);
		}
	}

	return ret;
}

static ComponentResult ConvertUTF8toScriptCode(const char *str, Handle *h, ScriptCode *script)
{
	TextEncoding    sourceEncoding = CreateTextEncoding(kTextEncodingUnicodeV3_0, kUnicodeNoSubset, kUnicodeUTF8Format);
	TextEncoding    destEncoding = CreateTextEncoding(kTextEncodingMacRoman, kTextEncodingDefaultVariant, kTextEncodingDefaultFormat);
	TECObjectRef	converter;
    Handle          temp;
    int             length = strlen(str);
    int             tempLen = length * 2;
    
    OSErr err = TECCreateConverter(&converter, sourceEncoding, destEncoding);
    if (err != noErr)
        return err;

    *script = 0;
    *h = NULL;

	temp = NewHandle(tempLen);
	if (temp == NULL)
		return memFullErr;

	while (1)
	{
        ByteCount actualIn, actualOut, flushOut;

		HLock(temp);
		actualIn = 0;
        flushOut = 0;
		
		err = TECConvertText(converter, (ConstTextPtr)str, length, &actualIn, (TextPtr)*temp, tempLen, &actualOut);
		if (length == actualIn || actualOut < tempLen - 20)
		{
            if (err == noErr)
                err = TECFlushText(converter, (TextPtr) ((*temp) + actualOut), tempLen - actualOut, &flushOut);
			HUnlock(temp);
            SetHandleSize(temp, actualOut + flushOut);
            *h = temp;
            temp = NULL;
            err = noErr;    // ignore and supress conversion errors
			break;
		}
		else
		{
			HUnlock(temp);
			tempLen += length;
			SetHandleSize(temp, tempLen);
			err = MemError();
			if (err != noErr)
				break;	
		}
	}
	
	if (temp != NULL)
		DisposeHandle(temp);

    TECDisposeConverter(converter);

    return err;
}

static ComponentResult AddCommentToMetaData(StreamInfoPtr si, const char *str, int len, QTMetaDataRef md) {
	ComponentResult ret = noErr;
	long tag;

    int	tagLen = LookupTagMD(si, str, &tag);
	Handle h;
	ScriptCode script;
        
	
    if (tagLen != -1 && str[tagLen] != '\0') {
		dprintf("-- TAG: %08lx\n", tag);

		ret = QTMetaDataAddItem(md, kQTMetaDataStorageFormatQuickTime, kQTMetaDataKeyFormatCommon,
								&tag, sizeof(tag), str + tagLen, len - tagLen, kQTMetaDataTypeUTF8, NULL);
		dprintf("-- TAG: %4.4s :: QT    = %ld\n", (char *)&tag, (long)ret);
	}
	
    tagLen = LookupTagUD(si, str, &tag);
	
    if (tagLen != -1 && str[tagLen] != '\0') {
		QTMetaDataItem mdi;
		char * localestr = "en";
		Handle localeh = NewEmptyHandle();
		
		PtrAndHand(&localestr, localeh, strlen(localestr));

		dprintf("-- TAG: %08lx\n", tag);

		ret = ConvertUTF8toScriptCode(str + tagLen, &h, &script);
		if (ret == noErr) {
			HLock(h);
			ret = QTMetaDataAddItem(md, kQTMetaDataStorageFormatUserData, kQTMetaDataKeyFormatUserData,
									&tag, sizeof(tag), *h, GetHandleSize(h), kQTMetaDataTypeMacEncodedText, &mdi);
			dprintf("-- TAG: %4.4s :: QT[X] = %ld\n", (char *)&tag, (long)ret);
			HUnlock(h);
			if (ret == noErr) {
				ret = QTMetaDataSetItemProperty(md, mdi, kPropertyClass_MetaDataItem, kQTMetaDataItemPropertyID_Locale,
												GetHandleSize(localeh), *h);
				dprintf("-- TAG: %4.4s :: QT[X] locale (%5.5s)= %ld\n", (char *)&tag, *h, (long)ret);
			}
			DisposeHandle(h);
		}
	}

	return ret;
}

ComponentResult DecodeCommentsQT(OggImportGlobalsPtr globals, StreamInfoPtr si, vorbis_comment *vc)
{
    ComponentResult ret = noErr;
    int				i;
    QTMetaDataRef	md;
	
	//ret = QTCopyTrackMetaData(si->theTrack, &md);
	ret = QTCopyMovieMetaData(globals->theMovie, &md);

	if (ret != noErr)
		return ret;
	
    for (i = 0; i < vc->comments; i++)
    {
        ret = AddCommentToMetaData(si, vc->user_comments[i], vc->comment_lengths[i], md);
        if (ret != noErr) {
            //break;
			dprintf("AddCommentToMetaData() failed? = %d\n", ret);
		}
    }
    
	QTMetaDataRelease(md);

	ret = QTCopyTrackMetaData(si->theTrack, &md);
	//ret = QTCopyMovieMetaData(globals->theMovie, &md);

	if (ret != noErr)
		return ret;
	
    for (i = 0; i < vc->comments; i++)
    {
        ret = AddCommentToMetaData(si, vc->user_comments[i], vc->comment_lengths[i], md);
        if (ret != noErr) {
            //break;
			dprintf("AddCommentToMetaData() failed? = %d\n", ret);
		}
    }
    
	QTMetaDataRelease(md);

    return ret;
}

static ComponentResult CreateSampleDescription(StreamInfoPtr si)
{
    ComponentResult err = noErr;
    if (si->sfhf->sample_description != NULL)
        err = (*si->sfhf->sample_description)(si);
    else
        err = invalidMedia; // ??!
	
    return err;
}

ComponentResult CreateTrackAndMedia(OggImportGlobalsPtr globals, StreamInfoPtr si, ogg_page *opg)
{
    ComponentResult err = noErr;

    if (err == noErr)
    {
        // build the sample description
        err = CreateSampleDescription(si);
        if (err == noErr)
        {
            dprintf("! -- SampleDescription created OK\n");
            si->theTrack = NewMovieTrack(globals->theMovie, 0, 0, kFullVolume);
            if (si->theTrack)
            {
                dprintf("! -- MovieTrack created OK\n");
                dprintf("! -- calling => NewTrackMedia(%lx)\n", si->rate);
                si->theMedia = NewTrackMedia(si->theTrack, SoundMediaType,
                        si->rate, globals->dataRef, globals->dataRefType);
                if (si->theMedia)
                {
                    dprintf("! -- TrackMedia created OK\n");
                    SetTrackEnabled(si->theTrack, true);
        
                    si->lastGranulePos = 0;
                }
                else
                {
                    err = GetMoviesError();
                    DisposeMovieTrack(si->theTrack);
                }
            } else
                err = GetMoviesError();
        }
    }
    
    return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ComponentResult NotifyMovieChanged(OggImportGlobalsPtr globals)
{
    //Notify the movie it's changed (from email from Chris Flick)
    QTAtomContainer container = NULL;
    OSErr err = QTNewAtomContainer (&container);

    if (err == noErr)
    {
        QTAtom anAction;
        OSType whichAction = EndianU32_NtoB (kActionMovieChanged);
    
        err = QTInsertChild (container, kParentAtomIsContainer, kAction, 1, 0, 0, NULL, &anAction);
        if (err == noErr) 
            err = QTInsertChild (container, anAction, kWhichAction, 1, 0, sizeof (whichAction), &whichAction, NULL);
        if (err == noErr) 
            err = MovieExecuteWiredActions (globals->theMovie, 0, container);
    
        err = QTDisposeAtomContainer (container);
    }
    return err;
}

#ifndef NDEBUG
int logg_page_last_packet_incomplete(ogg_page *op)
{
    unsigned char *header = op->header;
    return (header[26 + header[26]] == 255);
}
#endif /* NDEBUG */

static ComponentResult ProcessStreamPage(OggImportGlobalsPtr globals, StreamInfoPtr si, ogg_page *opg) {
	ComponentResult ret = noErr;

    if (si->sfhf->process_page != NULL)
        ret = (*si->sfhf->process_page)(globals, si, opg);
    else {
        // shouldn't happen, but just skip a page here
        ogg_packet op;
        ogg_stream_pagein(&si->os, opg);
        while (ogg_stream_packetout(&si->os, &op) > 0)
            ; // do nothing, just loop
    }

	return ret;
}

static stream_format_handle_funcs* find_stream_support(ogg_page *op) {

    stream_format_handle_funcs *ff = &s_formats[0];
    int i = 0;

    while(ff->recognize != NULL) {
        if ((*ff->recognize)(op) == 0 && (ff->verify == NULL || (*ff->verify)(op) == 0))
            break;
        i += 1;
        ff = &s_formats[i];
    }

    if (ff->recognize == NULL)
        return NULL;

    return ff;
}

static ComponentResult ProcessPage(OggImportGlobalsPtr globals, ogg_page *op) {
	ComponentResult ret = noErr;
	long serialno;
	StreamInfoPtr si;

	serialno = ogg_page_serialno(op);

	dprintf("   - = page found, nr: %08lx\n", ogg_page_pageno(op));
	if (ogg_page_bos(op)) {
		dprintf("   - = new stream found: %lx\n" , serialno);
		stream_format_handle_funcs *ff = find_stream_support(op);
		if (ff != NULL) {
			dprintf("   - == And a supported one!\n");
			ret = OpenStream(globals, serialno, op, ff);
			
			if (ret == noErr) {
				ogg_packet       opckt;
				StreamInfoPtr si = FindStream(globals, serialno);

				if (si != NULL) {
					ogg_stream_pagein(&si->os, op); //check errors?
					ogg_stream_packetout(&si->os, &opckt); //check errors?

                    if (si->sfhf->first_packet != NULL)
                        (*si->sfhf->first_packet)(si, op, &opckt); //check errors?
                }
			}
		}
	} else {
		si = FindStream(globals, serialno);
		
		if (si != NULL) {
			ret = ProcessStreamPage(globals, si, op);
		}
	}

	globals->dataOffset = S64Add(globals->dataOffset, S64Set(globals->currentData - globals->dataRB.b_start));
	rb_zap(&globals->dataRB, globals->currentData - globals->dataRB.b_start);

	globals->currentData = rb_data(&globals->dataRB);
	globals->validDataEnd = globals->currentData + rb_data_available(&globals->dataRB);

	return ret;
}

static ComponentResult GetFileSize(OggImportGlobalsPtr globals);

static ComponentResult StateProcess(OggImportGlobalsPtr globals) {
    ComponentResult result = noErr;
    ogg_page og;
	Boolean process = true;

	dprintf("-----= StateProcess() called\n");
	while (process) {
		switch (globals->state) {
			case kStateInitial:
				dprintf("   - (:kStateInitial:)\n");
				globals->dataOffset = globals->dataStartOffset;
				globals->numTracksSeen = 0;
				globals->timeLoaded = 0;
				globals->dataRequested = false;
				globals->startTickCount = TickCount();
				
				if (S64Compare(globals->dataEndOffset, S64Set(-1)) == 0) {
					globals->sizeInitialised = false;
					globals->state = kStateGettingSize;
					result = GetFileSize(globals);
					if (!globals->sizeInitialised)
						process = false;
				} else
					globals->state = kStateReadingPages;
				
				break;
				
			case kStateGettingSize:
				dprintf("   - (:kStateGettingSize:)\n");
				if (!globals->sizeInitialised) {
					process = false;
					break;
				}
				
				globals->state = kStateReadingPages;
				break;
				
			case kStateReadingPages:
				dprintf("   - (:kStateReadingPages:)\n");
				if (globals->dataRequested) {
					DataHTask(globals->dataReader);
					process = false;
					break;
				}
				globals->currentData = rb_data(&globals->dataRB);
				globals->validDataEnd = globals->currentData + rb_data_available(&globals->dataRB);
				
				while (result == noErr && FindPage(&globals->currentData, globals->validDataEnd, &og)) {
					result = ProcessPage(globals, &og);
				}
				
				if (result != noErr)
					break;

				result = FillBuffer(globals);
				if (result == eofErr)
					globals->state = kStateReadingLastPages;

				break;
				
			case kStateReadingLastPages:
				dprintf("   - (:kStateReadingLastPages:)\n");
				globals->currentData = rb_data(&globals->dataRB);
				globals->validDataEnd = globals->currentData + rb_data_available(&globals->dataRB);
				
				dprintf("   + (:kStateReadingLastPages:)\n");
				while (FindPage(&globals->currentData, globals->validDataEnd, &og)) {
					result = ProcessPage(globals, &og);
					dprintf("  <- (:kStateReadingLastPages:) = %ld\n", (long)result);
				}
					
					globals->state = kStateImportComplete;
				break;
				
			case kStateImportComplete:
				dprintf("   - (:kStateImportComplete:)\n");
				process = false;
				break;
		}
	}
	
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void ReadCompletion(Ptr request, long refcon, OSErr readErr)
{
    OggImportGlobalsPtr globals = (OggImportGlobalsPtr) refcon;
    ComponentResult		 result = readErr;
    
	dprintf("---> ReadCompletion() called\n");
    if (readErr == noErr)
    {
		dprintf("--1- ReadCompletion() :: noErr\n");

		rb_sync_reserved(&globals->dataRB);
		globals->dataRequested = false;

		if (globals->idleManager != NULL) {
			dprintf("--2- ReadCompletion() :: requesting Idle\n");
			QTIdleManagerSetNextIdleTimeNow(globals->idleManager);
		}
	}
    
    if (result != noErr)
    {
		dprintf("--3- ReadCompletion() :: !noErr - %ld (%lx), eofErr: %d\n", result, result, result == eofErr);

        if (result == eofErr) {
            result = noErr;
			globals->dataRequested = false;
			globals->state = kStateImportComplete;
		}

        globals->errEncountered = result;
        
        /* close off every open stream */
		if (globals->streamCount > 0)
			CloseAllStreams(globals);
    }

	dprintf("---< ReadCompletion()\n");
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void FileSizeCompletion(Ptr request, long refcon, OSErr readErr)
{
    OggImportGlobalsPtr globals = (OggImportGlobalsPtr) refcon;

	dprintf("----- FileSizeCompletion() called = %ld\n", (long) readErr);
    if (readErr == noErr)
    {
        globals->dataEndOffset = WideToSInt64(globals->wideTempforFileSize);
		globals->sizeInitialised = true;
		QTIdleManagerSetNextIdleTimeNow(globals->idleManager);
    }
}

static ComponentResult GetFileSize(OggImportGlobalsPtr globals)
{
    ComponentResult 	err = badComponentSelector;
    wide                size;
    
	dprintf("---> GetFileSize() called\n");
	if (globals->usingIdle && globals->dataCanDoGetFileSizeAsync && false) {
        err = DataHGetFileSizeAsync(globals->dataReader, &globals->wideTempforFileSize, 
                        globals->fileSizeCompletion, (long) globals);
		dprintf("---- :: async size, err: %ld (%lx)\n", (long)err, (long)err);		

    } else if (globals->dataCanDoGetFileSize64) {
        err = DataHGetFileSize64(globals->dataReader, &size);
		dprintf("---- :: size: %ld%ld, err: %ld (%lx)\n", size.hi, size.lo, (long)err, (long)err);
        globals->readError = err;
        if (err == noErr) {
            globals->dataEndOffset = WideToSInt64(size);
			globals->sizeInitialised = true;
		}
    } else {
		globals->dataEndOffset = S64Set(-1);
		globals->sizeInitialised = true;
		err = noErr;
	}
    
	dprintf("---< GetFileSize() = %ld (%lx)\n", (long) err, (long) err);
    return err;
}

static ComponentResult StartImport(OggImportGlobalsPtr globals, Handle dataRef, OSType dataRefType)
{
    ComponentResult err = noErr;
    
    globals->state = kStateInitial;
    
    return err;
}

static ComponentResult JustImport(OggImportGlobalsPtr globals, Handle dataRef, OSType dataRefType) {
    ComponentResult	ret = noErr;
	Boolean	do_read = true;
    
    globals->state = kStateInitial;
    
    /* if limits have not been set, then try to get the size of the file. */
    if (S64Compare(globals->dataEndOffset, S64Set(-1)) == 0) {
        ret = GetFileSize(globals);
    }

	if (ret != noErr)
		return ret;

	while (do_read) {
		ret = StateProcess(globals);
		if ((ret != noErr && ret != eofErr) || globals->state == kStateImportComplete)
			do_read = false;
	}

	if (ret == eofErr)
		ret = noErr;

	dprintf("-<<- JustImport(): %ld\n", (long)ret);
    return ret;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static ComponentResult SetupDataHandler(OggImportGlobalsPtr globals, Handle dataRef, OSType dataRefType)
{
    ComponentResult err = noErr;
    
	dprintf("---> SetupDataHandler(type: '%4.4s') called\n", &dataRefType);
    if (globals->dataReader == NULL)
    {
		Component dataHComponent = NULL;

#if 0
		if (dataRefType == URLDataHandlerSubType)
		{
			ComponentDescription cdesc, cd;
			int count;
			Handle cname = NewHandle(0);
			
			cdesc.componentType = DataHandlerType;
			cdesc.componentSubType = URLDataHandlerSubType;
			cdesc.componentManufacturer = kAnyComponentManufacturer;
			cdesc.componentFlags = kAnyComponentFlagsMask;
			cdesc.componentFlagsMask = kAnyComponentFlagsMask;
			
			dprintf("---- >> CountComponents(urlDataHandlers): %ld\n", CountComponents(&cdesc));
			count = 6;
			while (count-- > 0) {
				dataHComponent = FindNextComponent(dataHComponent, &cdesc);
				GetComponentInfo(dataHComponent, &cd, cname, NULL, NULL);
				dprintf("---- ->-> component desc: %s, manu: %4.4s\n", *cname, &cd.componentManufacturer);
			}
			
		} else {
#endif
			dataHComponent = GetDataHandler(dataRef, dataRefType, kDataHCanRead);
#if 0
		}
#endif

        err = OpenAComponent(dataHComponent, &globals->dataReader);
    
		dprintf("---- >> OpenAComponent() = %ld\n", (long)err);
        if (err == noErr)
        {
            err = DataHSetDataRef(globals->dataReader, dataRef);
			dprintf("---- >> DataHSetDataRef() = %ld\n", (long)err);
            if (err == noErr)
                err = DataHOpenForRead(globals->dataReader);
#if 0
			else {
				Boolean wc;
				err = DataHResolveDataRef(globals->dataReader, dataRef, &wc, false);
				dprintf("---- >> DataHResolveDataRef() = %ld\n", (long)err);
				err = noErr;
			}
#endif
			DataHPlaybackHints(globals->dataReader, 0, 0, -1, 49152);  // Don't care if it fails
            
            if (err == noErr)
            {
                long	blockSize = 1024;
                
                globals->dataOffset = S64Set(0);
                
                globals->dataRef = dataRef;
                globals->dataRefType = dataRefType;
    
                globals->dataCanDoAsyncRead = (CallComponentCanDo(globals->dataReader, kDataHReadAsyncSelect) == true);
                globals->dataCanDoGetFileSizeAsync = (CallComponentCanDo(globals->dataReader, kDataHGetFileSizeAsyncSelect) == true);
                globals->dataCanDoGetFileSize64 = (CallComponentCanDo(globals->dataReader, kDataHGetFileSize64Select) == true);
    
                globals->dataReadChunkSize = kDataBufferSize;
				if ((globals->newMovieFlags & newMovieAsyncOK) != 0 && globals->dataCanDoGetFileSizeAsync)
					globals->dataReadChunkSize = kDataAsyncBufferSize;
                err = DataHGetPreferredBlockSize(globals->dataReader, &blockSize);
                if (err == noErr && blockSize < globals->dataReadChunkSize && blockSize > 1024)
                    globals->dataReadChunkSize = blockSize;
				dprintf("     - allocating buffer, size: %d (prefBlockSize: %ld); ret = %ld\n",
						globals->dataReadChunkSize, blockSize, (long)err);
                err = noErr;	/* ignore any error and use our default read block size */
    
				err = rb_init(&globals->dataRB, 2 * globals->dataReadChunkSize); //hmm why was it x2 ?

                globals->currentData = (unsigned char *)globals->dataRB.buffer;
                globals->validDataEnd = (unsigned char *)globals->dataRB.buffer;
            }
            
            if (err == noErr)
            {
                globals->dataReadCompletion = NewDataHCompletionUPP(ReadCompletion);
            }
            
            if (err == noErr && globals->dataCanDoGetFileSizeAsync)
            {
                globals->fileSizeCompletion = NewDataHCompletionUPP(FileSizeCompletion);
            }

            if (err == noErr && globals->idleManager)
            {
            // purposely ignore the error message here, i.e. set it if the data handler supports it
                OggImportSetIdleManager(globals, globals->idleManager);
            }

            if (err == noErr)
            {
            // This logic is similar to the MP3 importer
                UInt32  flags = 0;
                
                globals->dataIsStream = globals->dataCanDoGetFileSizeAsync;

                err = DataHGetInfoFlags(globals->dataReader, &flags);
                if (err == noErr && (flags & kDataHInfoFlagNeverStreams))
                    globals->dataIsStream = false;
                err = noErr;
				dprintf("---- -:: InfoFlags: NeverStreams: %d, CanUpdate...: %d, NeedsNet: %d\n",
						(flags & kDataHInfoFlagNeverStreams) != 0,
						(flags & kDataHInfoFlagCanUpdateDataRefs) != 0,
						(flags & kDataHInfoFlagNeedsNetworkBandwidth) != 0);
            }
        }
    }
    
	dprintf("---< SetupDataHandler() = %ld\n", (long)err);
	return err;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//			Component Routines
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
COMPONENTFUNC OggImportOpen(OggImportGlobalsPtr globals, ComponentInstance self)
{
    OSErr						result;
    
	dprintf("-- Open() called\n");
    globals = (OggImportGlobalsPtr)NewPtrClear(sizeof(OggImportGlobals));
    if (globals != nil)
    {	
        // set our storage pointer to our globals
        SetComponentInstanceStorage(self, (Handle) globals);
        globals->self = self;
        
        globals->dataEndOffset = S64Set(-1);
		globals->idleManager = NULL;
		globals->dataIdleManager = NULL;
    
        result = noErr;
    }
    else
        result = MemError();
    
    return (result);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
COMPONENTFUNC OggImportClose(OggImportGlobalsPtr globals, ComponentInstance self)
{
    ComponentResult		result;
    (void)self;
    
	dprintf("-- Close() called\n");
    if (globals != nil)											// we have some globals
    {
        if (globals->streamInfoHandle)
        {
			if (globals->streamCount > 0)
				CloseAllStreams(globals);
            DisposeHandle((Handle)globals->streamInfoHandle);
        }
        
        if (globals->dataReader)
        {
            result = CloseComponent(globals->dataReader);
            //FailMessage(result != noErr);		//@@@
            globals->dataReader = NULL;
        }
        
        if (globals->dataBuffer)
        {
            DisposePtr(globals->dataBuffer);
            globals->dataBuffer = NULL;
        }
		
		if (globals->dataRB.buffer) {
			rb_free(&globals->dataRB);
		}
        
        if (globals->dataReadCompletion)
            DisposeDataHCompletionUPP(globals->dataReadCompletion);
        
        if (globals->fileSizeCompletion)
            DisposeDataHCompletionUPP(globals->fileSizeCompletion);
        
        if (globals->aliasHandle)
            DisposeHandle((Handle)globals->aliasHandle);
            
		if (globals->dataIdleManager != NULL)
			QTIdleManagerClose(globals->dataIdleManager);
        
        DisposePtr((Ptr)globals);
    }
    
    return noErr;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
COMPONENTFUNC OggImportVersion(OggImportGlobalsPtr globals)
{
	dprintf("-- Version() called\n");
    return kOgg_eat__Version;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
COMPONENTFUNC OggImportSetOffsetAndLimit64(OggImportGlobalsPtr globals, const wide *offset,
		const wide *limit)
{
	dprintf("-- SetOffsetAndLimit64(%ld%ld, %ld%ld) called\n", offset->hi, offset->lo, limit->hi, limit->lo);
    globals->dataStartOffset = WideToSInt64(*offset);
    globals->dataEndOffset  = WideToSInt64(*limit);
    
    return noErr;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
COMPONENTFUNC OggImportSetOffsetAndLimit(OggImportGlobalsPtr globals, unsigned long offset,
		unsigned long limit)
{
	dprintf("-- SetOffsetAndLimit(%ld, %ld) called\n", offset, limit);
    globals->dataStartOffset = S64SetU(offset);	
    globals->dataEndOffset = S64SetU(limit);
    
    return noErr;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
COMPONENTFUNC OggImportValidate(OggImportGlobalsPtr globals, 
  const FSSpec *         theFile,
  Handle                 theData,
  Boolean *              valid)
{
    ComponentResult err = noErr;
	UInt8 extvalid = 0;

	dprintf("-- Validate() called\n");
    if (theFile == NULL)
    {
        Handle	dataHandle = NewHandle(sizeof(HandleDataRefRecord));
        if (dataHandle != NULL)
        {
            (*(HandleDataRefRecord **)dataHandle)->dataHndl = theData;
            err = MovieImportValidateDataRef(globals->self,
                                        dataHandle,
                                        HandleDataHandlerSubType,
                                        &extvalid);
            DisposeHandle(dataHandle);
        }
    }
    else
    {
        AliasHandle alias = NULL;
    
        err = NewAliasMinimal(theFile, &alias);
        if (err == noErr)
        {
            err = MovieImportValidateDataRef(globals->self,
                                    (Handle)alias,
                                    rAliasType,
                                    &extvalid);
    
            DisposeHandle((Handle)alias);
        }
    }
    
	if (extvalid > 0)
		*valid = true;
	else
		*valid = false;

    return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
COMPONENTFUNC OggImportValidateDataRef(OggImportGlobalsPtr globals, 
  Handle                 dataRef,
  OSType                 dataRefType,
  UInt8 *                valid)
{
    ComponentResult err = noErr;
	dprintf("-- ValidateDataRef() called\n");
    
	*valid = 128;

    return err;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
COMPONENTFUNC OggImportFile(OggImportGlobalsPtr globals, const FSSpec *theFile,
                                Movie theMovie, Track targetTrack, Track *usedTrack,
                                TimeValue atTime, TimeValue *durationAdded,
                                long inFlags, long *outFlags)
{
    ComponentResult err = noErr;
    AliasHandle alias = NULL;

	dprintf("-- File() called\n");
    
    *outFlags = 0;
    
    err = NewAliasMinimal(theFile, &alias);
    if (err == noErr)
    {
        err = MovieImportDataRef(globals->self,
                                (Handle)alias,
                                rAliasType,
                                theMovie,
                                targetTrack,
                                usedTrack,
                                atTime,
                                durationAdded,
                                inFlags,
                                outFlags);
    
        if (!(*outFlags & movieImportResultNeedIdles))
            DisposeHandle((Handle) alias);
        else
            globals->aliasHandle = alias;
    }
    
    return err;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
COMPONENTFUNC OggImportGetMIMETypeList(OggImportGlobalsPtr globals, QTAtomContainer *retMimeInfo)
{
	dprintf("-- GetMIMETypeList() called\n");
    return GetComponentResource((Component)globals->self, FOUR_CHAR_CODE('mime'), kImporterResID, (Handle *)retMimeInfo);
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
COMPONENTFUNC OggImportGetFileType(OggImportGlobalsPtr globals, OSType *fileType)
{
	dprintf("-- GetFileType() called\n");
    *fileType = kCodecFormat;
    return noErr;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
COMPONENTFUNC OggImportSetChunkSize(OggImportGlobalsPtr globals, long chunkSize)
{
    ComponentResult err = noErr;
    
	dprintf("-- ImportSetChunkSize(%ld) called\n", chunkSize);
    if (chunkSize > 2048 && chunkSize < 204800)
        globals->chunkSize = chunkSize;
    else
        err = paramErr;
    
    return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
COMPONENTFUNC OggImportIdle(OggImportGlobalsPtr globals,
                                long  inFlags,
                                long *outFlags)
{
    ComponentResult err = noErr;
	dprintf("-> Idle() called    [%08lx]\n", (long)globals);

	if (globals->state == kStateImportComplete) {
		*outFlags |= movieImportResultComplete;
		return err;
	}

	err = StateProcess(globals);

#if 0
	if (true) {
		Boolean needs = false;
		TimeRecord ni;

		if (globals->idleManager != NULL) {
			QTIdleManagerNeedsAnIdle(globals->idleManager, &needs);
			if (needs) {
				QTIdleManagerGetNextIdleTime(globals->idleManager, &ni);
				dprintf("-- -- IdleManager :: requested: base: %ld, scale: %ld, value: %ld %ld\n", (long)ni.base, ni.scale,
						ni.value.hi, ni.value.lo);
			} else {
				dprintf("-- -- IdleManager :: not needed\n");
			}
		}

		if (globals->dataIdleManager != NULL) {
			QTIdleManagerNeedsAnIdle(globals->dataIdleManager, &needs);
			if (needs) {
				QTIdleManagerGetNextIdleTime(globals->dataIdleManager, &ni);
				dprintf("-- -- DataIdleManager :: requested: base: %ld, scale: %ld, value: %ld %ld\n", (long)ni.base, ni.scale,
						ni.value.hi, ni.value.lo);
			} else {
				dprintf("-- -- DataIdleManager :: not needed\n");
			}
		}
	}
#endif

	dprintf("-< Idle: %ld        [%08lx]\n", (long)err, (long)globals);
    return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
COMPONENTFUNC OggImportDataRef(OggImportGlobalsPtr globals, Handle dataRef,
                                OSType dataRefType, Movie theMovie,
                                Track targetTrack, Track *usedTrack,
                                TimeValue atTime, TimeValue *durationAdded,
                                long inFlags, long *outFlags)
{
    ComponentResult err = noErr;
	
	*outFlags = 0;
	
	globals->theMovie = theMovie;
	globals->startTime = atTime;

	dprintf("-- DataRef(at:%ld) called\n", atTime);
	if (GetHandleSize(dataRef) < 256) {
		dprintf("-- - DataRef: \"%s\"\n", *dataRef);
	} else {
		dprintf("-- - DataRef: '%c'\n", *dataRef[0]);
	}

	dprintf("    theMovie: %lx,  targetTrack: %lx\n", theMovie, targetTrack);
	dprintf("    track count: %ld\n", GetMovieTrackCount(theMovie));
	dprintf("    flags:\n\tmovieImportCreateTrack:%d\n\tmovieImportInParallel:%d\n"
			"\tmovieImportMustUseTrack:%d\n\tmovieImportWithIdle:%d\n",
		   (inFlags & movieImportCreateTrack)  != 0,
		   (inFlags & movieImportInParallel)   != 0,
		   (inFlags & movieImportMustUseTrack) != 0,
		   (inFlags & movieImportWithIdle)     != 0);
	dprintf("     : importing at: %ld, added: %ld\n", atTime, *durationAdded);
    err = SetupDataHandler(globals, dataRef, dataRefType);
	if (err == noErr)
		dprintf("    SetupDataHandler() succeeded\n");

	globals->usingIdle = ((globals->dataIsStream || globals->dataCanDoAsyncRead)
						  //&& globals->dataCanDoGetFileSizeAsync
						  && (inFlags & movieImportWithIdle) != 0);

    if (dataRefType != URLDataHandlerSubType)
        globals->usingIdle = false;
	dprintf("--> 2: globals->usingIdle: %d\n", globals->usingIdle);

	if (globals->usingIdle) {
		err = StartImport(globals, dataRef, dataRefType);
		*outFlags |= movieImportResultNeedIdles;
		*durationAdded = 0;
	} else {
		err = JustImport(globals, dataRef, dataRefType);
		*outFlags &= !movieImportResultNeedIdles;
		if (err == noErr) {
			*outFlags |= movieImportResultComplete;
			*durationAdded = globals->timeLoaded;
			if (globals->numTracksSeen == 1)
				*usedTrack = globals->firstTrack;
		}
	}
	
	//globals->usingIdle = true;
	//*outFlags |= movieImportResultNeedIdles;
	
	dprintf("-< DataRef(at:%ld)\n", atTime);
    return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
COMPONENTFUNC OggImportGetLoadState(OggImportGlobalsPtr globals, long *loadState)
{
	dprintf("-- GetLoadState() called\n");
    switch (globals->state)
    {
	case kStateInitial:
    case kStateGettingSize:
        *loadState = kMovieLoadStateLoading;
        break;
    
    case kStateReadingPages:
		if (globals->timeLoaded > 0)
			if (globals->sizeInitialised && S64Compare(globals->dataEndOffset, S64Set(-1)) == 0)
				*loadState = kMovieLoadStatePlaythroughOK;
			else
				*loadState = kMovieLoadStatePlayable;
		else
			*loadState = kMovieLoadStateLoading;

		break;

    case kStateReadingLastPages:
            *loadState = kMovieLoadStatePlaythroughOK;
        break;
        
    case kStateImportComplete:
        *loadState = kMovieLoadStateComplete;
        break;	
    }
    
	dprintf("-- GetLoadState returning %ld\n", *loadState);

    return noErr;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
COMPONENTFUNC OggImportGetMaxLoadedTime(OggImportGlobalsPtr globals, TimeValue *time)
{
	dprintf("-- GetMaxLoadedTime() called: %8ld (at: %ld)\n", globals->timeLoaded, TickCount());

	*time = globals->timeLoaded;

	return noErr;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
COMPONENTFUNC OggImportEstimateCompletionTime(OggImportGlobalsPtr globals, TimeRecord *time)
{
    unsigned long timeUsed = TickCount() - globals->startTickCount;
    SInt64        dataUsed = S64Subtract(globals->dataOffset, globals->dataStartOffset);
    SInt64        dataLeft = S64Subtract(globals->dataEndOffset, globals->dataOffset);
    SInt64        ratio    = S64Div(S64Multiply(S64Set(timeUsed), dataLeft), dataUsed);

	dprintf("-- EstimateCompletionTime() called: ratio = %lld\n", ratio);

    time->value = SInt64ToWide(ratio);
    time->scale = 60;
    time->base  = NULL;   // this time record is a duration

	return noErr;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
COMPONENTFUNC OggImportSetDontBlock(OggImportGlobalsPtr globals, Boolean  dontBlock)
{
	dprintf("-- SetDontBlock(%d) called\n", dontBlock);
    globals->blocking = dontBlock;
    return noErr;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
COMPONENTFUNC OggImportGetDontBlock(OggImportGlobalsPtr globals, Boolean  *willBlock)
{
	dprintf("-- GetDontBlock() called\n");
    *willBlock = globals->blocking;
    return noErr;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
COMPONENTFUNC OggImportSetIdleManager(OggImportGlobalsPtr globals, IdleManager im)
{
    ComponentResult err = noErr;

	dprintf("-- SetIdleManager() called\n");
    globals->idleManager = im;

    if (globals->dataReader)
    {
        if (CallComponentCanDo(globals->dataReader, kDataHSetIdleManagerSelect) == true)
        {
			globals->dataIdleManager = QTIdleManagerOpen();
			if (globals->dataIdleManager != NULL) {
				err = DataHSetIdleManager(globals->dataReader, im);
				dprintf("--  -- SetIdleManager(dataReader) = %ld\n", (long)err);
				if (err != noErr) {
					QTIdleManagerClose(globals->dataIdleManager);
					err = noErr;
				} else {
					err = QTIdleManagerSetParent(globals->dataIdleManager, globals->idleManager);
					dprintf("--  -- SetParentIdleManager() = %ld\n", (long)err);
					err = noErr;
				}
			}
        } else {
			dprintf("--  -- SetIdleManager(dataReader) = DOESN'T SUPPORT IDLE!!\n");
		}
    }
	
	QTIdleManagerSetNextIdleTimeNow(globals->idleManager);
    return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
COMPONENTFUNC OggImportSetNewMovieFlags(OggImportGlobalsPtr globals, long flags)
{
	dprintf("-- SetNewMovieFlags() called: %08lx\n", flags);

    globals->newMovieFlags = flags;
    return noErr;
}
