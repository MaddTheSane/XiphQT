//
//  OggAudioFile.hpp
//  Xiph Audio Components
//
//  Created by C.W. Betts on 5/5/16.
//  Copyright © 2016 C.W. Betts. All rights reserved.
//

#ifndef OggAudioFile_hpp
#define OggAudioFile_hpp

#include "AudioFileFormat.h"
#include "ogg/ogg.h"

#define kAudioFileOggType	'OggS'
#define kAudioFormatOgg		'OggS'
#define kOggStartBytes		'OggS'
#define kOggHeaderFixedDataSize 16
#define kValidFramesUninitialized	-1

class OggAudioFormat : public AudioFileFormat
{
public:
	OggAudioFormat() : AudioFileFormat('OggS') {}
	
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

class  OggAudioFile : public AudioFileObject
{
private:
	AudioChannelLayoutTag			mChannelLayoutTag;
	UInt32							mBitDepth;
	UInt32							mMagicCookieSize;
	void*							mMagicCookie;
	Byte							mFixedHeader[kOggHeaderFixedDataSize];
	bool							mFileCompletelyParsed;
	bool							mPacketTableInfoSet;
	AudioFilePacketTableInfo		mPacketTableInfo;
public:
	
	OggAudioFile ()
	: AudioFileObject(kAudioFileOggType),
	mChannelLayoutTag(0),
	mBitDepth(0),
	mMagicCookieSize(0),
	mMagicCookie(NULL),
	mFileCompletelyParsed(false),
	mPacketTableInfoSet(false)
	{
		memset (mFixedHeader, 0, kOggHeaderFixedDataSize);
		mPacketTableInfo.mNumberValidFrames = kValidFramesUninitialized;		// init
		mPacketTableInfo.mRemainderFrames = 0;									// init
		mPacketTableInfo.mPrimingFrames = 0;									// init
	}
	
	~OggAudioFile();

	virtual OSStatus ReadPackets(Boolean						inUseCache,
								 UInt32							*outNumBytes,
								 AudioStreamPacketDescription	*outPacketDescriptions,
								 SInt64							inStartingPacket,
								 UInt32  						*ioNumPackets,
								 void							*outBuffer);
	
	virtual OSStatus WritePackets(Boolean								inUseCache,
								  UInt32								inNumBytes,
								  const AudioStreamPacketDescription	*inPacketDescriptions,
								  SInt64								inStartingPacket,
								  UInt32								*ioNumPackets,
								  const void							*inBuffer);
	
	
	virtual OSStatus GetPropertyInfo(AudioFilePropertyID	inPropertyID,
									 UInt32					*outDataSize,
									 UInt32					*isWritable);
	
	virtual OSStatus GetProperty(AudioFilePropertyID	inPropertyID,
								 UInt32					*ioDataSize,
								 void					*ioPropertyData);
	
	virtual	OSStatus SetProperty(AudioFilePropertyID	inPropertyID,
								 UInt32					inDataSize,
								 const void				*inPropertyData);

	virtual OSStatus OpenFromDataSource(SInt8 inPermissions);
	
	virtual OSStatus Create(CFURLRef							inFileRef,
							const AudioStreamBasicDescription	*inFormat);
	
	virtual OSStatus InitializeDataSource(const AudioStreamBasicDescription		*inFormat,
										  UInt32								inFlags);
	
	virtual OSStatus UpdateSize();
	
	virtual OSStatus GetChannelLayoutSize(UInt32	*outDataSize,
										  UInt32	*isWritable);
	
	virtual OSStatus GetChannelLayout(UInt32				*ioDataSize,
									  AudioChannelLayout	*ioRegionList);
	
	virtual OSStatus GetMagicCookieDataSize(UInt32	*outDataSize,
											UInt32	*isWritable);
	
	virtual OSStatus GetMagicCookieData(UInt32	*ioDataSize,
										void	*ioPropertyData);
	
	virtual OSStatus SetMagicCookieData(UInt32		inDataSize,
										const void	*inPropertyData);
	
	virtual OSStatus GetFormatListInfo(UInt32	&outDataSize,
									   UInt32	&outWritable);
	
	virtual OSStatus GetFormatList(UInt32				&ioDataSize,
								   AudioFormatListItem	*ioPropertyData);
	
	/* Other Helper Methods: (some may not be necessary depending on how things are refactored) */
	
	virtual OSStatus 	ParseAudioFile();
	virtual Boolean 	IsDataFormatSupported(const AudioStreamBasicDescription	*inFormat);
	
	virtual OSStatus	CreatePacketTable();
	virtual OSStatus	ScanForPackets(SInt64  inToPacketCount);
	
	OSStatus 	ParseFirstPacketHeader(SInt64 filePosition, AudioStreamBasicDescription &format);
	OSStatus 	ParseStreamInfo(SInt64 filePosition, AudioStreamBasicDescription	&format);
	OSStatus	GetHeaderSize(SInt64 filePosition, UInt32 &headerSize, bool checkFirstHeader);
	OSStatus	ScanForSyncWord(SInt64 filePosition, SInt64 endOfAudioData, UInt32 &offsetToSyncWord);
	bool		VerifySyncWord(SInt64 filePosition);
	//void		ConvertCookieToStreamInfo(UInt32 inMagicCookieDataByteSize, const void* inMagicCookieData, FLAC_StreamInfo * theStreamInfo);
	
	SInt64      GetNumPackets(void);
	void        SetNumPackets(SInt64 inNumPackets);
	SInt64		GetNumBytes(void);
	void		SetNumBytes(SInt64 inNumBytes);
	OSStatus	GetEstimatedDuration(Float64 * duration);
};


#endif /* OggAudioFile_hpp */
