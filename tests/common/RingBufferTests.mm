/*
 *  RingBufferTests.cpp
 *
 *    RingBuffer class test cases.
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
 *  Last modified: $Id: RingBufferTests.cpp 12356 2007-01-20 00:18:04Z arek $
 *
 */


#include "RingBufferTests.h"


@implementation RingBufferTests


- (void)setUp
{
	[super setUp];
	mBuffer = new RingBuffer();
}

- (void)tearDown
{
    delete mBuffer;
    mBuffer = NULL;
	[super tearDown];
}

- (void)testBasic;
{
    mBuffer->Initialize(32);

    XCTAssert(mBuffer->GetBufferByteSize() == 32);
    XCTAssert(mBuffer->GetSpaceAvailable() == 32);
    XCTAssert(mBuffer->GetDataAvailable() == 0);

    UInt32 b_size = mBuffer->Reallocate(16); // shouldn't reallocate

    XCTAssert(b_size == 32);
    XCTAssert(mBuffer->GetBufferByteSize() == 32);
    XCTAssert(mBuffer->GetSpaceAvailable() == 32);
    XCTAssert(mBuffer->GetDataAvailable() == 0);

    Byte *tmp_buf = new Byte[64];
    memset(tmp_buf, 0, 64);

    tmp_buf[0] = 255;
    tmp_buf[1] = 15;
    tmp_buf[16] = 137;

    b_size = 63;
    mBuffer->In(tmp_buf, b_size);

    XCTAssert(b_size == 31);
    XCTAssert(mBuffer->GetBufferByteSize() == 32);
    XCTAssert(mBuffer->GetSpaceAvailable() == 0);
    XCTAssert(mBuffer->GetDataAvailable() == 32);
    XCTAssert(mBuffer->GetData()[0] == 255 && mBuffer->GetData()[1] == 15);

    mBuffer->Zap(32);
    XCTAssert(mBuffer->GetSpaceAvailable() == 32);
    XCTAssert(mBuffer->GetDataAvailable() == 0);

    b_size = mBuffer->Reallocate(48);

    XCTAssert(b_size == 48);
    XCTAssert(mBuffer->GetBufferByteSize() == 48);
    XCTAssert(mBuffer->GetSpaceAvailable() == 48);
    XCTAssert(mBuffer->GetDataAvailable() == 0);

    b_size = 32;
    mBuffer->In(tmp_buf, b_size);
    XCTAssert(b_size == 0);
    XCTAssert(mBuffer->GetDataAvailable() == 32);

    mBuffer->Zap(16);
    XCTAssert(mBuffer->GetDataAvailable() == 16);
    XCTAssert(mBuffer->GetData()[0] == 137);
    XCTAssert(mBuffer->GetDataAvailable() == 16);
    XCTAssert(mBuffer->GetSpaceAvailable() == 32);

    b_size = 31;
    mBuffer->In(tmp_buf, b_size); // should be wrapped now
    XCTAssert(b_size == 0);
    XCTAssert(mBuffer->GetDataAvailable() == 47);
    XCTAssert(mBuffer->GetSpaceAvailable() == 1);
    XCTAssert(mBuffer->GetData()[0] == 137);

    b_size = mBuffer->Reallocate(64);
    XCTAssert(b_size == 64);
    XCTAssert(mBuffer->GetBufferByteSize() == 64);
    XCTAssert(mBuffer->GetSpaceAvailable() == 17);
    XCTAssert(mBuffer->GetDataAvailable() == 47);
    //std::cout << "mBuffer->GetData()[0] == " << (UInt32) mBuffer->GetData()[0] << std::endl;
    XCTAssert(mBuffer->GetData()[0] == 137);

    mBuffer->Zap(16); // [255, 15] should be at the head
    XCTAssert(mBuffer->GetData()[0] == 255 && mBuffer->GetData()[1] == 15);

    b_size = 32;
    mBuffer->In(tmp_buf, b_size); // should be wrapped again
    b_size = mBuffer->Reallocate(96);
    XCTAssert(b_size == 96);
    XCTAssert(mBuffer->GetBufferByteSize() == 96);
    XCTAssert(mBuffer->GetSpaceAvailable() == 33);
    XCTAssert(mBuffer->GetDataAvailable() == 63);
    XCTAssert(mBuffer->GetData()[0] == 255 && mBuffer->GetData()[1] == 15);

    mBuffer->Uninitialize();

    b_size = 32;
    mBuffer->In(tmp_buf, b_size);
    XCTAssert(b_size == 32);
}

@end
