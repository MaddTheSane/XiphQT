//
//  OggAudioFile.cpp
//  Xiph Audio Components
//
//  Created by C.W. Betts on 5/5/16.
//  Copyright © 2016 C.W. Betts. All rights reserved.
//

#include <AudioToolbox/AudioToolbox.h>
#include "OggAudioFile.hpp"
#include "CABundleLocker.h"
#include "fccs.h"
#include "GetCodecBundle.h"

static OSStatus GetAllFormatIDs(UInt32* ioDataSize, void* outPropertyData)
{
	if (!outPropertyData)
	{
		return AudioFormatGetPropertyInfo(kAudioFormatProperty_DecodeFormatIDs, 0, NULL, ioDataSize);
	}
	else
	{
		return AudioFormatGetProperty(kAudioFormatProperty_DecodeFormatIDs, 0, NULL, ioDataSize, outPropertyData);
	}
}

static bool	IsSupportedOggFormat(UInt32	inFormatID)
{
	switch (inFormatID) {
		case kAudioFormatXiphOggFramedVorbis:
		case kAudioFormatXiphOggFramedSpeex:
		case kAudioFormatXiphOggFramedFLAC:
			return true;
			break;
			
		case kAudioFormatXiphVorbis:
		case kAudioFormatXiphSpeex:
		case kAudioFormatXiphFLAC:
			return true;
			break;
	}
	return false;
}

#pragma mark - OggAudioFormat
Boolean OggAudioFormat::ExtensionIsThisFormat(CFStringRef inExtension)
{
	if ((CFStringCompare(inExtension, CFSTR("ogg"), kCFCompareCaseInsensitive) == kCFCompareEqualTo))
	{
		return true;
	}
	else
	{
		return false;
	}
}

UncertainResult OggAudioFormat::FileDataIsThisFormat(UInt32 inDataByteSize, const void* inData)
{
	if (CFSwapInt32BigToHost(*(UInt32 *)inData) == kOggStartBytes) {
		return kTrue;
	} else {
		return kFalse;
	}
}

void OggAudioFormat::GetExtensions(CFArrayRef *outArray)
{
	const int size = 1;
	CFStringRef data[size];
	
	data[0] = CFSTR("ogg");
	
	*outArray = CFArrayCreate(kCFAllocatorDefault, (const void**)data, size, &kCFTypeArrayCallBacks);
}

AudioFileObject* OggAudioFormat::New()
{
	return new OggAudioFile();
}

void OggAudioFormat::GetFileTypeName(CFStringRef *outName)
{
	CABundleLocker lock;
	CFBundleRef theBundle = GetCodecBundle();
	if(theBundle != NULL)
	{
		*outName = CFCopyLocalizedStringFromTableInBundle(CFSTR("Ogg"), CFSTR("FileTypeNames"), theBundle, CFSTR("A file type name."));
	}
	else
	{
		*outName = CFSTR("Ogg");
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus OggAudioFormat::GetHFSCodes(UInt32* ioDataSize, void* outPropertyData)
{
	const UInt32 size = 1;
	UInt32 data[size];
	data[0] = kAudioFormatOgg;
	
	UInt32 numIDs = std::min(size, UInt32(*ioDataSize / sizeof(UInt32)));
	*ioDataSize = numIDs * sizeof(UInt32);
	if (outPropertyData)
	{
		memcpy(outPropertyData, data, *ioDataSize);
	}
	return noErr;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus OggAudioFormat::GetAvailableFormatIDs(UInt32* ioDataSize, void* outPropertyData)
{
	UInt32		formatCount = 0;
	UInt32		data[4];
	
	UInt32 fmtIDsize;
	OSStatus err = GetAllFormatIDs(&fmtIDsize, NULL);
	if (err)
	{
		return err;
	}
	
	UInt32 numFormats = fmtIDsize / sizeof(UInt32);
	CAAutoFree<UInt32> fmtIDs(numFormats, true);
	err = GetAllFormatIDs(&fmtIDsize, fmtIDs());
	if (err)
	{
		return err;
	}
	
	for (UInt32 i=0; i<numFormats; ++i)
	{
		if(IsSupportedOggFormat(fmtIDs[i]))
		{
			data[formatCount] = fmtIDs[i];
			formatCount++;
		}
	}
	
	fmtIDs = NULL;
	
	*ioDataSize = formatCount * sizeof(UInt32);
	if (outPropertyData)
	{
		memcpy(outPropertyData, data, *ioDataSize);
	}
	return noErr;
	
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus OggAudioFormat::GetAvailableStreamDescriptions(UInt32 inFormatID, UInt32* ioDataSize, void* outPropertyData)
{
	OSStatus						err = noErr;
	UInt32							numIDs = 1;
	const UInt32					maxsize = 1;
	AudioStreamBasicDescription		desc[maxsize];
	
	// make sure it is a supported format.
	UInt32 fmtIDsize;
	err = GetAllFormatIDs(&fmtIDsize, NULL);
	if (err)
	{
		return err;
	}
	
	UInt32 numFormats = fmtIDsize / sizeof(UInt32);
	CAAutoFree<UInt32> fmtIDs(numFormats, true);
	err = GetAllFormatIDs(&fmtIDsize, fmtIDs());
	if (err)
	{
		return err;
	}
	
	bool found = false;
	for (UInt32 i = 0; i < numFormats; ++i)
	{
		if (fmtIDs[i] == inFormatID)
		{
			// the format is available, see if it is ok for the file type
			if(IsSupportedOggFormat(inFormatID))
			{
				found = true;
				break;
			}
		}
	}
	
	fmtIDs = NULL;
	if (!found)
	{
		*ioDataSize = 0;
		return kAudioFileUnsupportedDataFormatError;
	}
	
	memset(desc, 0, numIDs * sizeof(AudioStreamBasicDescription));
	desc[0].mFormatID = inFormatID;
	UInt32 size = sizeof(AudioStreamBasicDescription);
	AudioFormatGetProperty(kAudioFormatProperty_FormatInfo, 0, NULL, &size, desc+0);
	
	numIDs = std::min(numIDs, UInt32(*ioDataSize / sizeof(AudioStreamBasicDescription)));
	*ioDataSize = numIDs * sizeof(AudioStreamBasicDescription);
	if (outPropertyData)
	{
		memcpy(outPropertyData, desc, *ioDataSize);
	}
	return err;
}


#pragma mark - OggAudioFile
OggAudioFile::~OggAudioFile ()
{
	if (mMagicCookie != NULL)
	{
		free (mMagicCookie);
	}
}

OSStatus OggAudioFile::UpdateSize()
{
	return kAudio_UnimplementedError;
}

OSStatus OggAudioFile::Create(CFURLRef inFileRef, const AudioStreamBasicDescription *inFormat)
{
	return kAudio_UnimplementedError;
}

OSStatus OggAudioFile::CreatePacketTable()
{
	return kAudio_UnimplementedError;
}

OSStatus OggAudioFile::GetChannelLayout(UInt32 *ioDataSize, AudioChannelLayout *ioRegionList)
{
	return kAudio_UnimplementedError;
}

OSStatus OggAudioFile::GetChannelLayoutSize(UInt32 *outDataSize, UInt32 *isWritable)
{
	return kAudio_UnimplementedError;
}

OSStatus OggAudioFile::GetEstimatedDuration(Float64 *duration)
{
	return kAudio_UnimplementedError;
}

OSStatus OggAudioFile::GetFormatList(UInt32 &ioDataSize, AudioFormatListItem *ioPropertyData)
{
	return kAudio_UnimplementedError;
}

OSStatus OggAudioFile::GetFormatListInfo(UInt32 &outDataSize, UInt32 &outWritable)
{
	return kAudio_UnimplementedError;
}

OSStatus OggAudioFile::GetHeaderSize(SInt64 filePosition, UInt32 &headerSize, bool checkFirstHeader)
{
	return kAudio_UnimplementedError;
}

OSStatus OggAudioFile::GetMagicCookieData(UInt32 *ioDataSize, void *ioPropertyData)
{
	return kAudio_UnimplementedError;
}

OSStatus OggAudioFile::GetMagicCookieDataSize(UInt32 *outDataSize, UInt32 *isWritable)
{
	return kAudio_UnimplementedError;
}

SInt64 OggAudioFile::GetNumBytes()
{
	return -1;
}

void OggAudioFile::SetNumBytes(SInt64 inNumBytes)
{
	
}

SInt64 OggAudioFile::GetNumPackets()
{
	return -1;
}

void OggAudioFile::SetNumPackets(SInt64 inNumPackets)
{
	
}

OSStatus OggAudioFile::GetProperty(AudioFilePropertyID inPropertyID, UInt32 *ioDataSize, void *ioPropertyData)
{
	return kAudio_UnimplementedError;
}

OSStatus OggAudioFile::GetPropertyInfo(AudioFilePropertyID inPropertyID, UInt32 *outDataSize, UInt32 *isWritable)
{
	return kAudio_UnimplementedError;
}

OSStatus OggAudioFile::InitializeDataSource(const AudioStreamBasicDescription *inFormat, UInt32 inFlags)
{
	return kAudio_UnimplementedError;
}

Boolean OggAudioFile::IsDataFormatSupported(const AudioStreamBasicDescription *inFormat)
{
	return false;
}

OSStatus OggAudioFile::OpenFromDataSource()
{
	return kAudio_UnimplementedError;
}

OSStatus OggAudioFile::ParseAudioFile()
{
	return kAudio_UnimplementedError;
}

OSStatus OggAudioFile::ScanForPackets(SInt64 inToPacketCount, DataSource* inDataSrc, bool fullyParsedIfEndOfDataReached)
{
	return kAudio_UnimplementedError;
}

OSStatus OggAudioFile::ReadPackets(Boolean inUseCache, UInt32 *outNumBytes, AudioStreamPacketDescription *outPacketDescriptions, SInt64 inStartingPacket, UInt32 *ioNumPackets, void *outBuffer)
{
	return kAudio_UnimplementedError;
}

OSStatus OggAudioFile::SetMagicCookieData(UInt32 inDataSize, const void *inPropertyData)
{
	return kAudio_UnimplementedError;
}

OSStatus OggAudioFile::SetProperty(AudioFilePropertyID inPropertyID, UInt32 inDataSize, const void *inPropertyData)
{
	return kAudio_UnimplementedError;
}

OSStatus OggAudioFile::WritePackets(Boolean inUseCache, UInt32 inNumBytes, const AudioStreamPacketDescription *inPacketDescriptions, SInt64 inStartingPacket, UInt32 *ioNumPackets, const void *inBuffer)
{
	return kAudio_UnimplementedError;
}
