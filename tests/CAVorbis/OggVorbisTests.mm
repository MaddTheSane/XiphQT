/*
 *  OggVorbisTests.cpp
 *
 *    CAOggVorbisDecoder class test cases.
 *
 *
 *  Copyright (c) 2007  Arek Korbik
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
 *  Last modified: $Id: OggVorbisTests.cpp 12814 2007-03-27 22:09:32Z arek $
 *
 */


#include "OggVorbisTests.h"
#include <iostream>
#include <fstream>


@implementation OggVorbisTests 

-(void)setUp
{
	[super setUp];
	mOggDecoder = new CAOggVorbisDecoder();
}

-(void)tearDown
{
	delete mOggDecoder;
	mOggDecoder = NULL;
	
	[super tearDown];
}

- (void)testAppendUnitialized
{
	UInt32 bytes = 0;
	UInt32 packets = 0;
	
	Boolean appended = false;
	
	try {
		mOggDecoder->AppendInputData(NULL, bytes, packets, NULL);
		appended = true;
	} catch (...) {
	};
	
	XCTAssert(appended == false);
}

- (void)testInitCookie
{
	std::ifstream f_in;
	char cookie[8192];
	@autoreleasepool {
		NSBundle *ourBundle = [NSBundle bundleForClass:[self class]];
		NSString *filepath = [ourBundle pathForResource:@"vorbis.ogg.cookie" ofType:nil];
		
		f_in.open([filepath fileSystemRepresentation], std::ios::in);
	}
	
	XCTAssert(f_in.good());
	
	f_in.read(cookie, 8192);
	f_in.close();
	
	AudioStreamBasicDescription in_dsc = {44100.0, 'XoVs', 0, 0, 0, 0, 2, 0, 0};
	AudioStreamBasicDescription out_dsc = {44100.0, kAudioFormatLinearPCM,
	 kAudioFormatFlagsNativeFloatPacked,
	 8, 1, 8, 2, 32, 0};
	
	mOggDecoder->Initialize(&in_dsc, &out_dsc, NULL, 0);
	
	XCTAssert(!mOggDecoder->IsInitialized());
	
	XCTAssertNoThrow(mOggDecoder->Uninitialize());
	
	XCTAssert(!mOggDecoder->IsInitialized());
	
	XCTAssertNoThrow(mOggDecoder->Initialize(NULL, NULL, cookie, 4216));
	
	XCTAssert(mOggDecoder->IsInitialized());
}

- (void)testAppendSingle
{
	std::ifstream f_in;
	char buffer[16384];
	
	@autoreleasepool {
		NSBundle *ourBundle = [NSBundle bundleForClass:[self class]];
		NSString *filepath = [ourBundle pathForResource:@"vorbis.ogg.cookie" ofType:nil];
		
		f_in.open([filepath fileSystemRepresentation], std::ios::in);
	}
	
	XCTAssert(f_in.good());
	
	f_in.read(buffer, 16384);
	f_in.close();
	
	AudioStreamBasicDescription in_dsc = {44100.0, 'XoVs', 0, 0, 0, 0, 2, 0, 0};
	AudioStreamBasicDescription out_dsc = {44100.0, kAudioFormatLinearPCM,
	 kAudioFormatFlagsNativeFloatPacked,
	 8, 1, 8, 2, 32, 0};
	
	mOggDecoder->Initialize(&in_dsc, &out_dsc, buffer, 4216);
	XCTAssert(mOggDecoder->IsInitialized());
	
	@autoreleasepool {
		NSBundle *ourBundle = [NSBundle bundleForClass:[self class]];
		NSString *filepath = [ourBundle pathForResource:@"vorbis.ogg.data" ofType:nil];
		
		f_in.open([filepath fileSystemRepresentation], std::ios::in);
	}
	
	XCTAssert(f_in.good());
	
	f_in.read(buffer, 16384);
	f_in.close();
	
	UInt32 bytes = 4152;
	UInt32 packets = 1;
	
	Boolean appended = false;
	
	AudioStreamPacketDescription pd = {0, 6720, 4152};
	
	try {
		mOggDecoder->AppendInputData(buffer, bytes, packets, &pd);
		appended = true;
	} catch (...) {
	};
	
	XCTAssert(appended == true);
	
	packets = 6720;
	bytes = 8 * packets;
	char audio[bytes];
	UInt32 ac_ret = kAudioCodecProduceOutputPacketFailure;
	
	try {
	 ac_ret = mOggDecoder->ProduceOutputPackets(audio, bytes, packets, NULL);
	} catch (...) {
	 bytes = 0;
	 packets = 0;
	};
	
	XCTAssert(ac_ret == kAudioCodecProduceOutputPacketSuccess &&
			  bytes == 53760 && packets == 6720);
}

- (void)testAppendMultiple
{
	std::ifstream f_in;
	char buffer[16384];
	
	@autoreleasepool {
		NSBundle *ourBundle = [NSBundle bundleForClass:[self class]];
		NSString *filepath = [ourBundle pathForResource:@"vorbis.ogg.cookie" ofType:nil];
		
		f_in.open([filepath fileSystemRepresentation], std::ios::in);
	}
	
	XCTAssert(f_in.good());
	
	f_in.read(buffer, 16384);
	f_in.close();
	
	AudioStreamBasicDescription in_dsc = {44100.0, 'XoVs', 0, 0, 0, 0, 2, 0, 0};
	AudioStreamBasicDescription out_dsc = {44100.0, kAudioFormatLinearPCM,
		kAudioFormatFlagsNativeFloatPacked,
		8, 1, 8, 2, 32, 0};
	XCTAssertNoThrow(mOggDecoder->Initialize(&in_dsc, &out_dsc, buffer, 4216));
	
	@autoreleasepool {
		NSBundle *ourBundle = [NSBundle bundleForClass:[self class]];
		NSString *filepath = [ourBundle pathForResource:@"vorbis.ogg.data" ofType:nil];
		
		f_in.open([filepath fileSystemRepresentation], std::ios::in);
	}
	
	XCTAssert(f_in.good());
	
	f_in.read(buffer, 16384);
	f_in.close();
	
	UInt32 bytes = 8370;
	UInt32 packets = 2;
	
	Boolean appended = false;
	
	AudioStreamPacketDescription pd[2] = {{0, 6720, 4152}, {4152, 7168, 4218}};
	
	XCTAssertNoThrow(mOggDecoder->AppendInputData(buffer, bytes, packets, pd));
	
	packets = 6720;
	bytes = 8 * packets;
	char audio[bytes];
	UInt32 ac_ret = kAudioCodecProduceOutputPacketFailure;
	
	try {
		ac_ret = mOggDecoder->ProduceOutputPackets(audio, bytes, packets, NULL);
	} catch (...) {
		bytes = 0;
		packets = 0;
	};
	
	XCTAssert(ac_ret == kAudioCodecProduceOutputPacketSuccess &&
			  bytes == 53760 && packets == 6720);
}

/*
 * The first ogg page contains more/less audio data than indicated by the grpos.
 * (Vorbis I specification, Section A.2)
 */
- (void)testAudioOffset
{
    std::ifstream f_in;
    char cookie[8192];
    char buffer[16384];
    UInt32 cookie_size = 4216;

	@autoreleasepool {
		NSBundle *ourBundle = [NSBundle bundleForClass:[self class]];
		NSString *filepath = [ourBundle pathForResource:@"vorbis.ogg.cookie" ofType:nil];
		
		f_in.open([filepath fileSystemRepresentation], std::ios::in);
	}

    XCTAssert(f_in.good());

    f_in.read(cookie, 8192);
    f_in.close();

    AudioStreamBasicDescription in_dsc = {44100.0, 'XoVs', 0, 0, 0, 0, 2, 0, 0};
    AudioStreamBasicDescription out_dsc = {44100.0, kAudioFormatLinearPCM,
                                           kAudioFormatFlagsNativeFloatPacked,
                                           8, 1, 8, 2, 32, 0};

    XCTAssertNoThrow(mOggDecoder->Initialize(&in_dsc, &out_dsc, cookie, cookie_size));

	@autoreleasepool {
		NSBundle *ourBundle = [NSBundle bundleForClass:[self class]];
		NSString *filepath = [ourBundle pathForResource:@"vorbis.ogg.data" ofType:nil];
		
		f_in.open([filepath fileSystemRepresentation], std::ios::in);
	}

    XCTAssert(f_in.good());

    f_in.read(buffer, 16384);
    f_in.close();

    UInt32 bytes = 4152;
    UInt32 packets = 1;
    UInt32 orig_duration = 6720;

    Boolean appended = false;

    AudioStreamPacketDescription pd = {0, orig_duration, 4152};

	XCTAssertNoThrow(mOggDecoder->AppendInputData(buffer, bytes, packets, &pd));

    packets = orig_duration;
    bytes = 8 * packets;
    char audio_1[bytes];
    UInt32 ac_ret = kAudioCodecProduceOutputPacketFailure;

    try {
        ac_ret = mOggDecoder->ProduceOutputPackets(audio_1, bytes, packets, NULL);
    } catch (...) {
        bytes = 0;
        packets = 0;
    };

    XCTAssert(ac_ret == kAudioCodecProduceOutputPacketSuccess &&
              bytes == (orig_duration * 8) && packets == orig_duration);

    /* decode again, increased duration (decoder should prepend zeros) */
    mOggDecoder->Reset();
    XCTAssertNoThrow(mOggDecoder->Uninitialize());
    XCTAssert(!mOggDecoder->IsInitialized());

    mOggDecoder->Initialize(&in_dsc, &out_dsc, cookie, cookie_size);
    XCTAssert(mOggDecoder->IsInitialized());

    bytes = 4152;
    packets = 1;
    appended = false;
    UInt32 inc_duration = 6721;

    pd.mStartOffset = 0;
    pd.mVariableFramesInPacket = inc_duration;
    pd.mDataByteSize = 4152;

	XCTAssertNoThrow(mOggDecoder->AppendInputData(buffer, bytes, packets, &pd));

    packets = inc_duration;
    bytes = 8 * packets;
    char audio_2[bytes];
    char audio_z[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    ac_ret = kAudioCodecProduceOutputPacketFailure;

    try {
        XCTAssertNoThrow(ac_ret = mOggDecoder->ProduceOutputPackets(audio_2, bytes, packets, NULL));
    } catch (...) {
        bytes = 0;
        packets = 0;
    };

    XCTAssert(ac_ret == kAudioCodecProduceOutputPacketSuccess &&
              bytes == (inc_duration * 8) && packets == inc_duration);
    XCTAssert(memcmp(audio_2, audio_z, 8) == 0);
    XCTAssert(memcmp(audio_2 + ((inc_duration - orig_duration) * 8),
                     audio_1, (orig_duration * 8)) == 0);

    /* decode again, decreased duration (decoder should truncate head) */
    mOggDecoder->Reset();
    XCTAssertNoThrow(mOggDecoder->Uninitialize());
    XCTAssert(!mOggDecoder->IsInitialized());

    mOggDecoder->Initialize(&in_dsc, &out_dsc, cookie, cookie_size);
    XCTAssert(mOggDecoder->IsInitialized());

    bytes = 4152;
    packets = 1;
    appended = false;
    UInt32 dec_duration_1 = 6716;

    pd.mStartOffset = 0;
    pd.mVariableFramesInPacket = dec_duration_1;
    pd.mDataByteSize = 4152;

    try {
        mOggDecoder->AppendInputData(buffer, bytes, packets, &pd);
        appended = true;
    } catch (...) {
    };

    XCTAssert(appended == true);

    packets = dec_duration_1;
    bytes = 8 * packets;
    ac_ret = kAudioCodecProduceOutputPacketFailure;

    try {
        ac_ret = mOggDecoder->ProduceOutputPackets(audio_2, bytes, packets, NULL);
    } catch (...) {
        bytes = 0;
        packets = 0;
    };

    XCTAssert(ac_ret == kAudioCodecProduceOutputPacketSuccess &&
              bytes == (dec_duration_1 * 8) && packets == dec_duration_1);
    XCTAssert(memcmp(audio_2, audio_1 + ((orig_duration - dec_duration_1) * 8),
                     (dec_duration_1 * 8)) == 0);

    /* decode again, duration decreased a lot */
    mOggDecoder->Reset();
    mOggDecoder->Uninitialize();
    XCTAssert(!mOggDecoder->IsInitialized());

    mOggDecoder->Initialize(&in_dsc, &out_dsc, cookie, cookie_size);
    XCTAssert(mOggDecoder->IsInitialized());

    bytes = 4152;
    packets = 1;
    appended = false;
    UInt32 dec_duration_2 = 2560;

    pd.mStartOffset = 0;
    pd.mVariableFramesInPacket = dec_duration_2;
    pd.mDataByteSize = 4152;

    try {
        mOggDecoder->AppendInputData(buffer, bytes, packets, &pd);
        appended = true;
    } catch (...) {
    };

    XCTAssert(appended == true);

    packets = dec_duration_2;
    bytes = 8 * packets;
    ac_ret = kAudioCodecProduceOutputPacketFailure;

    try {
        ac_ret = mOggDecoder->ProduceOutputPackets(audio_2, bytes, packets, NULL);
    } catch (...) {
        bytes = 0;
        packets = 0;
    };

    XCTAssert(ac_ret == kAudioCodecProduceOutputPacketSuccess &&
              bytes == (dec_duration_2 * 8) && packets == dec_duration_2);
    XCTAssert(memcmp(audio_2, audio_1 + ((orig_duration - dec_duration_2) * 8),
                     (dec_duration_2 * 8)) == 0);
}

@end
