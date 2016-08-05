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
#if !defined(__COREAUDIO_USE_FLAT_INCLUDES__)
#else
	#include <ConditionalMacros.h>
#endif
#include "AudioFileFormat.h"

#define kFLACMaxNumChannels 8
// the size of the fixed header, variable header and error check in bytes
#define		kFLACMaxHeaderSize					16
#define		kFLACMinHeaderSize					6
#define		kFLACFooterSize						2
#define		kFLACMaxSubFrameHeaderSize			8
#define		kFLACMaxRawDataSize					(8 * 4 * 32768) // max channels * max sample depth in bytes * max number of frames per packet
#define		kFLACHeaderFixedDataSize			4

#define	kFLACID		'FLAC'
#define	kflacID		'flac'
#define kAudioFormatFLAC	'flac'
#define kAudioFileFLACType 'flac'

#define	kFLACSyncMask	0xfffc // 14 bits 1111 1111 1111 1100
#define kFLACSyncWord	0xfff8 // 14-bits 1111 1111 1111 1000

#define kFront					1
#define kRear					2
#define ID_SCE					0
#define ID_CPE					1
#define ID_LFE					3
#define ID_PCE					5
#define	kMaxPCESizeInBits		385		 

#define TEST_MULTI_RAW_DATA_BLOCKS 0

#define kFLACStartBytes 'fLaC'

#define		kValidFramesUninitialized	-1

// *** Remove this when we move to a component -- use the def in format.h
typedef struct {
	UInt32 min_blocksize, max_blocksize;
	UInt32 min_framesize, max_framesize;
	UInt32 sample_rate;
	UInt32 channels;
	UInt32 bits_per_sample;
	UInt64 total_samples;
	Byte md5sum[16];
} FLAC_StreamInfo; 

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class  FLACAudioFile : public AudioFileObject
{
private:
	AudioChannelLayoutTag			mChannelLayoutTag;
	UInt32							mBitDepth;
	UInt32							mMagicCookieSize;
	void*							mMagicCookie;
	Byte							mFixedHeader[kFLACHeaderFixedDataSize];
    bool							mFileCompletelyParsed;
	bool							mPacketTableInfoSet;
	AudioFilePacketTableInfo		mPacketTableInfo;
public:    

	FLACAudioFile ()
		: AudioFileObject(kAudioFileFLACType),
          mChannelLayoutTag(0),
		  mBitDepth(0),
		  mMagicCookieSize(0),
		  mMagicCookie(NULL),
          mFileCompletelyParsed(false),
		  mPacketTableInfoSet(false)
        { 
			memset (mFixedHeader, 0, kFLACHeaderFixedDataSize); 
			mPacketTableInfo.mNumberValidFrames = kValidFramesUninitialized;		// init
			mPacketTableInfo.mRemainderFrames = 0;									// init
			mPacketTableInfo.mPrimingFrames = 0;									// init
		}

    ~FLACAudioFile();

        
    virtual OSStatus ReadPackets(	Boolean							inUseCache,
                                    UInt32							*outNumBytes,
                                    AudioStreamPacketDescription	*outPacketDescriptions,
                                    SInt64							inStartingPacket, 
                                    UInt32  						*ioNumPackets, 
                                    void							*outBuffer);

    virtual OSStatus WritePackets(	Boolean								inUseCache,
                                    UInt32								inNumBytes,
                                    const AudioStreamPacketDescription	*inPacketDescriptions,
                                    SInt64								inStartingPacket, 
                                    UInt32								*ioNumPackets, 
                                    const void							*inBuffer);


    virtual OSStatus GetPropertyInfo(   AudioFilePropertyID		inPropertyID,
                                        UInt32					*outDataSize,
                                        UInt32					*isWritable);

    virtual OSStatus GetProperty    (   AudioFilePropertyID		inPropertyID,
                                        UInt32					*ioDataSize,
                                        void					*ioPropertyData);
                                                                    
	virtual	OSStatus SetProperty    (
										AudioFilePropertyID		inPropertyID,
										UInt32					inDataSize,
										const void				*inPropertyData);

	virtual OSStatus OpenFromDataSource(	SInt8  			inPermissions);

	virtual OSStatus Create(CFURLRef							inFileRef,
							const AudioStreamBasicDescription	*inFormat);
						
	virtual OSStatus InitializeDataSource(	const AudioStreamBasicDescription	*inFormat,
                                            UInt32								inFlags);
						
	virtual OSStatus UpdateSize();

	virtual OSStatus GetChannelLayoutSize(		UInt32							*outDataSize,
												UInt32							*isWritable);
	                      
	virtual OSStatus GetChannelLayout(			UInt32							*ioDataSize,
												AudioChannelLayout				*ioRegionList);

	virtual OSStatus GetMagicCookieDataSize(	UInt32					*outDataSize,
												UInt32					*isWritable);
	                      
	virtual OSStatus GetMagicCookieData(		UInt32					*ioDataSize,
												void					*ioPropertyData);

	virtual OSStatus SetMagicCookieData(		UInt32					inDataSize,
												const void				*inPropertyData);

	virtual OSStatus GetFormatListInfo(	UInt32				&outDataSize,
										UInt32				&outWritable);
										
	virtual OSStatus GetFormatList(	UInt32							&ioDataSize,
									AudioFormatListItem				*ioPropertyData);
	
/* Other Helper Methods: (some may not be necessary depending on how things are refactored) */
	
	virtual OSStatus 	ParseAudioFile();	
	virtual Boolean 	IsDataFormatSupported(const AudioStreamBasicDescription	*inFormat);
	
    virtual OSStatus	CreatePacketTable();	
    virtual OSStatus	ScanForPackets(SInt64  inToPacketCount);
	
            OSStatus 	ParseFirstPacketHeader	(SInt64 filePosition, AudioStreamBasicDescription &format);
            OSStatus 	ParseStreamInfo	(SInt64 filePosition, AudioStreamBasicDescription	&format);
			OSStatus	GetHeaderSize (SInt64 filePosition, UInt32 &headerSize, bool checkFirstHeader);
			OSStatus	ScanForSyncWord (SInt64 filePosition, SInt64 endOfAudioData, UInt32 &offsetToSyncWord);
			bool		VerifySyncWord (SInt64 filePosition);
			void		ConvertCookieToStreamInfo(UInt32 inMagicCookieDataByteSize, const void* inMagicCookieData, FLAC_StreamInfo * theStreamInfo);

#if	TEST_MULTI_RAW_DATA_BLOCKS
			OSStatus	GetFullFLACPacket(	UInt32					*ioDataSize,
											void					*ioPropertyData);
#endif

            SInt64      GetNumPackets(void);
            void        SetNumPackets(SInt64 inNumPackets);
			SInt64		GetNumBytes(void);
			void		SetNumBytes(SInt64 inNumBytes);
			OSStatus	GetEstimatedDuration(Float64 * duration);

};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class FLACAudioFormat : public AudioFileFormat
{
public:
	FLACAudioFormat() : AudioFileFormat('flac') {}
	
	// create an AudioFileObject for this format type.
	virtual AudioFileObject* New(); 
	//virtual AudioFileStreamObject* NewStream(); 
	
	// return true if file is of this format type
	virtual Boolean ExtensionIsThisFormat(CFStringRef inExtension);
	
	// return true if file is of this format type
	virtual UncertainResult FileDataIsThisFormat(UInt32 inDataByteSize, const void* inData);

	virtual void GetExtensions(CFArrayRef *outArray);
	virtual void GetFileTypeName(CFStringRef *outName);
	virtual OSStatus GetHFSCodes(UInt32* ioDataSize, void* outPropertyData);
	virtual OSStatus GetAvailableFormatIDs(UInt32* ioDataSize, void* outPropertyData);
	virtual OSStatus GetAvailableStreamDescriptions(UInt32 inFormatID, UInt32* ioDataSize, void* outPropertyData);
};
