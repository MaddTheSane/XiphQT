/*
 *  XCACodec.cpp
 *
 *    XCACodec class implementation; shared packet i/o functionality.
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


#include "XCACodec.h"

//#define NDEBUG
#include "debug.h"


XCACodec::XCACodec() :
mBDCBuffer(),
mBDCStatus(kBDCStatusOK)
{
}

XCACodec::~XCACodec()
{
}

#pragma mark The Core Functions

void XCACodec::AppendInputData(const void* inInputData, UInt32& ioInputDataByteSize, UInt32& ioNumberPackets,
                                const AudioStreamPacketDescription* inPacketDescription)
{
    dprintf(" >> [%08lx] XCACodec :: AppendInputData(%ld [%ld])\n", (UInt32) this, ioNumberPackets, ioInputDataByteSize);
	if(!mIsInitialized) CODEC_THROW(kAudioCodecStateError);
	
    UInt32 bytesToCopy = BufferGetAvailableBytesSize();
    if (bytesToCopy > 0) {
        UInt32 packet = 0;
        UInt32 bytes = 0;
        while (packet < ioNumberPackets) {
            if (bytes + inPacketDescription[packet].mDataByteSize > bytesToCopy)
                break;
            dprintf("     ----  :: %ld: %ld [%ld]\n", packet, inPacketDescription[packet].mDataByteSize,
                   inPacketDescription[packet].mVariableFramesInPacket);
            InPacket(inInputData, &inPacketDescription[packet]);
            
            bytes += inPacketDescription[packet].mDataByteSize;
            packet++;
        }
        
        if (bytes == 0)
            CODEC_THROW(kAudioCodecNotEnoughBufferSpaceError);
        else {
            ioInputDataByteSize = bytes;
            ioNumberPackets = packet;
        }
    } else {
        CODEC_THROW(kAudioCodecNotEnoughBufferSpaceError);
    }
    dprintf("<.. [%08lx] XCACodec :: AppendInputData()\n", (UInt32) this);
}

UInt32 XCACodec::ProduceOutputPackets(void* outOutputData, UInt32& ioOutputDataByteSize, UInt32& ioNumberPackets,
                                     AudioStreamPacketDescription* outPacketDescription)
{
    dprintf(" >> [%08lx] XCACodec :: ProduceOutputPackets(%ld [%ld])\n", (UInt32) this, ioNumberPackets, ioOutputDataByteSize);

	UInt32 theAnswer = kAudioCodecProduceOutputPacketSuccess;
	
	if(!mIsInitialized)
		CODEC_THROW(kAudioCodecStateError);
    
    UInt32 frames = 0;
    UInt32 fout = 0; //frames produced
    UInt32 pout = 0; //full (input) packets processed
    UInt32 requested_space_as_frames = ioOutputDataByteSize / mOutputFormat.mBytesPerFrame;
    
    // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
    
    //TODO: zamienic fout/pout z ioOutputDataByteSize/ioNumberPackets:
    //      zainicjowac na poczatku, a potem bezposrednio modyfikowac io*, nie [pf]out ...
    while (fout < requested_space_as_frames && pout < ioNumberPackets) {
        while ((frames = FramesReady()) == 0) {
            if (BufferIsEmpty()) {
                ioNumberPackets = pout;
                ioOutputDataByteSize = mOutputFormat.mBytesPerFrame * fout;
                theAnswer = kAudioCodecProduceOutputPacketNeedsMoreInputData;
                dprintf("<.! [%08lx] XCACodec :: ProduceOutputPackets(%ld [%ld]) = %ld [%ld]\n", (UInt32) this,
                       ioNumberPackets, ioOutputDataByteSize, theAnswer, FramesReady());
                return theAnswer;
            }
            
            if (GenerateFrames() != true) {
                if (BDCGetStatus() == kBDCStatusAbort) {
                    ioNumberPackets = pout;
                    ioOutputDataByteSize = mOutputFormat.mBytesPerFrame * fout;
                    theAnswer = kAudioCodecProduceOutputPacketFailure;
                    dprintf("<!! [%08lx] XCACodec :: ProduceOutputPackets(%ld [%ld]) = %ld [%ld]\n", (UInt32) this,
                           ioNumberPackets, ioOutputDataByteSize, theAnswer, FramesReady());
                    return theAnswer;
                }
            }
        }
        
        if (frames == 0)
            continue;
        
        if ((fout + frames) * mOutputFormat.mBytesPerFrame > ioOutputDataByteSize)
            frames = requested_space_as_frames - fout;
        
        OutputFrames(outOutputData, frames, fout);

        fout += frames;
        
        Zap(frames);
    
        pout += InPacketsConsumed();
    }
    
    // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    
    ioOutputDataByteSize = mOutputFormat.mBytesPerFrame * fout; //???
    ioNumberPackets = pout;
    
    theAnswer = (FramesReady() > 0 || !BufferIsEmpty()) ? kAudioCodecProduceOutputPacketSuccessHasMore
                                                            : kAudioCodecProduceOutputPacketSuccess;
    
    dprintf("<.. [%08lx] XCACodec :: ProduceOutputPackets(%ld [%ld]) = %ld [%ld]\n",
           (UInt32) this, ioNumberPackets, ioOutputDataByteSize, theAnswer, FramesReady());
    return theAnswer;
}

UInt32 XCACodec::InPacketsConsumed() const
{
    // the simplest case, works properly only if _every_ 'in' packet
    // generates positive number of samples (so it won't work with Vorbis)
    return (FramesReady() == 0);
}


#pragma mark Buffer/Decode/Convert interface

void XCACodec::BDCInitialize(UInt32 inInputBufferByteSize)
{
    mBDCBuffer.Initialize(inInputBufferByteSize);
    mBDCStatus = kBDCStatusOK;
}

void XCACodec::BDCUninitialize()
{
    mBDCBuffer.Uninitialize();
}

void XCACodec::BDCReset()
{
    mBDCBuffer.Reset();
}

void XCACodec::BDCReallocate(UInt32 inInputBufferByteSize)
{
    mBDCBuffer.Uninitialize();
    mBDCBuffer.Initialize(inInputBufferByteSize);
}


UInt32 XCACodec::BufferGetBytesSize() const
{
    return mBDCBuffer.GetBufferByteSize();
}

UInt32 XCACodec::BufferGetUsedBytesSize() const
{
    return mBDCBuffer.GetDataAvailable();
}

UInt32 XCACodec::BufferGetAvailableBytesSize() const
{
    return mBDCBuffer.GetSpaceAvailable();
}

Boolean XCACodec::BufferIsEmpty() const
{
    return (mBDCBuffer.GetDataAvailable() == 0);
}

