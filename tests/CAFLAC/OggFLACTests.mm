/*
 *  OggFLACTests.cpp
 *
 *    CAOggFLACDecoder class test cases.
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
 *  Last modified: $Id: OggFLACTests.cpp 12356 2007-01-20 00:18:04Z arek $
 *
 */


#include "OggFLACTests.h"
#include <iostream>
#include <fstream>

@implementation OggFLACTests


-(void)setUp
{
	[super setUp];
	mOggDecoder = new CAOggFLACDecoder();
}

-(void)tearDown
{
    delete mOggDecoder;
    mOggDecoder = NULL;
	[super tearDown];
}

- (void)testAppendUninitialized;
{
    UInt32 bytes = 0;
    UInt32 packets = 0;

    BOOL appended = NO;

    try {
        mOggDecoder->AppendInputData(NULL, bytes, packets, NULL);
        appended = YES;
    } catch (...) {
    };

    XCTAssert(appended == NO);
}

- (void)testInitCookie;
{
    std::ifstream f_in;
    char cookie[8192];

	@autoreleasepool {
		NSBundle *ourBundle = [NSBundle bundleForClass:[self class]];
		NSString *filepath = [ourBundle pathForResource:@"flac.ogg.cookie" ofType:nil];
		
		f_in.open([filepath fileSystemRepresentation], std::ios::in);
	}

    XCTAssert(f_in.good());

    f_in.read(cookie, 8192);
    f_in.close();

    AudioStreamBasicDescription in_dsc = {96000.0, 'XoFL', 0, 0, 0, 0, 6, 24, 0};
    AudioStreamBasicDescription out_dsc = {96000.0, kAudioFormatLinearPCM,
                                           kAudioFormatFlagsNativeFloatPacked,
                                           24, 1, 24, 6, 32, 0};

    mOggDecoder->Initialize(&in_dsc, &out_dsc, NULL, 0);

    XCTAssert(mOggDecoder->IsInitialized());

    mOggDecoder->Uninitialize();

    XCTAssert(!mOggDecoder->IsInitialized());

    XCTAssertNoThrow(mOggDecoder->Initialize(NULL, NULL, cookie, 4264));

    XCTAssert(mOggDecoder->IsInitialized());
}

@end
