/*
 *  CAVorbisEncoder.cpp
 *
 *    CAVorbisEncoder class implementation; the main part of the Vorbis
 *    encoding functionality.
 *
 *
 *  Copyright (c) 2006  Arek Korbik
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


#include <Ogg/ogg.h>
#include <Vorbis/vorbisenc.h>

#include "CAVorbisEncoder.h"

#include "CABundleLocker.h"

#include "vorbis_versions.h"
#include "fccs.h"
#include "data_types.h"

//#define NDEBUG
#include "debug.h"


#if !defined(NO_ACS)
#define NO_ACS 1
#endif /* defined(NO_ACS) */

#if !defined(NO_ADV_PROPS)
#define NO_ADV_PROPS 1
#endif /* defined(NO_ADV_PROPS) */

#define DBG_STREAMDESC_FMT " [CASBD: sr=%lf, fmt=%4.4s, fl=%lx, bpp=%ld, fpp=%ld, bpf=%ld, ch=%ld, bpc=%ld]"
#define DBG_STREAMDESC_FILL(x) (x)->mSampleRate, reinterpret_cast<const char*> (&((x)->mFormatID)), \
        (x)->mFormatFlags, (x)->mBytesPerPacket, (x)->mFramesPerPacket, (x)->mBytesPerPacket, \
        (x)->mChannelsPerFrame, (x)->mBitsPerChannel

CAVorbisEncoder::CAVorbisEncoder() :
    mCookie(NULL), mCookieSize(0), mCompressionInitialized(false), mEOSHit(false),
    last_granulepos(0), last_packetno(0), mVorbisFPList(), mProducedPList()
{
    CAStreamBasicDescription theInputFormat1(kAudioStreamAnyRate, kAudioFormatLinearPCM,
                                             0, 1, 0, 0, 32, kAudioFormatFlagsNativeFloatPacked);
    AddInputFormat(theInputFormat1);

    CAStreamBasicDescription theInputFormat2(kAudioStreamAnyRate, kAudioFormatLinearPCM,
                                             0, 1, 0, 0, 16,
                                             kAudioFormatFlagsNativeEndian |
                                             kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked);
    AddInputFormat(theInputFormat2);

    mInputFormat.mSampleRate = 44100.0;
    mInputFormat.mFormatID = kAudioFormatLinearPCM;
    mInputFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
    mInputFormat.mBytesPerPacket = 8;
    mInputFormat.mFramesPerPacket = 1;
    mInputFormat.mBytesPerFrame = 8;
    mInputFormat.mChannelsPerFrame = 2;
    mInputFormat.mBitsPerChannel = 32;

    CAStreamBasicDescription theOutputFormat(kAudioStreamAnyRate, kAudioFormatXiphVorbis,
                                             kVorbisBytesPerPacket, kVorbisFramesPerPacket,
                                             kVorbisBytesPerFrame, kVorbisChannelsPerFrame,
                                             kVorbisBitsPerChannel, kVorbisFormatFlags);
    AddOutputFormat(theOutputFormat);

    mOutputFormat.mSampleRate = 44100.0;
    mOutputFormat.mFormatID = kAudioFormatXiphVorbis;
    mOutputFormat.mFormatFlags = kVorbisFormatFlags;
    mOutputFormat.mBytesPerPacket = kVorbisBytesPerPacket;
    mOutputFormat.mFramesPerPacket = kVorbisFramesPerPacket;
    mOutputFormat.mBytesPerFrame = kVorbisBytesPerFrame;
    mOutputFormat.mChannelsPerFrame = 2;
    mOutputFormat.mBitsPerChannel = kVorbisBitsPerChannel;
}

CAVorbisEncoder::~CAVorbisEncoder()
{
    if (mCookie != NULL)
        delete[] mCookie;

    if (mCompressionInitialized) {
        vorbis_block_clear(&mV_vb);
        vorbis_dsp_clear(&mV_vd);

        vorbis_comment_clear(&mV_vc);

        vorbis_info_clear(&mV_vi);
    }
}

void CAVorbisEncoder::Initialize(const AudioStreamBasicDescription* inInputFormat,
                                 const AudioStreamBasicDescription* inOutputFormat,
                                 const void* inMagicCookie, UInt32 inMagicCookieByteSize)
{
    dbg_printf("[  VE]  >> [%08lx] :: Initialize(%d, %d, %d)\n", (UInt32) this, inInputFormat != NULL, inOutputFormat != NULL, inMagicCookieByteSize != 0);
    if (inInputFormat)
        dbg_printf("[  VE]   > [%08lx] :: InputFormat :" DBG_STREAMDESC_FMT "\n", (UInt32) this, DBG_STREAMDESC_FILL(inInputFormat));
    if (inOutputFormat)
        dbg_printf("[  VE]   > [%08lx] :: OutputFormat:" DBG_STREAMDESC_FMT "\n", (UInt32) this, DBG_STREAMDESC_FILL(inOutputFormat));

    if(inInputFormat != NULL) {
        SetCurrentInputFormat(*inInputFormat);
    }

    if(inOutputFormat != NULL) {
        SetCurrentOutputFormat(*inOutputFormat);
    }

    if ((mInputFormat.mSampleRate != mOutputFormat.mSampleRate) ||
        (mInputFormat.mChannelsPerFrame != mOutputFormat.mChannelsPerFrame) ||
        (mInputFormat.mSampleRate == 0.0)) {
        CODEC_THROW(kAudioCodecUnsupportedFormatError);
    }

    BDCInitialize(kVorbisEncoderBufferSize);

    InitializeCompressionSettings();

    FixFormats();

    XCACodec::Initialize(inInputFormat, inOutputFormat, inMagicCookie, inMagicCookieByteSize);

    dbg_printf("[  VE]  <  [%08lx] :: InputFormat :" DBG_STREAMDESC_FMT "\n", (UInt32) this, DBG_STREAMDESC_FILL(&mInputFormat));
    dbg_printf("[  VE]  <  [%08lx] :: OutputFormat:" DBG_STREAMDESC_FMT "\n", (UInt32) this, DBG_STREAMDESC_FILL(&mOutputFormat));
    dbg_printf("[  VE] <.. [%08lx] :: Initialize(%d, %d, %d)\n", (UInt32) this, inInputFormat != NULL, inOutputFormat != NULL, inMagicCookieByteSize != 0);
}

void CAVorbisEncoder::Uninitialize()
{
    dbg_printf("[  VE]  >> [%08lx] :: Uninitialize()\n", (UInt32) this);
    BDCUninitialize();

    XCACodec::Uninitialize();
    dbg_printf("[  VE] <.. [%08lx] :: Uninitialize()\n", (UInt32) this);
}

/*
 * Property selectors TODO (?):
 *  'gcsc' - ?
 *  'acps' - ?
 */


void CAVorbisEncoder::GetProperty(AudioCodecPropertyID inPropertyID, UInt32& ioPropertyDataSize, void* outPropertyData)
{
    dbg_printf("[  VE]  >> [%08lx] :: GetProperty('%4.4s') (%d)\n", (UInt32) this, reinterpret_cast<char*> (&inPropertyID), inPropertyID == kAudioCodecPropertyFormatCFString);
    switch(inPropertyID)
    {

        /*
    case kAudioCodecPropertyInputChannelLayout :
    case kAudioCodecPropertyOutputChannelLayout :
        // by default a codec doesn't support channel layouts.
        CODEC_THROW(kAudioCodecIllegalOperationError);
        break;
    case kAudioCodecPropertyAvailableInputChannelLayouts :
    case kAudioCodecPropertyAvailableOutputChannelLayouts :
        // by default a codec doesn't support channel layouts.
        CODEC_THROW(kAudioCodecIllegalOperationError);
        break;
	*/

    case kAudioCodecPropertyAvailableNumberChannels:
        if(ioPropertyDataSize == sizeof(UInt32) * 1) {
            (reinterpret_cast<UInt32*>(outPropertyData))[0] = 0xffffffff;
        } else {
            CODEC_THROW(kAudioCodecBadPropertySizeError);
        }
        break;

    case kAudioCodecPropertyAvailableBitRateRange:
        if (ioPropertyDataSize == sizeof(AudioValueRange) * 1) {
            (reinterpret_cast<AudioValueRange*>(outPropertyData))->mMinimum = 1.0;
            (reinterpret_cast<AudioValueRange*>(outPropertyData))->mMaximum = 480.0;
        } else {
            CODEC_THROW(kAudioCodecBadPropertySizeError);
        }
        break;

    case kAudioCodecPropertyPrimeMethod:
        if(ioPropertyDataSize == sizeof(UInt32)) {
            *reinterpret_cast<UInt32*>(outPropertyData) = (UInt32)kAudioCodecPrimeMethod_None;
        } else {
            CODEC_THROW(kAudioCodecBadPropertySizeError);
        }
        break;

    case kAudioCodecPropertyPrimeInfo:
        if(ioPropertyDataSize == sizeof(AudioCodecPrimeInfo) ) {
            (reinterpret_cast<AudioCodecPrimeInfo*>(outPropertyData))->leadingFrames = 0;
            (reinterpret_cast<AudioCodecPrimeInfo*>(outPropertyData))->trailingFrames = 0;
        } else {
            CODEC_THROW(kAudioCodecBadPropertySizeError);
        }
        break;

#if !defined(NO_ADV_PROPS) || NO_ADV_PROPS == 0
    case kAudioCodecPropertyApplicableInputSampleRates:
        if (ioPropertyDataSize == sizeof(AudioValueRange)) {
            (reinterpret_cast<AudioValueRange*>(outPropertyData))->mMinimum = 0.0;
            (reinterpret_cast<AudioValueRange*>(outPropertyData))->mMaximum = 0.0;
        } else {
            CODEC_THROW(kAudioCodecBadPropertySizeError);
        }
        break;

    case kAudioCodecPropertyApplicableOutputSampleRates:
        if (ioPropertyDataSize == sizeof(AudioValueRange)) {
            (reinterpret_cast<AudioValueRange*>(outPropertyData))->mMinimum = 0.0;
            (reinterpret_cast<AudioValueRange*>(outPropertyData))->mMaximum = 0.0;
        } else {
            CODEC_THROW(kAudioCodecBadPropertySizeError);
        }
        break;
#endif

    case kAudioCodecPropertyAvailableInputSampleRates:
        if (ioPropertyDataSize == sizeof(AudioValueRange)) {
            (reinterpret_cast<AudioValueRange*>(outPropertyData))->mMinimum = 0.0;
            (reinterpret_cast<AudioValueRange*>(outPropertyData))->mMaximum = 0.0;
        } else {
            CODEC_THROW(kAudioCodecBadPropertySizeError);
        }
        break;

    case kAudioCodecPropertyAvailableOutputSampleRates:
        if (ioPropertyDataSize == sizeof(AudioValueRange)) {
            (reinterpret_cast<AudioValueRange*>(outPropertyData))->mMinimum = 0.0;
            (reinterpret_cast<AudioValueRange*>(outPropertyData))->mMaximum = 0.0;
        } else {
            CODEC_THROW(kAudioCodecBadPropertySizeError);
        }
        break;

#if !defined(NO_ACS) || NO_ACS == 0
    case kAudioCodecPropertySettings:
        if (ioPropertyDataSize == sizeof(CFDictionaryRef)) {
            
            BuildSettings(outPropertyData);
            
        } else {
            CODEC_THROW(kAudioCodecBadPropertySizeError);
        }
        break;
#endif

        
    case kAudioCodecPropertyRequiresPacketDescription:
        if(ioPropertyDataSize == sizeof(UInt32)) {
            *reinterpret_cast<UInt32*>(outPropertyData) = 1;
        } else {
            CODEC_THROW(kAudioCodecBadPropertySizeError);
        }
        break;

    case kAudioCodecPropertyHasVariablePacketByteSizes:
        if(ioPropertyDataSize == sizeof(UInt32)) {
            *reinterpret_cast<UInt32*>(outPropertyData) = 1;
        } else {
            CODEC_THROW(kAudioCodecBadPropertySizeError);
        }
        break;

    case kAudioCodecPropertyPacketFrameSize:
        if(ioPropertyDataSize == sizeof(UInt32))
        {
            if (mIsInitialized) {
                long long_blocksize = (reinterpret_cast<long *>(mV_vi.codec_setup))[1];
                *reinterpret_cast<UInt32*>(outPropertyData) = long_blocksize;
                //*reinterpret_cast<UInt32*>(outPropertyData) = 0;
            } else {
                *reinterpret_cast<UInt32*>(outPropertyData) = 0;
            }
        }
        else
        {
            CODEC_THROW(kAudioCodecBadPropertySizeError);
        }
        break;

    case kAudioCodecPropertyMaximumPacketByteSize:
        if(ioPropertyDataSize == sizeof(UInt32)) {
            *reinterpret_cast<UInt32*>(outPropertyData) = mCompressionInitialized ? kVorbisFormatMaxBytesPerPacket : 0;
            //*reinterpret_cast<UInt32*>(outPropertyData) = mCompressionInitialized ? (64 * 1024) : 0;
        } else {
            CODEC_THROW(kAudioCodecBadPropertySizeError);
        }
        break;

    case kAudioCodecPropertyCurrentInputSampleRate:
        if (ioPropertyDataSize == sizeof(Float64)) {
            *reinterpret_cast<Float64*>(outPropertyData) = mInputFormat.mSampleRate;
        } else {
            CODEC_THROW(kAudioCodecBadPropertySizeError);
        }
        break;

    case kAudioCodecPropertyCurrentOutputSampleRate:
        if (ioPropertyDataSize == sizeof(Float64)) {
            *reinterpret_cast<Float64*>(outPropertyData) = mOutputFormat.mSampleRate;
        } else {
            CODEC_THROW(kAudioCodecBadPropertySizeError);
        }
        break;

    case kAudioCodecDoesSampleRateConversion:
        if(ioPropertyDataSize == sizeof(UInt32)) {
            *reinterpret_cast<UInt32*>(outPropertyData) = 0;
        } else {
            CODEC_THROW(kAudioCodecBadPropertySizeError);
        }
        break;

    case kAudioCodecPropertyQualitySetting:
        if(ioPropertyDataSize == sizeof(UInt32)) {
            *reinterpret_cast<UInt32*>(outPropertyData) = kAudioCodecQuality_Max;
        } else {
            CODEC_THROW(kAudioCodecBadPropertySizeError);
        }
        break;

#if TARGET_OS_MAC || 1
    case kAudioCodecPropertyFormatCFString:
        {
            if (ioPropertyDataSize != sizeof(CFStringRef))
                CODEC_THROW(kAudioCodecBadPropertySizeError);
            CABundleLocker lock;
            CFStringRef name = CFCopyLocalizedStringFromTableInBundle(CFSTR("Xiph Vorbis"), CFSTR("CodecNames"), GetCodecBundle(), CFSTR(""));
            *(CFStringRef*)outPropertyData = name;
            break;
        }

    case kAudioCodecPropertyNameCFString:
        {
            if (ioPropertyDataSize != sizeof(CFStringRef)) CODEC_THROW(kAudioCodecBadPropertySizeError);
            CABundleLocker lock;
            CFStringRef name = CFCopyLocalizedStringFromTableInBundle(CFSTR("Xiph Vorbis encoder"), CFSTR("CodecNames"), GetCodecBundle(), CFSTR(""));
            *(CFStringRef*)outPropertyData = name;
            break;
        }

    case kAudioCodecPropertyManufacturerCFString:
        {
            if (ioPropertyDataSize != sizeof(CFStringRef)) CODEC_THROW(kAudioCodecBadPropertySizeError);
            CABundleLocker lock;
            CFStringRef name = CFCopyLocalizedStringFromTableInBundle(CFSTR("Xiph.Org Foundation"), CFSTR("CodecNames"), GetCodecBundle(), CFSTR(""));
            *(CFStringRef*)outPropertyData = name;
            break;
        }
#endif

    default:
        ACBaseCodec::GetProperty(inPropertyID, ioPropertyDataSize, outPropertyData);
    }
    dbg_printf("[  VE] <.. [%08lx] :: GetProperty('%4.4s')\n", (UInt32) this, reinterpret_cast<char*> (&inPropertyID));
}

void CAVorbisEncoder::GetPropertyInfo(AudioCodecPropertyID inPropertyID, UInt32& outPropertyDataSize, bool& outWritable)
{
    dbg_printf("[  VE]  >> [%08lx] :: GetPropertyInfo('%4.4s')\n", (UInt32) this, reinterpret_cast<char*> (&inPropertyID));
    switch(inPropertyID)
    {
        /*
    case kAudioCodecPropertyInputChannelLayout :
    case kAudioCodecPropertyOutputChannelLayout :
        // by default a codec doesn't support channel layouts.
        CODEC_THROW(kAudioCodecIllegalOperationError);
        break;
    case kAudioCodecPropertyAvailableInputChannelLayouts :
    case kAudioCodecPropertyAvailableOutputChannelLayouts :
        // by default a codec doesn't support channel layouts.
        CODEC_THROW(kAudioCodecIllegalOperationError);
        break;
        */
    case kAudioCodecPropertyAvailableNumberChannels:
        outPropertyDataSize = sizeof(UInt32) * 1; // [0xffffffff - any number]
        outWritable = false;
        break;

    case kAudioCodecPropertyAvailableBitRateRange:
        outPropertyDataSize = sizeof(AudioValueRange) * 1;
        outWritable = false;
        break;

#if !defined(NO_ADV_PROPS) || NO_ADV_PROPS == 0
    case kAudioCodecPropertyApplicableInputSampleRates:
    case kAudioCodecPropertyApplicableOutputSampleRates:
#endif

    case kAudioCodecPropertyAvailableInputSampleRates:
        outPropertyDataSize = sizeof(AudioValueRange);
        outWritable = false;
        break;

    case kAudioCodecPropertyAvailableOutputSampleRates:
        outPropertyDataSize = sizeof(AudioValueRange);
        outWritable = false;
        break;

#if !defined(NO_ACS) || NO_ACS == 0
    case kAudioCodecPropertySettings:
        outPropertyDataSize = sizeof(CFDictionaryRef);
        outWritable = true;
        break;
#endif

        
    case kAudioCodecPropertyRequiresPacketDescription:
        outPropertyDataSize = sizeof(UInt32);
        outWritable = false;
        break;

    case kAudioCodecPropertyHasVariablePacketByteSizes:
        outPropertyDataSize = sizeof(UInt32);
        outWritable = false;
        break;

    case kAudioCodecPropertyPacketFrameSize:
        outPropertyDataSize = sizeof(UInt32);
        outWritable = false;
        break;

    case kAudioCodecPropertyMaximumPacketByteSize:
        outPropertyDataSize = sizeof(UInt32);
        outWritable = false;
        break;

    case kAudioCodecPropertyCurrentInputSampleRate:
    case kAudioCodecPropertyCurrentOutputSampleRate:
        outPropertyDataSize = sizeof(Float64);
        outWritable = false;
        break;

    case kAudioCodecDoesSampleRateConversion:
        outPropertyDataSize = sizeof(UInt32);
        outWritable = false;
        break;

    case kAudioCodecPropertyQualitySetting:
        outPropertyDataSize = sizeof(UInt32);
        outWritable = true;
        break;

    default:
        ACBaseCodec::GetPropertyInfo(inPropertyID, outPropertyDataSize, outWritable);
        break;

    }
    dbg_printf("[  VE] <.. [%08lx] :: GetPropertyInfo('%4.4s')\n", (UInt32) this, reinterpret_cast<char*> (&inPropertyID));
}

void CAVorbisEncoder::SetProperty(AudioCodecPropertyID inPropertyID, UInt32 inPropertyDataSize, const void* inPropertyData)
{
    dbg_printf("[  VE]  >> [%08lx] :: SetProperty('%4.4s')\n", (UInt32) this, reinterpret_cast<char*> (&inPropertyID));

    switch(inPropertyID) {
#if !defined(NO_ACS) || NO_ACS == 0
    case kAudioCodecPropertySettings:
        if (inPropertyDataSize == sizeof(CFDictionaryRef)) {
            ApplySettings(inPropertyData);
        } else {
            CODEC_THROW(kAudioCodecBadPropertySizeError);
        }
        break;
#endif

    default:
        ACBaseCodec::SetProperty(inPropertyID, inPropertyDataSize, inPropertyData);
        break;
    }
    dbg_printf("[  VE] <.. [%08lx] :: SetProperty('%4.4s')\n", (UInt32) this, reinterpret_cast<char*> (&inPropertyID));
}

void CAVorbisEncoder::Reset()
{
    dbg_printf("[  VE] > > [%08lx] :: Reset()\n", (UInt32) this);
    BDCReset();

    XCACodec::Reset();
    dbg_printf("[  VE] < < [%08lx] :: Reset()\n", (UInt32) this);
}

UInt32 CAVorbisEncoder::GetVersion() const
{
    return kCAVorbis_aenc_Version;
}


void CAVorbisEncoder::SetCurrentInputFormat(const AudioStreamBasicDescription& inInputFormat)
{
    if (!mIsInitialized) {
        //	check to make sure the input format is legal
        if ((inInputFormat.mFormatID != kAudioFormatLinearPCM) ||
            !(((inInputFormat.mFormatFlags == kAudioFormatFlagsNativeFloatPacked) &&
               (inInputFormat.mBitsPerChannel == 32)) ||
              ((inInputFormat.mFormatFlags == (kLinearPCMFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked)) &&
               (inInputFormat.mBitsPerChannel == 16))))
        {
            dbg_printf("CAVorbisEncoder::SetFormats: only supports either 16 bit native endian signed integer or 32 bit native endian Core Audio floats for input\n");
            CODEC_THROW(kAudioCodecUnsupportedFormatError);
        }

        //	tell our base class about the new format
        XCACodec::SetCurrentInputFormat(inInputFormat);
    } else {
        CODEC_THROW(kAudioCodecStateError);
    }
}

void CAVorbisEncoder::SetCurrentOutputFormat(const AudioStreamBasicDescription& inOutputFormat)
{
    if (!mIsInitialized) {
        //	check to make sure the output format is legal
        if (inOutputFormat.mFormatID != kAudioFormatXiphVorbis) {
            dbg_printf("CAVorbisEncoder::SetFormats: only supports Xiph Vorbis for output\n");
            CODEC_THROW(kAudioCodecUnsupportedFormatError);
        }

        //	tell our base class about the new format
        XCACodec::SetCurrentOutputFormat(inOutputFormat);
    } else {
        CODEC_THROW(kAudioCodecStateError);
    }
}

UInt32 CAVorbisEncoder::GetMagicCookieByteSize() const
{
    return mCookieSize;
}

void CAVorbisEncoder::GetMagicCookie(void* outMagicCookieData, UInt32& ioMagicCookieDataByteSize) const
{
    if (mCookie != NULL) {
        ioMagicCookieDataByteSize = mCookieSize;
        BlockMoveData(mCookie, outMagicCookieData, ioMagicCookieDataByteSize);
    } else {
        ioMagicCookieDataByteSize = 0;
    }
}

void CAVorbisEncoder::SetMagicCookie(const void* inMagicCookieData, UInt32 inMagicCookieDataByteSize)
{
    dbg_printf("[  VE]  >> [%08lx] :: SetMagicCookie()\n", (UInt32) this);
    CODEC_THROW(kAudioCodecIllegalOperationError);
    dbg_printf("[  VE] <.. [%08lx] :: SetMagicCookie()\n", (UInt32) this);
}

void CAVorbisEncoder::SetCookie(const void* inMagicCookieData, UInt32 inMagicCookieDataByteSize)
{
#if 0
    if (mCookie != NULL)
        delete[] mCookie;

    mCookieSize = inMagicCookieDataByteSize;
    if (inMagicCookieDataByteSize != 0) {
        mCookie = new Byte[inMagicCookieDataByteSize];
        BlockMoveData(inMagicCookieData, mCookie, inMagicCookieDataByteSize);
    } else {
        mCookie = NULL;
    }
#endif /* 0 */
}



void CAVorbisEncoder::FixFormats()
{
    dbg_printf("[  VE]  >> [%08lx] :: FixFormats()\n", (UInt32) this);
    mOutputFormat.mSampleRate = mInputFormat.mSampleRate;
    mOutputFormat.mBitsPerChannel = 0;
    mOutputFormat.mBytesPerPacket = 0;
    mOutputFormat.mFramesPerPacket = 0;

    //long long_blocksize = (reinterpret_cast<long *>(mV_vi.codec_setup))[1];
    //mOutputFormat.mFramesPerPacket = long_blocksize;

    dbg_printf("[  VE] <.. [%08lx] :: FixFormats()\n", (UInt32) this);
}


Boolean CAVorbisEncoder::BuildSettings(void *outSettingsDict)
{
    const char *buffer = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"
        "<plist version=\"1.0\">"
        "<dict>"
        "	<key>name</key>"
        "	<string>Xiph Vorbis Encoder</string>"
        "	<key>parameters</key>"
        "	<array>"
        "		<dict>"
        "			<key>available values</key>"
        "			<array>"
        "				<string>Variable Bit Rate (target quality)</string>"
        "				<string>Average Bit Rate</string>"
        "				<string>Variable Bit Rate (target bit rate)</string>"
        "			</array>"
        "			<key>current value</key>"
        "			<integer>0</integer>"
        "			<key>hint</key>"
        "			<integer>5</integer>"
        "			<key>key</key>"
        "			<string>Target Format</string>"
        "			<key>name</key>"
        "			<string>Bit Rate Format</string>"
        "			<key>summary</key>"
        "			<string>Vorbis encoding bit rate management mode</string>"
        "			<key>value type</key>"
        "			<integer>10</integer>"
        "		</dict>"
        "		<dict>"
        "			<key>available values</key>"
        "			<array>"
        "				<string>16</string>"
        "				<string>20</string>"
        "				<string>24</string>"
        "				<string>28</string>"
        "				<string>32</string>"
        "				<string>40</string>"
        "				<string>48</string>"
        "				<string>56</string>"
        "				<string>64</string>"
        "				<string>80</string>"
        "				<string>96</string>"
        "				<string>112</string>"
        "				<string>128</string>"
        "				<string>160</string>"
        "				<string>192</string>"
        "				<string>224</string>"
        "				<string>256</string>"
        "				<string>320</string>"
        "			</array>"
        "			<key>current value</key>"
        "			<integer>12</integer>"
        "			<key>hint</key>"
        "			<integer>4</integer>"
        "			<key>key</key>"
        "			<string>Bit Rate</string>"
        "			<key>limited values</key>"
        "			<array>"
        "				<string>16</string>"
        "				<string>20</string>"
        "				<string>24</string>"
        "				<string>28</string>"
        "				<string>32</string>"
        "				<string>40</string>"
        "				<string>48</string>"
        "				<string>56</string>"
        "				<string>64</string>"
        "				<string>80</string>"
        "				<string>96</string>"
        "				<string>112</string>"
        "				<string>128</string>"
        "				<string>160</string>"
        "				<string>192</string>"
        "				<string>224</string>"
        "				<string>256</string>"
        "				<string>320</string>"
        "			</array>"
        "			<key>name</key>"
        "			<string>Target Bit Rate</string>"
        "			<key>summary</key>"
        "			<string>The bit rate of the Vorbis bitstream produced by the encoder</string>"
        "			<key>unit</key>"
        "			<string>kbps</string>"
        "			<key>value type</key>"
        "			<integer>10</integer>"
        "		</dict>"
        "		<dict>"
        "			<key>available values</key>"
        "			<array>"
        "				<string>Recommended</string>"
        "				<string>8.000</string>"
        "				<string>11.025</string>"
        "				<string>12.000</string>"
        "				<string>16.000</string>"
        "				<string>22.050</string>"
        "				<string>24.000</string>"
        "				<string>32.000</string>"
        "				<string>44.100</string>"
        "				<string>48.000</string>"
        "				<string>52.000</string>"
        "			</array>"
        "			<key>current value</key>"
        "			<integer>8</integer>"
        "			<key>hint</key>"
        "			<integer>6</integer>"
        "			<key>key</key>"
        "			<string>Sample Rate</string>"
        "			<key>limited values</key>"
        "			<array>"
        "				<string>Recommended</string>"
        "				<string>32.000</string>"
        "				<string>44.100</string>"
        "			</array>"
        "			<key>name</key>"
        "			<string>Sample Rate</string>"
        "			<key>summary</key>"
        "			<string>The sample rate of the Vorbis bitstream produced by the encoder</string>"
        "			<key>unit</key>"
        "			<string>kHz</string>"
        "			<key>value type</key>"
        "			<integer>10</integer>"
        "		</dict>"
        "		<dict>"
        "			<key>available values</key>"
        "			<array>"
        "				<string>None</string>"
        "				<string>Bit Rate</string>"
        "				<string>Sample Rate</string>"
        "			</array>"
        "			<key>current value</key>"
        "			<integer>1</integer>"
        "			<key>hint</key>"
        "			<integer>13</integer>"
        "			<key>key</key>"
        "			<string>Precedence</string>"
        "			<key>name</key>"
        "			<string>Precedence</string>"
        "			<key>summary</key>"
        "			<string>If either the bit rate or sample rate is allowed to override the other setting</string>"
        "			<key>value type</key>"
        "			<integer>10</integer>"
        "		</dict>"
        "		<dict>"
        "			<key>available values</key>"
        "			<array>"
        "				<string>Good</string>"
        "				<string>Better</string>"
        "				<string>Best</string>"
        "			</array>"
        "			<key>current value</key>"
        "			<integer>1</integer>"
        "			<key>hint</key>"
        "			<integer>1</integer>"
        "			<key>key</key>"
        "			<string>Quality</string>"
        "			<key>name</key>"
        "			<string>Quality</string>"
        "			<key>summary</key>"
        "			<string>The quality of the encoded Vorbis bitstream</string>"
        "			<key>value type</key>"
        "			<integer>10</integer>"
        "		</dict>"
        "		<dict>"
        "			<key>available values</key>"
        "			<array>"
        "				<string>Mono</string>"
        "				<string>Stereo</string>"
        "				<string>2.0 (2.1?!)</string>"
        "				<string>Quadraphonic</string>"
        "				<string>5.0</string>"
        "				<string>5.1</string>"
        "			</array>"
        "			<key>current value</key>"
        "			<integer>1</integer>"
        "			<key>hint</key>"
        "			<integer>5</integer>"
        "			<key>key</key>"
        "			<string>Channel Configuration</string>"
        "			<key>limited values</key>"
        "			<array>"
        "				<string>Stereo</string>"
        "			</array>"
        "			<key>name</key>"
        "			<string>Channel Configuration</string>"
        "			<key>summary</key>"
        "			<string>The channel layout of the Vorbis bitstream produced by the encoder</string>"
        "			<key>value type</key>"
        "			<integer>10</integer>"
        "		</dict>"
        "	</array>"
        "</dict>"
        "</plist>";
    CFDataRef data = CFDataCreate(NULL, (UInt8 *) buffer, strlen(buffer));
    CFDictionaryRef dict = (CFDictionaryRef) CFPropertyListCreateFromXMLData(NULL, data, kCFPropertyListImmutable, NULL);
    if (dict != NULL) {
        *reinterpret_cast<CFDictionaryRef*>(outSettingsDict) = dict;
        return true;
    }
    return false;
}

void CAVorbisEncoder::InitializeCompressionSettings()
{
    int ret = 0;

    if (mCompressionInitialized) {
        vorbis_block_clear(&mV_vb);
        vorbis_dsp_clear(&mV_vd);

        vorbis_info_clear(&mV_vi);

        if (mCookie != NULL) {
            delete[] mCookie;
            mCookieSize = 0;
            mCookie = NULL;
        }
    }

    mCompressionInitialized = false;

    vorbis_info_init(&mV_vi);

    ret = vorbis_encode_init_vbr(&mV_vi, mOutputFormat.mChannelsPerFrame, lround(mOutputFormat.mSampleRate), 0.4);

    if (ret)
        CODEC_THROW(kAudioCodecUnspecifiedError);

    vorbis_comment_init(&mV_vc);
    vorbis_comment_add_tag(&mV_vc, "ENCODER", "XiphQT, CAVorbisEncoder.cpp $Rev$");

    vorbis_analysis_init(&mV_vd, &mV_vi);
    vorbis_block_init(&mV_vd, &mV_vb);


    {
        ogg_packet header, header_vc, header_cb;
        vorbis_analysis_headerout(&mV_vd, &mV_vc, &header, &header_vc, &header_cb);

        mCookieSize = header.bytes + header_vc.bytes + header_cb.bytes + 4 * 2 * sizeof(UInt32);
        mCookie = new Byte[mCookieSize];

        unsigned long *qtatom = reinterpret_cast<unsigned long*>(mCookie); // reinterpret_cast ?!?
        *qtatom++ = EndianU32_NtoB(header.bytes + 2 * sizeof(UInt32));
        *qtatom++ = EndianU32_NtoB(kCookieTypeVorbisHeader);
        BlockMoveData(header.packet, qtatom, header.bytes);

        qtatom = reinterpret_cast<unsigned long*>(mCookie + 2 * sizeof(UInt32) + header.bytes);
        *qtatom++ = EndianU32_NtoB(header_vc.bytes + 2 * sizeof(UInt32));
        *qtatom++ = EndianU32_NtoB(kCookieTypeVorbisComments);
        BlockMoveData(header_vc.packet, qtatom, header_vc.bytes);

        qtatom = reinterpret_cast<unsigned long*>(mCookie + 4 * sizeof(UInt32) + header.bytes + header_vc.bytes);
        *qtatom++ = EndianU32_NtoB(header_cb.bytes + 2 * sizeof(UInt32));
        *qtatom++ = EndianU32_NtoB(kCookieTypeVorbisCodebooks);
        BlockMoveData(header_cb.packet, qtatom, header_cb.bytes);

        qtatom = reinterpret_cast<unsigned long*>(mCookie + 6 * sizeof(UInt32) + header.bytes + header_vc.bytes + header_cb.bytes);
        *qtatom++ = EndianU32_NtoB(2 * sizeof(UInt32));
        *qtatom++ = EndianU32_NtoB(kAudioTerminatorAtomType);
    }

    dbg_printf("[  VE] < > [%08lx] :: InitializeCompressionSettings() - bru: %ld, brn: %ld, brl: %ld, brw: %ld\n",
               (UInt32) this, mV_vi.bitrate_upper, mV_vi.bitrate_nominal, mV_vi.bitrate_lower, mV_vi.bitrate_window);
    mCompressionInitialized = true;
}

#pragma mark BDC handling

void CAVorbisEncoder::BDCInitialize(UInt32 inInputBufferByteSize)
{
    XCACodec::BDCInitialize(inInputBufferByteSize);
}

void CAVorbisEncoder::BDCUninitialize()
{
    mVorbisFPList.clear();
    mProducedPList.clear();

    XCACodec::BDCUninitialize();
}

void CAVorbisEncoder::BDCReset()
{
    mVorbisFPList.clear();
    mProducedPList.clear();

    XCACodec::BDCReset();
}

void CAVorbisEncoder::BDCReallocate(UInt32 inInputBufferByteSize)
{
    mVorbisFPList.clear();
    mProducedPList.clear();

    XCACodec::BDCReallocate(inInputBufferByteSize);
}


void CAVorbisEncoder::InPacket(const void* inInputData, const AudioStreamPacketDescription* inPacketDescription)
{
    const Byte * theData = static_cast<const Byte *> (inInputData) + inPacketDescription->mStartOffset;
    UInt32 size = inPacketDescription->mDataByteSize;
    mBDCBuffer.In(theData, size);
    mVorbisFPList.push_back(VorbisFramePacket(inPacketDescription->mVariableFramesInPacket, inPacketDescription->mDataByteSize));
}


UInt32 CAVorbisEncoder::ProduceOutputPackets(void* outOutputData, UInt32& ioOutputDataByteSize, UInt32& ioNumberPackets,
                                             AudioStreamPacketDescription* outPacketDescription)
{
    dbg_printf("[  VE]  >> [%08lx] CAVorbisEncoder :: ProduceOutputPackets(%ld [%ld] %d)\n", (UInt32) this, ioNumberPackets, ioOutputDataByteSize, outPacketDescription != NULL);

    UInt32 theAnswer = kAudioCodecProduceOutputPacketSuccess;

    if (!mIsInitialized)
        CODEC_THROW(kAudioCodecStateError);

    UInt32 frames = 0;
    UInt32 fout = 0; //frames produced
    UInt32 bout = 0; //bytes produced
    UInt32 bspace = ioOutputDataByteSize;
    ogg_packet op;

    while (fout < ioNumberPackets && !mEOSHit) {
        if (!mProducedPList.empty()) {
            ogg_packet &op_left = mProducedPList.front();
            if (op_left.bytes > bspace) {
                ioNumberPackets = fout;
                ioOutputDataByteSize = bout;
                theAnswer = kAudioCodecNotEnoughBufferSpaceError;
                dbg_printf("[  VE] <.! [%08lx] CAVorbisEncoder :: ProduceOutputPackets(%ld [%ld]) = %ld\n", (UInt32) this,
                           ioNumberPackets, ioOutputDataByteSize, theAnswer);
                return theAnswer;
            }

            BlockMoveData(op_left.packet, &((static_cast<Byte *>(outOutputData))[bout]), op_left.bytes);
            outPacketDescription[fout].mStartOffset = bout;
            outPacketDescription[fout].mVariableFramesInPacket = op_left.granulepos - last_granulepos;
            outPacketDescription[fout].mDataByteSize = op_left.bytes;
            last_granulepos = op_left.granulepos;
            fout++;
            bout += op_left.bytes;
            bspace -= op_left.bytes;
            if (op_left.e_o_s != 0)
                mEOSHit = true;
            mProducedPList.erase(mProducedPList.begin());
            continue;
        } else if (vorbis_bitrate_flushpacket(&mV_vd, &op)) {
            if (op.bytes > bspace) {
                ioNumberPackets = fout;
                ioOutputDataByteSize = bout;
                theAnswer = kAudioCodecNotEnoughBufferSpaceError;
                mProducedPList.push_back(op);
                dbg_printf("[  VE] <.! [%08lx] CAVorbisEncoder :: ProduceOutputPackets(%ld [%ld]) = %ld\n", (UInt32) this,
                           ioNumberPackets, ioOutputDataByteSize, theAnswer);
                return theAnswer;
            }

            BlockMoveData(op.packet, &((static_cast<Byte *>(outOutputData))[bout]), op.bytes);
            outPacketDescription[fout].mStartOffset = bout;
            outPacketDescription[fout].mVariableFramesInPacket = op.granulepos - last_granulepos;
            outPacketDescription[fout].mDataByteSize = op.bytes;
            last_granulepos = op.granulepos;
            fout++;
            bout += op.bytes;
            bspace -= op.bytes;
            if (op.e_o_s != 0)
                mEOSHit = true;
            continue;
        } else if (vorbis_analysis_blockout(&mV_vd, &mV_vb) == 1) {
            vorbis_analysis(&mV_vb, NULL);
            vorbis_bitrate_addblock(&mV_vb);
            continue;
        }

        // get next block
        if (mVorbisFPList.empty()) {
            if (fout == 0) {
                ioNumberPackets = fout;
                ioOutputDataByteSize = bout;
                theAnswer = kAudioCodecProduceOutputPacketNeedsMoreInputData;
                dbg_printf("[  VE] <!. [%08lx] CAVorbisEncoder :: ProduceOutputPackets(%ld [%ld]) = %ld\n", (UInt32) this,
                           ioNumberPackets, ioOutputDataByteSize, theAnswer);
                return theAnswer;
            }
            break;
        }

        VorbisFramePacket &sfp = mVorbisFPList.front();
        if (sfp.frames > 0) {
            float **buffer = vorbis_analysis_buffer(&mV_vd, sfp.frames);
            void *inData = mBDCBuffer.GetData();
            int j = 0, i = 0;

            if (mInputFormat.mFormatFlags & kAudioFormatFlagIsSignedInteger) {
                for (j = 0; j < mOutputFormat.mChannelsPerFrame; j++) {
                    SInt16 *theInputData = static_cast<SInt16*> (inData) + j;
                    float *theOutputData = buffer[j];
                    for (i = 0; i < sfp.frames; i++) {
                        *theOutputData++ = *theInputData / 32768.0f;
                        theInputData += mInputFormat.mChannelsPerFrame;
                    }
                }
            } else {
                for (j = 0; j < mOutputFormat.mChannelsPerFrame; j++) {
                    float *theInputData = static_cast<float*> (inData) + j;
                    float *theOutputData = buffer[j];
                    for (i = 0; i < sfp.frames; i++) {
                        *theOutputData++ = *theInputData;
                        theInputData += mInputFormat.mChannelsPerFrame;
                    }
                }
            }
        }

        vorbis_analysis_wrote(&mV_vd, sfp.frames);
        mBDCBuffer.Zap(sfp.bytes);
        sfp.frames = sfp.left = 0;
        mVorbisFPList.erase(mVorbisFPList.begin());
    }

    ioOutputDataByteSize = bout; //???
    ioNumberPackets = fout;

    //theAnswer = (!mProducedPList.empty() || !BufferIsEmpty()) ? kAudioCodecProduceOutputPacketSuccessHasMore
    theAnswer = (!mProducedPList.empty() || !mVorbisFPList.empty()) ? kAudioCodecProduceOutputPacketSuccessHasMore
        : kAudioCodecProduceOutputPacketSuccess; // what about possible data inside the vorbis lib (if ..._flushpacket() returns more than 1 packet) ??

    if (mEOSHit)
        theAnswer = kAudioCodecProduceOutputPacketAtEOF;
    dbg_printf("[  VE] <.. [%08lx] CAVorbisEncoder :: ProduceOutputPackets(%ld [%ld]) = %ld\n",
               (UInt32) this, ioNumberPackets, ioOutputDataByteSize, theAnswer);
    return theAnswer;
}
