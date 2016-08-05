/*	Copyright © 2007 Apple Inc. All Rights Reserved.
	
	Disclaimer: IMPORTANT:  This Apple software is supplied to you by 
			Apple Inc. ("Apple") in consideration of your agreement to the
			following terms, and your use, installation, modification or
			redistribution of this Apple software constitutes acceptance of these
			terms.  If you do not agree with these terms, please do not use,
			install, modify or redistribute this Apple software.
			
			In consideration of your agreement to abide by the following terms, and
			subject to these terms, Apple grants you a personal, non-exclusive
			license, under Apple's copyrights in this original Apple software (the
			"Apple Software"), to use, reproduce, modify and redistribute the Apple
			Software, with or without modifications, in source and/or binary forms;
			provided that if you redistribute the Apple Software in its entirety and
			without modifications, you must retain this notice and the following
			text and disclaimers in all such redistributions of the Apple Software. 
			Neither the name, trademarks, service marks or logos of Apple Inc. 
			may be used to endorse or promote products derived from the Apple
			Software without specific prior written permission from Apple.  Except
			as expressly stated in this notice, no other rights or licenses, express
			or implied, are granted by Apple herein, including but not limited to
			any patent rights that may be infringed by your derivative works or by
			other works in which the Apple Software may be incorporated.
			
			The Apple Software is provided by Apple on an "AS IS" basis.  APPLE
			MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION
			THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS
			FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND
			OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
			
			IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL
			OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
			SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
			INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION,
			MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED
			AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE),
			STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
			POSSIBILITY OF SUCH DAMAGE.
*/
#include "FLACAudioFile.h"
#include "FLACAudioFileComponent.h"
#include <AudioToolbox/AudioConverter.h>
#include <AudioToolbox/AudioFile.h>
#include "AudioFileObject.h"
#include "CADebugMacros.h"
#include <AudioToolbox/AudioFormat.h>
#include "CAMath.h"
#include "CABundleLocker.h"
#include "CAAutoDisposer.h"
#include "ACFLACApple.h"
#include "fccs.h"

#define	kCompletePacketTable            -1

#define DEBUG_PRINT 0

#define kStreamInfoSize 38 // 1 byte type + 3 bytes size + 34 bytes data

static const Float32 gFLACSampleRates[16]			= 	{	0.0f, // in the stream info
                                            88200.0f,
                                           176400.0f,
										   192000.0f,
                                             8000.0f,
                                            16000.0f,
                                            22050.0f,
                                            24000.0f,
                                            32000.0f,
                                            44100.0f,
                                            48000.0f,
                                            96000.0f,
                                            0.0f,	// kHz in 8-bit field in header
                                            0.0f,	// Hz in 16-bit field in header
                                            0.0f,	// DHz in 16-bit field in header (yes, DHz)
                                            0.0f	};	// invalid to prevent syncword issues

static const UInt32 gFLACBlockSizes[16]			= 	{	0, // reserved
                                              192,
                                              576,
											 1152,
                                             2304,
                                             4608,
                                                0, // 8 bits at the end of the header
                                                0, // 16 bits at the end of the header
                                              256,
                                              512,
                                             1024,
                                             2048,
										     4096,
											 8192,	
											16384,
											32768 };

static const UInt32 gFLACBitDepths[8]			= 	{	0, // get from STREAMINFO metadata block
											8,
										   12,
											0, // reserved
										   16,
										   20,
										   24,
										    0 }; // reserved

static const AudioChannelLayoutTag gFLACChannelConfigToLayoutTag[] = {
	kAudioChannelLayoutTag_Mono,
	kAudioChannelLayoutTag_Stereo,
	kAudioChannelLayoutTag_MPEG_3_0_A,
	kAudioChannelLayoutTag_Quadraphonic,
	kAudioChannelLayoutTag_MPEG_5_0_A,
	kAudioChannelLayoutTag_MPEG_5_1_A,
	kAudioChannelLayoutTag_Unknown | 7, // 7 channels, not defined in FLAC
	kAudioChannelLayoutTag_Unknown | 8, // 8 channels, not defined in FLAC
	kAudioChannelLayoutTag_Unknown | 2, // left/side stereo, not defined by Apple
	kAudioChannelLayoutTag_Unknown | 2, // side/right stereo. not defined by Apple
	kAudioChannelLayoutTag_MidSide,
	0, // reserved
	0, // reserved
	0, // reserved
	0, // reserved
	0  // reserved
};

// This is directly out of the FLAC crc.c sources -- we don't want to link against
// those sources directly. This table should never change

static Byte const FLAC_crc8[256] = {
	0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
	0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
	0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
	0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
	0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
	0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
	0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
	0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
	0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
	0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
	0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
	0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
	0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
	0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
	0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
	0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
	0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
	0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
	0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
	0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
	0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
	0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
	0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
	0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
	0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
	0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
	0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
	0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
	0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
	0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
	0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
	0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static bool IsSupportedFLACFormat(UInt32	inFormatID)
{
	switch(inFormatID)
	{
		case kAudioFormatFLAC:
			return true;
			break;
			
		case kAudioFormatXiphFLAC:
			return true;
			break;
	}
	return false;
}

OSStatus GetAllFormatIDs(UInt32* ioDataSize, void* outPropertyData)
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

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AudioFileObject* FLACAudioFormat::New()
{
	return new FLACAudioFile();
}

Boolean FLACAudioFormat::ExtensionIsThisFormat(CFStringRef inExtension)
{
    if ((CFStringCompare(inExtension, CFSTR("flac"), kCFCompareCaseInsensitive) == kCFCompareEqualTo))
	{
        return true;
	}
    else
	{
        return false;
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UncertainResult FLACAudioFormat::FileDataIsThisFormat(UInt32 inDataByteSize, const void* inData)
{
    if (CFSwapInt32BigToHost(*(UInt32 *)inData) == kFLACStartBytes)
	{
		return kTrue;
	}
	else
	{
		return kFalse;
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus FLACAudioFile::ReadPackets(	Boolean							inUseCache,
                                    UInt32							*outNumBytes,
                                    AudioStreamPacketDescription	*outPacketDescriptions,
                                    SInt64							inStartingPacket, 
                                    UInt32  						*ioNumPackets, 
                                    void							*outBuffer)
{
    OSStatus		err = noErr;
    CompressedPacketTable*	packetTable	;
    bool            packetDescriptionsOnly = ((outBuffer == NULL) && (outPacketDescriptions != NULL)) ? true : false;
    
    FailWithAction((ioNumPackets == NULL) || (*ioNumPackets < 1), err = kAudioFileUnspecifiedError, Bail, "invalid num packets parameter");

	// make sure the packet table large enough to allow this read packets call
	err = ScanForPackets(inStartingPacket + *ioNumPackets);
	FailIf(err != noErr, Bail, "ScanForPackets (FLAC) failed");
	
	// verify that there are any packets after inStartingPacket
	if (inStartingPacket > GetNumPackets())
	{
		return kAudioFileInvalidPacketOffsetError;
	}
	
	// now get the packet table
	packetTable = GetPacketTable();
    if (packetTable)
    {
        SInt64                              startingPacketOffset = 0;
        UInt32	                           totalBytes = 0;
        UInt32                              i;
        AudioStreamPacketDescription 		curPacket;
        SInt64                              totalPacketCount =  GetNumPackets();
        
        *outNumBytes = 0;
        
        if (*ioNumPackets + inStartingPacket > totalPacketCount)
		{
            *ioNumPackets = totalPacketCount - inStartingPacket;
		}
			
		if (*ioNumPackets == 0)
		{
			// there aren't any more packets so return now
			*outNumBytes = 0;
			return eofErr;	// this error will get sanitized upstream
		}
			
        startingPacketOffset = (*packetTable)[inStartingPacket].mStartOffset;
        
        if (packetDescriptionsOnly)
        {
            // just fill out the packet descriptions
            for (i = 0; i < *ioNumPackets; i++)
            {
                curPacket = (*packetTable)[i + inStartingPacket];
                outPacketDescriptions[i].mStartOffset = (curPacket.mStartOffset - GetDataOffset() );
                outPacketDescriptions[i].mDataByteSize = curPacket.mDataByteSize;
                outPacketDescriptions[i].mVariableFramesInPacket = 0;
            }
            totalBytes = 0;
        }
        else
        {
			UInt8		*bufferPos = (UInt8 *) outBuffer;
            // calculate the size of the read and fill out the packet descriptions
            for (i = 0; i < *ioNumPackets; i++)
            {
                curPacket = (*packetTable)[i + inStartingPacket];
                totalBytes += curPacket.mDataByteSize;
                if (outPacketDescriptions)
                {	
                    outPacketDescriptions[i].mStartOffset = (curPacket.mStartOffset - startingPacketOffset );
                    outPacketDescriptions[i].mDataByteSize = curPacket.mDataByteSize;
                    outPacketDescriptions[i].mVariableFramesInPacket = 0;
                }
            }

            err = ReadBytes (inUseCache, (*packetTable)[inStartingPacket].mStartOffset - GetDataOffset(), &totalBytes, bufferPos);
            FailIf(err && err != eofErr, Bail, "ReadBytes failed");
        }
        
        *outNumBytes = totalBytes;
    }
    else
	{
        err = kAudioFileInvalidFileError;
	}

Bail:
    return (err);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
FLACAudioFile::~FLACAudioFile ()
{
	if (mMagicCookie != NULL)
	{
		free (mMagicCookie);
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus FLACAudioFile::WritePackets(	Boolean								inUseCache,
                                        UInt32								inNumBytes,
                                        const AudioStreamPacketDescription	*inPacketDescriptions,
                                        SInt64								inStartingPacket, 
                                        UInt32								*ioNumPackets, 
                                        const void							*inBuffer)
{
    OSStatus						err = noErr;
    UInt64							i;
    UInt8							*bufLoc = (UInt8 *) inBuffer;
    AudioStreamPacketDescription	curPacket = {0};
    CompressedPacketTable			*pTable;
    UInt32							byteCount;
    UInt32							totalByteCount = 0;
    UInt32							packetCount = 0;
	SInt64							offset = 0;
	AudioStreamBasicDescription		dataFormat = GetDataFormat();

	if ((inStartingPacket == 0) && (dataFormat.mFormatID != kAudioFormatFLAC) && (mMagicCookie == NULL))
	{
		// FLAC requires that the magic cookie be present before writing the first packet.
		return kAudioFileUnspecifiedError;
	}
		
    // return right away if requested to write zero packets or a NULL ioNumPackets is passed
    FailIf(ioNumPackets == NULL, Bail, "WritePackets Failed");
    FailIf(*ioNumPackets == 0, Bail, "WritePackets Failed");
    // for now, only append packets to the end
    FailWithAction(inStartingPacket != GetNumPackets(), err = kAudioFileInvalidPacketOffsetError, Bail, "");

	pTable = GetPacketTable(true);

	FailIf(pTable == NULL, Bail, "WritePackets: GetPacketTable() Failed");
    FailWithAction(inPacketDescriptions == NULL, err = kAudioFileInvalidPacketOffsetError, Bail, "Packet Descriptions were not provided");

    offset = (GetNumPackets() == 0) ? GetDataOffset() : (*pTable)[GetNumPackets() - 1].mStartOffset + (*pTable)[GetNumPackets() - 1].mDataByteSize;

    for (i = 0; i < *ioNumPackets; i++)
    {        
 		UInt32 flacStart = CFSwapInt32HostToBig(kFLACStartBytes);
        
        if (GetNumPackets() == 0)
        {
 			// we don't do any of the odd stereo layouts
			mChannelLayoutTag = gFLACChannelConfigToLayoutTag[dataFormat.mChannelsPerFrame - 1];
			// write out fLaC and the streaminfo
			err = GetDataSource()->WriteBytes(fsFromStart, 0, 4, &flacStart, &byteCount);
            // Figure out where the first packet goes -- for now we leave space for the streaminfo, but we can't write this until the very end
			curPacket.mStartOffset = sizeof(flacStart) + kStreamInfoSize;
            curPacket.mDataByteSize = inPacketDescriptions[i].mDataByteSize;
			mPacketTableInfo.mNumberValidFrames = 0;	
		}
		else
		{
			AudioStreamPacketDescription   previousPacket = (*pTable)[GetNumPackets() - 1];
            curPacket.mStartOffset = previousPacket.mStartOffset +  previousPacket.mDataByteSize;
            curPacket.mDataByteSize = inPacketDescriptions[i].mDataByteSize;
		}

        if (inPacketDescriptions[i].mDataByteSize > GetMaximumPacketSize())
		{
            SetMaximumPacketSize(inPacketDescriptions[i].mDataByteSize);
		}
        
        // write the packet 
        bufLoc = (UInt8 *) inBuffer + inPacketDescriptions[i].mStartOffset;
        err = GetDataSource()->WriteBytes(fsFromStart, curPacket.mStartOffset, inPacketDescriptions[i].mDataByteSize, (Ptr) bufLoc, &byteCount);
        totalByteCount += byteCount;
		mPacketTableInfo.mNumberValidFrames += dataFormat.mFramesPerPacket;

        // append the packet to the packet table
        AppendPacket(curPacket);

        packetCount++;
    }
    
    *ioNumPackets = packetCount;
    
Bail:
    return (err);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus FLACAudioFile::OpenFromDataSource(	SInt8 inPermissions)
{		
	OSStatus err = noErr;

	err = ParseAudioFile();
	FailIf(err != noErr, Bail, "ParseAudioFile failed");
		
#if 0
	// for testing read packets just to obtain the descriptions
	{
		SInt64	packetCount	= GetNumPackets();
		AudioStreamPacketDescription	pDesc[packetCount + 2048];
		SInt64	start = 0;
		UInt32	numBytes = 100;
		UInt32	numPackets = packetCount + 2048;
		
		OSStatus	err = ReadPackets(false, &numBytes, pDesc, start, &numPackets, NULL);

		printf("ReadPackets returned = %ld\n",numPackets);
	}
#endif
	
Bail:
	return err;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus FLACAudioFile::InitializeDataSource(	const AudioStreamBasicDescription*  inFormat,
                                                UInt32								/*inFlags*/)
{
	OSStatus err = noErr;
	
	// verify that the format is correct for GetFileType()
	if (!IsSupportedFLACFormat(inFormat->mFormatID))
	{
		return kAudioFileUnsupportedDataFormatError;
	}

	return err;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus FLACAudioFile::Create(		CFURLRef							inFileRef,
									const AudioStreamBasicDescription	*inFormat)
{
	
	// verify that the format is correct for GetFileType()
	if (!IsSupportedFLACFormat(inFormat->mFormatID))
	{
		return kAudioFileUnsupportedDataFormatError;
	}

	// it checked out ok, so really create the audio file now
	return AudioFileObject::Create(inFileRef, inFormat);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus FLACAudioFile::ParseAudioFile()
{
    OSStatus					err = noErr;
    UInt32						byteCount;
    AudioStreamBasicDescription	format;
    UInt32						theUInt32;
	SInt64						theFilePosition = 0;
	UInt32						moreData = 1;
	UInt32						theMetaDataBlockType = 0;

    // Ok, we assume we're starting from the beginning
	memset (&format, 0, sizeof(AudioStreamBasicDescription));
	
	// get the first 32 bits of the file so we can read the 32 bit FLAC stream marker
    err = GetDataSource()->ReadBytes(fsFromStart, theFilePosition, (uint32_t)sizeof(theUInt32), (Ptr) &theUInt32, &byteCount);
    FailIf(err != noErr, Bail, "FLAC FSRead Failed 1");
    
    // First we have the FLAC stream marker -- this must be the first four bytes of the file
	theUInt32 = CFSwapInt32BigToHost(theUInt32);
	if (theUInt32 != kFLACStartBytes)
	{
		return kAudioFileInvalidFileError;
	}
	theFilePosition += 4;

	// Next, parse the required metadata block and any additional blocks
	while (moreData)
	{
		err = GetDataSource()->ReadBytes(fsFromStart, theFilePosition, (uint32_t)sizeof(theUInt32), (Ptr) &theUInt32, &byteCount);
		theUInt32 = CFSwapInt32BigToHost(theUInt32);
		
		moreData =  theUInt32 >> 31;
		moreData = !moreData; // 1 means we hit the end
		theMetaDataBlockType = (theUInt32 >> 24) & 0x0000007f;
		switch (theMetaDataBlockType)
		{
			case 0: // STREAMINFO
				err = ParseStreamInfo(theFilePosition, format);
				SetDataFormat(&format);
				break;
			case 1: // PADDING
			case 2: // APPLICATION
			case 3: // SEEKTABLE // not implemented, but necessary for easy stream indexing
			case 4: // VORBIS_COMMENT
			case 5: // CUESHEET
			case 6: // PICTURE
			default: // 6 - 126 reserved
				break;
			case 127:
				return kAudioFileInvalidFileError; // illegal value
				break;
		}
		if (err != noErr)
		{
			return (err);
		}
		
		theFilePosition += 4; // header -- not counted in size of block
		theFilePosition += (theUInt32 & 0x00ffffff); // size of block
	}
	
	// Now we read the audio frames
	err = GetDataSource()->ReadBytes(fsFromStart, theFilePosition, (uint32_t)sizeof(theUInt32), (Ptr) &theUInt32, &byteCount);
	theUInt32 = CFSwapInt32BigToHost(theUInt32);
	theUInt32 >>= 16; // shift it down 16 bits to make this comparison easier
    if ((theUInt32 & kFLACSyncMask) == kFLACSyncWord)	
    {										
        // We found the data!
		SetDataOffset(theFilePosition);
    
		err = ScanForPackets(1);
		FailIf(err != noErr, Bail, "FLACAudioFile::ParseAudioFile : ScanForPackets failed");
                
        // Get the format of the data from the first packet. 
		// We could check this against the format data found in the StreamInfo
        err = ParseFirstPacketHeader(theFilePosition, format);
		FailIf(err != noErr, Bail, "FLAC FSRead Failed 2");
    }
    else
	{
        err = kAudioFileInvalidFileError;
	}

Bail:
    return (err);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// The nice thing is that it's a fixed size
OSStatus FLACAudioFile::ParseStreamInfo	(SInt64 filePosition, AudioStreamBasicDescription &format)
{
    OSStatus err = noErr;
	Byte theStreamInfo[kStreamInfoSize]; // Includes 4-byte header and 16 byte CRC
	UInt32 byteCount;
	UInt32 blockPosition = 0;
	UInt16 temp16Val1, temp16Val2;
	UInt32 temp32Val1, temp32Val2;
	UInt32 numPackets = 0;
	SInt64 duration = 0;
	FLAC_StreamInfo theCookie;
	
	format.mFormatID = kAudioFormatFLAC; // we know this already :-)
	
	err = GetDataSource()->ReadBytes(fsFromStart, filePosition, kStreamInfoSize, theStreamInfo, &byteCount);
	
	blockPosition += 4; // skip the size and type
	// check the block sizes -- two 16 bit fields
	theCookie.min_blocksize = temp16Val1 = CFSwapInt16BigToHost(*((UInt16 *)(theStreamInfo + blockPosition)));
	theCookie.max_blocksize = temp16Val2 = CFSwapInt16BigToHost(*((UInt16 *)(theStreamInfo + blockPosition + 2)));
	// the Cookie must be stored big endian
	theCookie.min_blocksize = CFSwapInt32HostToBig(theCookie.min_blocksize);
	theCookie.max_blocksize = CFSwapInt32HostToBig(theCookie.max_blocksize);
	
	if (temp16Val1 == temp16Val2)
	{
		format.mFramesPerPacket = temp16Val2;
	}
	else
	{
		format.mFramesPerPacket = 0; // it's variable
	}
	blockPosition += 4;
	// check the size of the packets -- two 24 bit fields
	temp32Val1 = CFSwapInt32BigToHost(*((UInt32 *)(theStreamInfo + blockPosition)));
	temp32Val2 = CFSwapInt32BigToHost(*((UInt32 *)(theStreamInfo + blockPosition + 3)));
	temp32Val1 >>= 8; // only the first 24 bits are valid
	temp32Val2 >>= 8; // only the first 24 bits are valid
	theCookie.min_framesize = CFSwapInt32HostToBig(temp32Val1);
	theCookie.max_framesize = CFSwapInt32HostToBig(temp32Val2);

	if (temp32Val1 == temp32Val2)
	{
		format.mBytesPerPacket = temp32Val2; // hypothetically we could be CBR -- however, a common value for this in flac is 0 -- unknown -- but that's fine
	}
	else
	{
		format.mBytesPerPacket = 0; // it's variable
	}
	blockPosition += 6;

	// get the sample rate, number of channels and bits per sample -- 20, 3, then 5 bits
	temp32Val1 = CFSwapInt32BigToHost(*((UInt32 *)(theStreamInfo + blockPosition)));

	theCookie.sample_rate = format.mSampleRate = (temp32Val1 >> 12);
	theCookie.sample_rate = CFSwapInt32HostToBig(theCookie.sample_rate);
	theCookie.channels = mChannelLayoutTag = format.mChannelsPerFrame = ((temp32Val1 >> 9) & 0x00000007) + 1;
	theCookie.channels = CFSwapInt32HostToBig(theCookie.channels);
	// mChannelLayoutTag only holds number of channels after this assignment -- we don't have enough info to look this up for stereo
	if (mChannelLayoutTag != 2)
	{
		mChannelLayoutTag = gFLACChannelConfigToLayoutTag[mChannelLayoutTag - 1];
	}

	// mBitsPerChannel is always 0 for a compressed format, so we have to store it off separately and set the format flags
	theCookie.bits_per_sample = mBitDepth = ((temp32Val1 >> 4) & 0x0000001f) + 1;
	theCookie.bits_per_sample = CFSwapInt32HostToBig(theCookie.bits_per_sample);
	// we assign the bit depth to the format flags -- we just use the same flags we use for Apple Lossless
	switch (mBitDepth)
	{
		case 16:
			format.mFormatFlags = kFLACFormatFlag_16BitSourceData;
			break;
		case 20:
			format.mFormatFlags = kFLACFormatFlag_20BitSourceData;
			break;
		case 24:
			format.mFormatFlags = kFLACFormatFlag_24BitSourceData;
			break;
		case 32:
			format.mFormatFlags = kFLACFormatFlag_32BitSourceData;
			break;
		default: // we don't have a flag for this
			break;
	}
	
	// These must be 0 for a compressed format
	format.mBitsPerChannel = format.mBytesPerFrame = format.mReserved = 0;
	
	// Get the total duration in frames -- 36 bits. the first 4 are from the current read
	duration = (temp32Val1 & 0x0000000f);
	duration <<= 32;
	blockPosition += 4;
	// the last 32 are from the next 4 bytes
	temp32Val1 = CFSwapInt32BigToHost(*((UInt32 *)(theStreamInfo + blockPosition)));
	duration += temp32Val1;
	
	theCookie.total_samples = mPacketTableInfo.mNumberValidFrames = duration;
	theCookie.total_samples = CFSwapInt64HostToBig(theCookie.total_samples);
	if (format.mFramesPerPacket != 0)
	{
		numPackets = (mPacketTableInfo.mNumberValidFrames/format.mFramesPerPacket);
		// we're done with duration so we use it as a temp variable
		duration = numPackets * format.mFramesPerPacket;
		if (duration < mPacketTableInfo.mNumberValidFrames) // we probably have a partial packet, so add one if we do
		{
			++numPackets;
		}
		mPacketTableInfo.mRemainderFrames = mPacketTableInfo.mNumberValidFrames - duration; // 0 is perfectly acceptable here
	}
	blockPosition += 4;
	
	// Finally, get the md5 checksum for the cookie
	for (UInt32 i = 0; i < 16; ++i)
	{
		theCookie.md5sum[i] = *(theStreamInfo + blockPosition + i);
	}
	// store the cookie
	SetMagicCookieData(sizeof(FLAC_StreamInfo), &theCookie);
	return err;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus FLACAudioFile::ParseFirstPacketHeader(SInt64 filePosition, AudioStreamBasicDescription &format)
{
    OSStatus                err = noErr;
    UInt32	                byteCount;
    CompressedPacketTable   *packetTable;
    UInt32                  sampleRateIndex = 0;
    UInt32                  channelConfigurationBits = 0;
    UInt32                  blockingStrategy = 0, blockSizeBits = 0;
	UInt32                  bitDepthBits = 0, bitDepth = 0;
	UInt32					blockingBytes = 0, blockingBits = 0;
	UInt32                  getBlockSize = 0, getSampleRate = 0;
	UInt32					currentByte = 0;
			
	memset(&format, 0, sizeof(format));
    
	packetTable = GetPacketTable(true);
    if (packetTable)
    {
		AudioStreamPacketDescription        curPacket = (*packetTable)[0];

        format.mSampleRate = 0;	
        format.mFormatID = kAudioFormatFLAC;	
        format.mFormatFlags = 0;	
        format.mBytesPerPacket = 0;	
        format.mFramesPerPacket = 0;		// It's constant, but we have no idea what the value is
        format.mBytesPerFrame = 0;		
        format.mChannelsPerFrame = 0;		// filled out later
        format.mBitsPerChannel = 0;	
        format.mReserved = 0;			
    
        CAAutoFree<Byte> FLACHeader(kFLACMaxHeaderSize, true);

        // read the fixed header
        err = GetDataSource()->ReadBytes(fsFromStart, curPacket.mStartOffset, kFLACMaxHeaderSize, (Ptr) FLACHeader(), &byteCount);
		if(err != noErr || byteCount != kFLACMaxHeaderSize) goto Bail;

        // skip some bits to get to the sample rate index -- syncword(14), reserved(1)
		++currentByte;
		blockingStrategy = FLACHeader[currentByte] & 0x01; // Blocking strategy (1)
		++currentByte;
		if (blockingStrategy == 0)
		{
			err = GetDataSource()->ReadBytes(fsFromStart, curPacket.mStartOffset, kFLACHeaderFixedDataSize, &mFixedHeader, &byteCount);
		}
		
		// get the block size in frames per packet
		blockSizeBits = (FLACHeader[currentByte] & 0xf0) >> 4; // Block size (4)
		format.mFramesPerPacket = gFLACBlockSizes[blockSizeBits]; // if blockSizeBits == 6 or 7, we'll need to revisit this
       
        // get the sample rate index
		sampleRateIndex = FLACHeader[currentByte] & 0x0f; // sample frequency index (4)
		++currentByte;
        
        // fill out the format
        format.mSampleRate = gFLACSampleRates[sampleRateIndex];	

        // get the channel layout
		channelConfigurationBits = (FLACHeader[currentByte] & 0xf0) >> 4; // channel configuration (4)
		mChannelLayoutTag = gFLACChannelConfigToLayoutTag[channelConfigurationBits];
		format.mChannelsPerFrame = mChannelLayoutTag & 0x0000ffff;
		
		// Get the bit depth
		bitDepthBits = (FLACHeader[currentByte] & 0x0e) >> 1; // bit depth (3)
		bitDepth = gFLACBitDepths[bitDepthBits];
		switch (bitDepth)
		{
			case 16:
				format.mFormatFlags = kFLACFormatFlag_16BitSourceData;
				break;
			case 20:
				format.mFormatFlags = kFLACFormatFlag_20BitSourceData;
				break;
			case 24:
				format.mFormatFlags = kFLACFormatFlag_24BitSourceData;
				break;
			case 32:
				format.mFormatFlags = kFLACFormatFlag_32BitSourceData;
				break;
			default: // we don't have a flag for this
				break;
		}
		
		// skip the reserve bit (1)
		++currentByte;

        // Ok, now we need to check to see if we need to read the end of the header
		if (blockSizeBits == 6 || blockSizeBits == 7) getBlockSize = 1;
		if (sampleRateIndex == 12 || sampleRateIndex == 13 || sampleRateIndex == 14) getSampleRate = 1;
		
		if (getBlockSize || getSampleRate)
		{
			// need to read past the sample/frame number
			blockingBits = FLACHeader[currentByte];
			if (blockingBits < 0x80)
			{
				blockingBytes = 1;
			}
			else if (blockingBits >= 0xc0 && blockingBits <= 0xdf)
			{
				blockingBytes = 2;
			}
			else if (blockingBits >= 0xe0 && blockingBits <= 0xef)
			{
				blockingBytes = 3;
			}
			else if (blockingBits >= 0xf0 && blockingBits <= 0xf7)
			{
				blockingBytes = 4;
			}
			else if (blockingBits >= 0xf8 && blockingBits <= 0xfb)
			{
				blockingBytes = 5;
			}
			else if (blockingBits >= 0xfc &&   blockingBits <= 0xfd)
			{
				blockingBytes = 6;
			}
			else if (blockingBits == 0xfe && blockingStrategy == 1)
			{
				blockingBytes = 7;
			}
			else
			{
				blockingBytes = 0; // invalid
			}
			currentByte += blockingBytes;
		}
		if (blockSizeBits == 6) // frames per packet is in 1 byte
		{
			 format.mFramesPerPacket = FLACHeader[currentByte] + 1;
			 ++currentByte;
		}
		else if (blockSizeBits == 7) // frames per packet is in 2 bytes
		{
			UInt32 tempFramesPerPacket = FLACHeader[currentByte];
			++currentByte;
			format.mFramesPerPacket = (tempFramesPerPacket << 8) + FLACHeader[currentByte] + 1;
			++currentByte;
		}
		
		if (sampleRateIndex == 12) // sample rate in kHz
		{
			format.mSampleRate = FLACHeader[currentByte] * 1000;
		}
		else if (sampleRateIndex == 13) // sample rate in Hz
		{
			UInt32 tempSampleRate = FLACHeader[currentByte];
			++currentByte;
			format.mSampleRate = (tempSampleRate << 8) + FLACHeader[currentByte];
		}
		else if (sampleRateIndex == 14) // sample rate in DHz
		{
			UInt32 tempSampleRate = FLACHeader[currentByte];
			++currentByte;
			format.mSampleRate = ((tempSampleRate << 8) + FLACHeader[currentByte]) * 10;
		}
   }
	    
Bail:
    return (err);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus	FLACAudioFile::CreatePacketTable()
{
    return ScanForPackets(kCompletePacketTable);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus FLACAudioFile::UpdateSize()
{
	OSStatus err = noErr;
	return err;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Boolean FLACAudioFile::IsDataFormatSupported(AudioStreamBasicDescription const  *desc)
{
	return IsSupportedFLACFormat(desc->mFormatID);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus FLACAudioFile::GetChannelLayoutSize(	UInt32					*outDataSize,
												UInt32					*isWritable)
{
	if (isWritable)
	{
		*isWritable = 0;
	}
    if (outDataSize)
	{
        *outDataSize = sizeof(AudioChannelLayout);
	}
	
    return noErr;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus FLACAudioFile::GetChannelLayout(	UInt32						*ioDataSize,
											AudioChannelLayout			*ioPropertyData)
{
	if (!ioDataSize)
	{
		return kAudioFileBadPropertySizeError;
	}
	if (*ioDataSize < offsetof(AudioChannelLayout, mChannelDescriptions))
	{
		return kAudioFileBadPropertySizeError;
	}

	size_t	dSize = *ioDataSize;
	*ioDataSize = (UInt32)std::min(dSize, sizeof(AudioChannelLayout));

    if (ioPropertyData)
    {
        memset(ioPropertyData, 0, *ioDataSize);
		ioPropertyData->mChannelLayoutTag = mChannelLayoutTag;
	}
	
	return noErr;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void FLACAudioFormat::GetExtensions(CFArrayRef *outArray)
{
	const int size = 1;
	CFStringRef data[size];
	
	data[0] = CFSTR("flac");
	
	*outArray = CFArrayCreate(kCFAllocatorDefault, (const void**)data, size, &kCFTypeArrayCallBacks);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void FLACAudioFormat::GetFileTypeName(CFStringRef *outName)
{
	CABundleLocker lock;
	CFBundleRef theBundle = NULL;//GetAudioToolboxBundle();
	if(theBundle != NULL)
	{
		*outName = CFCopyLocalizedStringFromTableInBundle(CFSTR("FLAC"), CFSTR("FileTypeNames"), theBundle, CFSTR("A file type name."));
	}
	else
	{
		*outName = CFSTR("FLAC");
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus FLACAudioFormat::GetHFSCodes(UInt32* ioDataSize, void* outPropertyData)
{
	const UInt32 size = 1;
	UInt32 data[size];
	data[0] = kAudioFormatFLAC;
	
	UInt32 numIDs = std::min(size, UInt32(*ioDataSize / sizeof(UInt32)));
	*ioDataSize = numIDs * sizeof(UInt32);
	if (outPropertyData)
	{
		memcpy(outPropertyData, data, *ioDataSize);
	}
	return noErr;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus FLACAudioFormat::GetAvailableFormatIDs(UInt32* ioDataSize, void* outPropertyData)
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
		if(IsSupportedFLACFormat(fmtIDs[i]))
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
OSStatus FLACAudioFormat::GetAvailableStreamDescriptions(UInt32 inFormatID, UInt32* ioDataSize, void* outPropertyData)
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
			if(IsSupportedFLACFormat(inFormatID))
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

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
SInt64 FLACAudioFile::GetNumBytes(void)
{
	SInt64	numBytes = 0;
	// flac files are not all data -- we'll need to subtract out the metadata
	GetDataSource()->GetSize(numBytes);
	
	return(numBytes);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void FLACAudioFile::SetNumBytes(SInt64 /*inNumBytes*/)
{
    // set num bytes is unnecessary as it is not stored in the file. it is accurately obtained just
    // by getting the data source size
    return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
SInt64 FLACAudioFile::GetNumPackets(void)
{
    if (!mFileCompletelyParsed)
    {
        OSStatus err = ScanForPackets(kCompletePacketTable);
        if (err != noErr)
		{
            return (err);
		}
    }
    
    return (GetNumPackets());
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void FLACAudioFile::SetNumPackets(SInt64 inNumPackets)
{
    // the packet count is not settable. It is accurately set by adding packets to the packet table
    return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus FLACAudioFile::GetEstimatedDuration(Float64 * duration)
{
	if (mPacketTableInfo.mNumberValidFrames > 0)
	{
		*duration = mPacketTableInfo.mNumberValidFrames/GetDataFormat().mSampleRate;
	}
	else
	{
		return AudioFileObject::GetEstimatedDuration(duration);
	}
	
	return noErr;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// will return headerSize == 0 if the header is illegal
OSStatus  FLACAudioFile::GetHeaderSize (SInt64 filePosition, UInt32 &headerSize, bool checkFirstHeader)
{
    OSStatus                err = noErr;
    UInt32	                byteCount;
    UInt32                  sampleRateIndex = 0;
    UInt32                  channelConfigurationBits = 0;
    UInt32                  blockingStrategy = 0, blockSizeBits = 0;
	UInt32                  bitDepthBits = 0;
	UInt32					blockingBytes = 0, blockingBits = 0;
	UInt32					tempHeaderSize;
	UInt32					crc8 = 0, tempCRC, currentByte = 0;
	Float64					derivedSampleRate = 0.0;
	Byte *					dataPtr;

			
	headerSize = 0;
    
	CAAutoFree<Byte> FLACHeader(kFLACMaxHeaderSize, true);
	dataPtr = FLACHeader();

	// read the fixed header
	err = GetDataSource()->ReadBytes(fsFromStart, filePosition, kFLACMaxHeaderSize, (Ptr) FLACHeader(), &byteCount);
	if(err != noErr || byteCount != kFLACMaxHeaderSize)
	{
		return err;
	}

	// skip some bits to get to the sample rate index
	++currentByte; // syncword(14) -- we assume we're starting at a sync word

	if ((FLACHeader[currentByte] & 0x02) >> 1) // reserved (1) must == 0
	{
		headerSize = 0;
		return err;
	}

	blockingStrategy = FLACHeader[currentByte] & 0x01; // Blocking strategy (1)
	++currentByte;
	
	// get the block size in frames per packet
	blockSizeBits = (FLACHeader[currentByte] & 0xf0) >> 4; // Block size (4)
	if (blockSizeBits == 0) // illegal
	{
		headerSize = 0;
		return err;
	}
   
	// get the sample rate index
	sampleRateIndex = FLACHeader[currentByte] & 0x0f; // sample frequency index (4)
	if (sampleRateIndex == 15) // illegal
	{
		headerSize = 0;
		return err;
	}
	++currentByte;
	
	// get the channel layout
	channelConfigurationBits = (FLACHeader[currentByte] & 0xf0) >> 4; // channel configuration (4)
	if (channelConfigurationBits > 10) // illegal
	{
		headerSize = 0;
		return err;
	}
	
	// get the bit depth
	bitDepthBits = (FLACHeader[currentByte] & 0x0e) >> 1; // bit depth (3)
	if (bitDepthBits == 3 || bitDepthBits == 7)
	{
		headerSize = 0;
		return err;
	}
	
	// read the reserve bit (1)
	if (FLACHeader[currentByte] & 0x01) // reserved (1) must == 0
	{
		headerSize = 0;
		return err;
	}
	++currentByte;

	// need to read past the sample/frame number
	blockingBits = FLACHeader[currentByte];
	if (blockingBits < 0x80)
	{
		blockingBytes = 1;
	}
	else if (blockingBits >= 0xc0 && blockingBits <= 0xdf)
	{
		blockingBytes = 2;
	}
	else if (blockingBits >= 0xe0 && blockingBits <= 0xef)
	{
		blockingBytes = 3;
	}
	else if (blockingBits >= 0xf0 && blockingBits <= 0xf7)
	{
		blockingBytes = 4;
	}
	else if (blockingBits >= 0xf8 && blockingBits <= 0xfb)
	{
		blockingBytes = 5;
	}
	else if (blockingBits >= 0xfc &&   blockingBits <= 0xfd)
	{
		blockingBytes = 6;
	}
	else if (blockingBits == 0xfe && blockingStrategy == 1)
	{
		blockingBytes = 7;
	}
	else
	{
		blockingBytes = 0; // invalid -- can't happen in the real header, but useful for finding false syncwords
	}
	if (blockingBytes > 0)
	{
		currentByte += blockingBytes;
	}
	else
	{
		headerSize = 0;
		return err;
	}

	if (blockSizeBits == 6)
	{
		++currentByte;
	}
	else if (blockSizeBits == 7)
	{
		currentByte += 2;
	}
	
	if (sampleRateIndex == 12)
	{
		derivedSampleRate = FLACHeader[currentByte] * 1000;
		++currentByte;
	}
	else if (sampleRateIndex == 13)
	{
		UInt32 tempSampleRate = FLACHeader[currentByte];
		++currentByte;
		derivedSampleRate = (tempSampleRate << 8) + FLACHeader[currentByte];
		++currentByte;
	}
	else if (sampleRateIndex == 14)
	{
		UInt32 tempSampleRate = FLACHeader[currentByte];
		++currentByte;
		derivedSampleRate = ((tempSampleRate << 8) + FLACHeader[currentByte]) * 10;
		++currentByte;
	}
	
	// 8 bit crc
	tempHeaderSize = currentByte;
	while(tempHeaderSize--)
	{
		crc8 = FLAC_crc8[crc8 ^ *dataPtr++];
	}

	tempCRC = FLACHeader[currentByte];
	if (tempCRC == crc8)
	{
		++currentByte; // we're good!
	}
	else
	{
		headerSize = 0;
		return err;
	}
	if (checkFirstHeader) // we're really paranoid -- check channels, sample rate, bit depth and blocking strategy as these can't change
	{
		UInt32 bitDepth = gFLACBitDepths[bitDepthBits];
		UInt32 numChannels = (gFLACChannelConfigToLayoutTag[channelConfigurationBits]) & 0x0000ffff;
		Float64 sampleRate = gFLACSampleRates[sampleRateIndex];
		AudioStreamBasicDescription theDataFormat = GetDataFormat();
		
		if (sampleRateIndex > 11) sampleRate = derivedSampleRate;
		if (mBitDepth != bitDepth || theDataFormat.mChannelsPerFrame != numChannels || theDataFormat.mSampleRate != sampleRate)
		{
			headerSize = 0;
			return err;
		}
		// the blocking strategy must also be consistent
		if ((blockingStrategy == 1 && theDataFormat.mFramesPerPacket != 0) || (blockingStrategy == 0 && theDataFormat.mFramesPerPacket == 0))
		{
			headerSize = 0;
			return err;
		}		
	}
	headerSize = currentByte;
    return (err);
}

OSStatus	FLACAudioFile::ScanForSyncWord (SInt64 filePosition, SInt64 endOfAudioData, UInt32 &offsetToSyncWord)
{
	OSStatus    err = noErr;
	UInt32		maxPacketSize, bytesToCheck;
	UInt32		syncFound = 0, tempBits = 0;
	UInt32	    byteCount, currentByte = 0;
	AudioStreamBasicDescription theDataFormat = GetDataFormat();
	offsetToSyncWord = 0;
	
	if (endOfAudioData < filePosition) // check this immediately
	{
		return err; // no sync word
	}
	else if (endOfAudioData - filePosition < kFLACMinHeaderSize)
	{
		return err; // no sync word
	}
	
	// if we know anything about the stream, we can do this intelligently
	maxPacketSize = (theDataFormat.mChannelsPerFrame * (mBitDepth >> 3) * theDataFormat.mFramesPerPacket);
	if (mBitDepth == 20)
	{
		maxPacketSize *= 1.5;
	}
	if (maxPacketSize == 0)
	{
		maxPacketSize = kFLACMaxRawDataSize;
	}
	maxPacketSize += (kFLACMaxHeaderSize + ((theDataFormat.mChannelsPerFrame == 0 ? kFLACMaxNumChannels : theDataFormat.mChannelsPerFrame) * kFLACMaxSubFrameHeaderSize) + kFLACFooterSize);
	
	if (endOfAudioData - filePosition < maxPacketSize)
	{
		maxPacketSize = endOfAudioData - filePosition;
	}
	bytesToCheck = maxPacketSize - kFLACMinHeaderSize;
	
	CAAutoFree<Byte> FLACPacket(maxPacketSize, true);
	
	err = GetDataSource()->ReadBytes(fsFromStart, filePosition, maxPacketSize, (Ptr) FLACPacket(), &byteCount);
	if(err != noErr || byteCount != maxPacketSize)
	{
		return err;
	}

	// There's no way to do this elegantly -- there's no packet size listed in the header
	// so we brute force search one byte at a time
	tempBits = FLACPacket[currentByte];
	++currentByte;
	while(offsetToSyncWord < bytesToCheck && syncFound == 0)
	{
		 // the 14 bit sync word is byte aligned and is 1111 1111 1111 10
		 if (tempBits == 0xff) // the first byte matches
		 {
			tempBits <<= 8;
			tempBits |= FLACPacket[currentByte];
			++currentByte;
			// we cheat and check the reserved bit here as well
			if ((tempBits & 0xfffe) == kFLACSyncWord && VerifySyncWord(filePosition + offsetToSyncWord)) // we have found a sync word
			{
				syncFound = 1;
			}
			else
			{
				// do not increment the current byte -- we already have
				++offsetToSyncWord;
				tempBits &= 0x000000ff;
			}
		 }
		 else
		 {
			++offsetToSyncWord;
			tempBits = FLACPacket[currentByte];
			++currentByte;
		 }
	}
	if(syncFound == 0) // if we don't find anything, set this to the max value 
	{
		offsetToSyncWord = maxPacketSize;
	}
	
	return err;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool FLACAudioFile::VerifySyncWord (SInt64 filePosition)
{
	UInt32 headerSize;
	
	GetHeaderSize (filePosition, headerSize, true); // basically checks the reserved bits
	if (headerSize == 0)
	{
		return false;
	}
	
	return true;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus	FLACAudioFile::ScanForPackets(SInt64  inToPacketCount)
{
    OSStatus                            err = noErr;
    SInt64                              pTotalAudioByteCount = 0;
    AudioStreamPacketDescription		curTableEntry = {0};
    Boolean                             done = false;
    UInt32	                            byteCount;
    SInt64                              filePos = 0;
    SInt64                              oldPos;
    SInt64                              packetCount = 0;
	SInt64                              endOfAudioData = 0;
	CompressedPacketTable               *packetTable = NULL;
    SInt64                              packetsToAdd = 0;
	UInt32								headerSize = 0, offsetToSyncWord = 0, currentByte = 0;

    packetTable = GetPacketTable(true); // get table to determine the size or create a new one
    packetCount = GetNumPackets();     // how many packets are already in the table?

    if (inToPacketCount != kCompletePacketTable)
    {
        // is a partial update requested?
        if (packetCount >= inToPacketCount)
		{
            return noErr; // packet table is already large enough
		}
    }

    err = GetDataSource()->GetSize(endOfAudioData);
	FailIf(err != noErr, Bail, "GetSize failed");	
					
    if (packetCount == 0)
    {
        // this is the first time the table is being written to
        SetMaximumPacketSize(0);
        filePos = GetDataOffset(); // set file pointer to the beginning of the data
    }
    else
    {
		filePos = (*packetTable)[packetCount-1].mStartOffset + (*packetTable)[packetCount-1].mDataByteSize;
    }
    
    packetsToAdd = (inToPacketCount == kCompletePacketTable) ? kCompletePacketTable : inToPacketCount - GetNumPackets();
    
	{
		CAAutoFree<Byte> FLACHeader(kFLACMaxHeaderSize, true);

		// data offset should already be set at a syncWord
		while (!done)
		{
			curTableEntry.mStartOffset = filePos;
			
			// read the flac header
			err = GetDataSource()->ReadBytes(fsFromStart, filePos, kFLACMaxHeaderSize, (Ptr) FLACHeader(), &byteCount);
			// reset currentByte
			currentByte = 0;
			if(err != noErr || byteCount != kFLACMaxHeaderSize)
			{
				goto Bail;
			}
																					 
			// verify that we are at the start of a valid FLAC frame - syncWord is 1111 1111 1111 10 bits
			UInt32 tLong = FLACHeader[currentByte];
			++currentByte;
			tLong <<= 8;
			tLong += (FLACHeader[currentByte]);
			tLong &= kFLACSyncMask;
			FailIf(tLong != kFLACSyncWord, Bail, "FLAC - Syncword not found");
			
			// we don't rigorously test the header as we're in theory at the start of the header
			GetHeaderSize (filePos, headerSize, false);
			
			// If we're on the last packet, this will take us to the end of the file
			ScanForSyncWord(filePos + headerSize, endOfAudioData, offsetToSyncWord);
			
			curTableEntry.mDataByteSize = headerSize + offsetToSyncWord;

			if (curTableEntry.mDataByteSize > GetMaximumPacketSize())
			{
				SetMaximumPacketSize(curTableEntry.mDataByteSize);
			}
			
			// verify that the entire packet is actually in the file
			if (filePos + curTableEntry.mDataByteSize > endOfAudioData)
			{
				err = noErr;
				mFileCompletelyParsed = true;
				goto Finished;
			}
			
			filePos += curTableEntry.mDataByteSize;

			pTotalAudioByteCount += curTableEntry.mDataByteSize;
			
			// there could be other data after the audio data so remember where the we start
			// looking for the next sync word
			oldPos = filePos;																							
			
			filePos = curTableEntry.mStartOffset + curTableEntry.mDataByteSize;
			if (filePos >= endOfAudioData)
			{
				err = noErr;
				mFileCompletelyParsed = true;
				done = true;
			}
			else
			{
				FailIf(err != noErr, Bail, "GetFPosForNextSyncWord - Could not find next syncWord");
				
				// the packetSize is a calculation of the audio data and does not include any allowed
				// ancillary data so update the packetSize if it's actually larger than the audio data
				// before appending to the table
				// Yes, flac doesn't allow this. However, we're paranoid.
				if (filePos > oldPos)
				{
					curTableEntry.mDataByteSize += (filePos - oldPos);
				}
			}

			AppendPacket(curTableEntry);
			packetCount++;
			
			// is this a partial packet table creation?
			if (packetsToAdd != kCompletePacketTable)
			{
				packetsToAdd--;
			}
			
			if (packetsToAdd == 0)
			{
				done = true;
			}
		}
	}
Finished:    
Bail:
	if (err == eofErr)
	{
		err = noErr;
	}
    return (err);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus	FLACAudioFile::GetPropertyInfo(  AudioFilePropertyID		inPropertyID,
                                            UInt32					*outDataSize,
                                            UInt32					*isWritable)
{
    OSStatus		err = noErr;
    
    switch (inPropertyID)
    {
		case kAudioFilePropertyPacketTableInfo:
            if (outDataSize)
			{
				*outDataSize = sizeof(AudioFilePacketTableInfo);
			}
			if (isWritable)
			{
				*isWritable = 1;
			}
            break;
        default:
			return AudioFileObject::GetPropertyInfo(inPropertyID, outDataSize, isWritable);
	}
	return err;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus	FLACAudioFile::GetProperty(  AudioFilePropertyID		inPropertyID,
                                        UInt32					*ioDataSize,
                                        void					*ioPropertyData)
{
    OSStatus		err = noErr;
	
    switch (inPropertyID)
    {		
 		case kAudioFilePropertyPacketTableInfo :
		{
            if (!*ioDataSize || *ioDataSize < sizeof(AudioFilePacketTableInfo))
			{
				return kAudioFileBadPropertySizeError;
			}

			AudioFilePacketTableInfo* info = (AudioFilePacketTableInfo*)ioPropertyData;
			info->mNumberValidFrames = mPacketTableInfo.mNumberValidFrames;
			info->mPrimingFrames = 0;
			info->mRemainderFrames = mPacketTableInfo.mRemainderFrames;
			*ioDataSize = sizeof(AudioFilePacketTableInfo);
			
            break;
		}
		case kAudioFilePropertyPacketSizeUpperBound:
		{
            if (*ioDataSize != sizeof(UInt32))
			{
                return kAudioFileBadPropertySizeError;
			}
			if (!mFileCompletelyParsed)
			{
				UInt32 maxPacketSize;
				AudioStreamBasicDescription theDataFormat = GetDataFormat();

				maxPacketSize = (theDataFormat.mChannelsPerFrame * (mBitDepth >> 3) * theDataFormat.mFramesPerPacket);
				if (mBitDepth == 20)
				{
					maxPacketSize *= 1.5;
				}
				if (maxPacketSize == 0)
				{
					maxPacketSize = kFLACMaxRawDataSize;
				}
				maxPacketSize += (kFLACMaxHeaderSize + (theDataFormat.mChannelsPerFrame == 0 ? kFLACMaxNumChannels : theDataFormat.mChannelsPerFrame) * kFLACMaxSubFrameHeaderSize + kFLACFooterSize);

				*(UInt32 *)ioPropertyData = maxPacketSize;
			}
			else
			{
				return AudioFileObject::GetProperty(inPropertyID, ioDataSize, ioPropertyData);
			}
			break;
		}
		case kAudioFilePropertyMaximumPacketSize :
		{
            if (*ioDataSize != sizeof(UInt32))
			{
                return kAudioFileBadPropertySizeError;
			}
			
			if (!mFileCompletelyParsed)
			{
				// must create the packet table first so object can return the correct value
                err = ScanForPackets(kCompletePacketTable);
                if (err != noErr)
				{
                    return (err);
				}
			}
			// now a valid size can be returned
			return AudioFileObject::GetProperty(inPropertyID, ioDataSize, ioPropertyData);
			break; // avoids warning on dumb compilers
		}

        default:
			return AudioFileObject::GetProperty(inPropertyID, ioDataSize, ioPropertyData);
			break; // avoids warning on dumb compilers
	}

 	return err;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Needed for encoding. The encoder will produce a magic cookie that can be accepted by QuickTime. We don't need all
// that for when encoding .flac, but we need to set up the stream info 
void FLACAudioFile::ConvertCookieToStreamInfo(UInt32 inMagicCookieDataByteSize, const void* inMagicCookieData, FLAC_StreamInfo * theStreamInfo)
{
	FLAC_StreamInfo * tempConfig;
	UInt32 cookieOffset = 0;
	
	// We might get a cookie with atoms -- strip them off
	if (inMagicCookieDataByteSize > sizeof(FLAC_StreamInfo))
	{
		if(CFSwapInt32BigToHost(((AudioFormatAtom *)inMagicCookieData)->atomType) == 'frma')
		{
			cookieOffset = (sizeof(AudioFormatAtom) + sizeof(FullAtomHeader));
		}
	} 
	// Finally, parse the cookie for the bits we care about -- what we receive will be in big endian format
	tempConfig = (FLAC_StreamInfo *)(&((Byte *)(inMagicCookieData))[cookieOffset]);
	theStreamInfo->min_blocksize	= CFSwapInt32BigToHost( tempConfig->min_blocksize );
	theStreamInfo->max_blocksize	= CFSwapInt32BigToHost( tempConfig->max_blocksize );
	theStreamInfo->min_framesize	= CFSwapInt32BigToHost( tempConfig->min_framesize );
	theStreamInfo->max_framesize	= CFSwapInt32BigToHost( tempConfig->max_framesize );
	theStreamInfo->sample_rate		= CFSwapInt32BigToHost( tempConfig->sample_rate );
	theStreamInfo->channels			= CFSwapInt32BigToHost( tempConfig->channels );
	theStreamInfo->bits_per_sample	= CFSwapInt32BigToHost( tempConfig->bits_per_sample );
	theStreamInfo->total_samples	= CFSwapInt64BigToHost( tempConfig->total_samples );
	theStreamInfo->md5sum[0]		= tempConfig->md5sum[0];
	theStreamInfo->md5sum[1]		= tempConfig->md5sum[1];
	theStreamInfo->md5sum[2]		= tempConfig->md5sum[2];
	theStreamInfo->md5sum[3]		= tempConfig->md5sum[3];
	theStreamInfo->md5sum[4]		= tempConfig->md5sum[4];
	theStreamInfo->md5sum[5]		= tempConfig->md5sum[5];
	theStreamInfo->md5sum[6]		= tempConfig->md5sum[6];
	theStreamInfo->md5sum[7]		= tempConfig->md5sum[7];
	theStreamInfo->md5sum[8]		= tempConfig->md5sum[8];
	theStreamInfo->md5sum[9]		= tempConfig->md5sum[9];
	theStreamInfo->md5sum[10]		= tempConfig->md5sum[10];
	theStreamInfo->md5sum[11]		= tempConfig->md5sum[11];
	theStreamInfo->md5sum[12]		= tempConfig->md5sum[12];
	theStreamInfo->md5sum[13]		= tempConfig->md5sum[13];
	theStreamInfo->md5sum[14]		= tempConfig->md5sum[14];
	theStreamInfo->md5sum[15]		= tempConfig->md5sum[15];
}
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus	FLACAudioFile::SetProperty(
                                    AudioFilePropertyID		inPropertyID,
                                    UInt32					inDataSize,
                                    const void				*inPropertyData)
{
    OSStatus		err = noErr;
	
    switch (inPropertyID)
    {
		case kAudioFilePropertyPacketTableInfo :
		{
			if (inDataSize < sizeof(Float64))
			{
				return kAudioFileBadPropertySizeError;
			}
			mPacketTableInfoSet = true;
			AudioFilePacketTableInfo* info = (AudioFilePacketTableInfo*)inPropertyData;
			mPacketTableInfo.mRemainderFrames = info->mRemainderFrames; // the only number we don't have
            break;
		}
		case kAudioFilePropertyMagicCookieData:
		{            
			err = SetMagicCookieData(inDataSize, inPropertyData);
			if (mPacketTableInfoSet) // we're at the tail end of an encode
			{
				// we cheat like all heck here -- we write out the cookie to a fixed location
				// this is the last thing we do to the file
				FLAC_StreamInfo theCookie;
				UInt32 theCookieSize = sizeof(FLAC_StreamInfo);
				UInt32 theNumChannels = 0, theBitDepth = 0;
				Byte theStreamInfo[kStreamInfoSize];

				ConvertCookieToStreamInfo(inDataSize, inPropertyData, &theCookie);

				theStreamInfo[0] = 0x80; // we're last block, type is streaminfo
				// size == 34
				theStreamInfo[1] = theStreamInfo[2] = 0;
				theStreamInfo[3] = 34;
				// Min block size
				theStreamInfo[4] = (theCookie.min_blocksize & 0x0000ff00) >> 8;
				theStreamInfo[5] = (theCookie.min_blocksize & 0x000000ff);
				// Max block size
				theStreamInfo[6] = (theCookie.max_blocksize & 0x0000ff00) >> 8;
				theStreamInfo[7] = (theCookie.max_blocksize & 0x000000ff);
				
				// Min packet size
				theStreamInfo[8] = (theCookie.min_framesize & 0x00ff0000) >> 16;
				theStreamInfo[9] = (theCookie.min_framesize & 0x0000ff00) >> 8;
				theStreamInfo[10] = (theCookie.min_framesize & 0x000000ff);
				// Max packet size
				theStreamInfo[11] = (theCookie.max_framesize & 0x00ff0000) >> 16;
				theStreamInfo[12] = (theCookie.max_framesize & 0x0000ff00) >> 8;
				theStreamInfo[13] = (theCookie.max_framesize & 0x000000ff);
				
				// Sample rate -- here we jump byte boundaries (20 bits)
				theStreamInfo[14] = (theCookie.sample_rate & 0x000ff000) >> 12;
				theStreamInfo[15] = (theCookie.sample_rate & 0x00000ff0) >> 4;
				theStreamInfo[16] = (theCookie.sample_rate & 0x0000000f) << 4;
				
				// number of channels - 1 (3 bits)
				theNumChannels = theCookie.channels - 1;
				theStreamInfo[16] |= ((theNumChannels & 0x00000007) << 1);
				
				// bit depth - 1 (5 bits)
				theBitDepth = theCookie.bits_per_sample - 1;
				theStreamInfo[16] |= ((theBitDepth & 0x00000010) >> 4);
				theStreamInfo[17] = ((theBitDepth & 0x0000000f) << 4);
				
				// number of samples in stream
				theStreamInfo[17] |= ((theCookie.total_samples & 0x0000000f00000000LL) >> 32);
				theStreamInfo[18] = (theCookie.total_samples &   0x00000000ff000000LL) >> 24;
				theStreamInfo[19] = (theCookie.total_samples &   0x0000000000ff0000LL) >> 16;
				theStreamInfo[20] = (theCookie.total_samples &   0x000000000000ff00LL) >> 8;
				theStreamInfo[21] = (theCookie.total_samples &   0x00000000000000ffLL);
				
				for (UInt32 i = 22; i < kStreamInfoSize; ++i)
				{
					theStreamInfo[i] = theCookie.md5sum[i-22];
				}
				err = GetDataSource()->WriteBytes(fsFromStart, 4, kStreamInfoSize, theStreamInfo, &theCookieSize);
			}
			break;
		}

		default:
			return AudioFileObject::SetProperty(inPropertyID, inDataSize, inPropertyData);
			break; // avoids warning on dumb compilers
	}
	return err;
}
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus FLACAudioFile::GetMagicCookieDataSize(	UInt32					*outDataSize,
												UInt32					*isWritable)
{
	if (isWritable)
		*isWritable = CanWrite();
	
	OSStatus	err = noErr;
	
	if (outDataSize)
	{
		if (mMagicCookie)
		{
			*outDataSize = mMagicCookieSize;
		}
		else
		{
			err = kAudioFileUnspecifiedError; // it shouldn't be possible to have no cookie if file wasn't open for write
		}
	}
		
	return err;
}
	                      
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus FLACAudioFile::GetMagicCookieData(	UInt32					*ioDataSize,
											void					*ioPropertyData)
{
	if (ioPropertyData == NULL)
	{
		return kAudioFileUnspecifiedError;
	}
	if (ioDataSize == NULL)
	{
		return kAudioFileBadPropertySizeError;
	}

	OSStatus	err = noErr;
	
	if (mMagicCookie)
	{
		if (*ioDataSize >= mMagicCookieSize) 
		{
			memcpy(ioPropertyData, mMagicCookie, mMagicCookieSize);
			*ioDataSize = mMagicCookieSize;
		}
		else
		{	
			err = kAudioFileBadPropertySizeError;
		}
	}
	else
	{
		err = kAudioFileUnspecifiedError; // it shouldn't be possible to have no cookie if file wasn't open for write
	}
	
	return err;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus FLACAudioFile::SetMagicCookieData(		UInt32					inDataSize,
												const void				*inPropertyData)
{
	if (mMagicCookie == NULL)
	{
		mMagicCookieSize = inDataSize;
		mMagicCookie = malloc (mMagicCookieSize);
		memcpy(mMagicCookie, inPropertyData, mMagicCookieSize);
	}
	
	return noErr;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus FLACAudioFile::GetFormatListInfo(	UInt32			&outDataSize,
											UInt32			&outWritable)
{
	AudioFormatInfo					fInfo;
	AudioStreamBasicDescription		asbd = GetDataFormat();
	
	memset(&fInfo.mASBD, 0 , sizeof(fInfo.mASBD));
	fInfo.mASBD.mFormatID = asbd.mFormatID;
	fInfo.mMagicCookie = mMagicCookie;
	fInfo.mMagicCookieSize = mMagicCookieSize;
	
	UInt32 size = sizeof(AudioStreamBasicDescription) + sizeof(UInt32) + mMagicCookieSize;
	OSStatus err = AudioFormatGetPropertyInfo (kAudioFormatProperty_FormatList, size, &fInfo, &outDataSize);
	// If the audio format item doesn't support a format list for this format, intervene
	if (err) 
	{
		outDataSize = sizeof(AudioFormatListItem);
		outWritable = 0;
	}
	
	return noErr;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus FLACAudioFile::GetFormatList(	UInt32						&ioDataSize,
										AudioFormatListItem			*ioPropertyData)
{
	AudioFormatInfo					fInfo;
	AudioStreamBasicDescription		asbd = GetDataFormat();
	
	memset(&fInfo.mASBD, 0 , sizeof(fInfo.mASBD));
	
	fInfo.mASBD.mFormatID = asbd.mFormatID;
	fInfo.mMagicCookie = mMagicCookie;
	fInfo.mMagicCookieSize = mMagicCookieSize;
	
	UInt32 size = sizeof(AudioStreamBasicDescription) + sizeof(UInt32) + mMagicCookieSize;
	OSStatus err = AudioFormatGetProperty(	kAudioFormatProperty_FormatList, size, &fInfo, &ioDataSize, ioPropertyData);
	// if we fail, fill the format list out with what we already have
	if (err) 
	{
		AudioFormatListItem*	fList = ioPropertyData;
		
		fList->mASBD = asbd;
		fList->mChannelLayoutTag = mChannelLayoutTag;
		ioDataSize = sizeof(AudioFormatListItem);
	}
	
	return noErr;
}

