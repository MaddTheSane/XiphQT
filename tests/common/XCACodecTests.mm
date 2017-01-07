/*
 *  XCACodecTests.cpp
 *
 *    XCACodec class test cases.
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
 *  Last modified: $Id$
 *
 */


#include "XCACodecTests.h"

@implementation XCACodecTests

- (void)setUp
{
	[super setUp];
    //mCodec = new XCACodec();
    mCodec = new test_XCACodec();
}

- (void)tearDown
{
    delete mCodec;
    mCodec = NULL;
	[super tearDown];
}

- (void)testAppendUninitialized
{
    UInt32 bytes = 0;
    UInt32 packets = 0;

    Boolean appended = false;

    try {
        mCodec->AppendInputData(NULL, bytes, packets, NULL);
        appended = true;
    } catch (...) {
    };

    XCTAssert(appended == false);
}

- (void)testAppendZero
{
    UInt32 bytes = 0;
    UInt32 packets = 0;

    Boolean appended = false;
    Boolean other_error = false;
    ComponentResult ac_error = kAudioCodecNoError;

    mCodec->Initialize(NULL, NULL, NULL, 0);
    XCTAssert(mCodec->IsInitialized());

    try {
        mCodec->AppendInputData(NULL, bytes, packets, NULL);
        appended = true;
    } catch (ComponentResult acError) {
        ac_error = acError;
    } catch (...) {
        other_error = true;
    };

    XCTAssert(appended == false);
    XCTAssert(bytes == 0);
    XCTAssert(packets == 0);
    XCTAssert(other_error == false);

    /* There is no apparent appropriate error code for 0-sized input
       on decoding, NotEnoughBufferSpace seems to do the trick on both
       PPC/Intel. */
    XCTAssert(ac_error == kAudioCodecNotEnoughBufferSpaceError);

    mCodec->Uninitialize();
    XCTAssert(mCodec->IsInitialized() == false);
}

@end
