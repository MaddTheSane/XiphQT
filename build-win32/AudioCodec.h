/*
 *  AudioCodec.h
 *
 *    CoreAudio AudioCodec definitions missing from Win32 QT SDK.
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


#if !defined(__audiocodec_h__)
#define __audiocodec_h__

// Apple SDK files depend on the following macro to be defined in AudioCodec.h
#define __AudioCodec_h__

//=============================================================================
//	Includes
//=============================================================================

#include <TargetConditionals.h>
#include <AvailabilityMacros.h>

#if !defined(__COREAUDIO_USE_FLAT_INCLUDES__)
	#include <CoreServices/CoreServices.h>
	#include <CoreAudio/CoreAudioTypes.h>
#else
	#include "Components.h"
	#include "CoreAudioTypes.h"
#endif

#if defined(__cplusplus)
extern "C"
{
#endif

struct AudioStreamPacketDescription
{
    SInt64  mStartOffset;
    UInt32  mVariableFramesInPacket;
    UInt32  mDataByteSize;
};
typedef struct AudioStreamPacketDescription AudioStreamPacketDescription;

typedef ComponentInstance  AudioCodec;
typedef UInt32             AudioCodecPropertyID;

enum
{
    kAudioDecoderComponentType     = 'adec',
    kAudioEncoderComponentType     = 'aenc',
    kAudioUnityCodecComponentType  = 'acdc'
};

enum
{
    kAudioCodecPropertyNameCFString                   = 'lnam',
    kAudioCodecPropertyManufacturerCFString           = 'lmak',
    kAudioCodecPropertyFormatCFString                 = 'lfor',
    kAudioCodecPropertyRequiresPacketDescription      = 'pakd',
    kAudioCodecPropertyPacketFrameSize                = 'pakf',
    kAudioCodecPropertyHasVariablePacketByteSizes     = 'vpk?',
    kAudioCodecPropertyMaximumPacketByteSize          = 'pakb',
    kAudioCodecPropertyCurrentInputFormat             = 'ifmt',
    kAudioCodecPropertySupportedInputFormats          = 'ifm#',
    kAudioCodecPropertyCurrentOutputFormat            = 'ofmt',
    kAudioCodecPropertySupportedOutputFormats         = 'ofm#',
    kAudioCodecPropertyMagicCookie                    = 'kuki',
    kAudioCodecPropertyInputBufferSize                = 'tbuf',
    kAudioCodecPropertyUsedInputBufferSize            = 'ubuf',
    kAudioCodecPropertyIsInitialized                  = 'init',
    kAudioCodecPropertyCurrentTargetBitRate           = 'brat',
    kAudioCodecPropertyAvailableBitRates              = 'brt#',
    kAudioCodecPropertyCurrentInputSampleRate         = 'cisr',
    kAudioCodecPropertyCurrentOutputSampleRate        = 'cosr',
    kAudioCodecPropertyAvailableInputSampleRates      = 'aisr',
    kAudioCodecPropertyAvailableOutputSampleRates     = 'aosr',
    kAudioCodecPropertyQualitySetting                 = 'srcq',
    kAudioCodecPropertyCurrentLoudnessStatistics      = 'loud',
    kAudioCodecPropertyAvailableBitRateRange          = 'abrt',
    kAudioCodecPropertyApplicableBitRateRange         = 'brta',
    kAudioCodecPropertyApplicableInputSampleRates     = 'isra',
    kAudioCodecPropertyApplicableOutputSampleRates    = 'osra',
    kAudioCodecPropertyMinimumNumberInputPackets      = 'mnip',
    kAudioCodecPropertyMinimumNumberOutputPackets     = 'mnop',
    kAudioCodecPropertyZeroFramesPadded               = 'pad0',
    kAudioCodecPropertyAvailableNumberChannels        = 'cmnc',
    kAudioCodecPropertyPrimeMethod                    = 'prmm',
    kAudioCodecPropertyPrimeInfo                      = 'prim',
    kAudioCodecDoesSampleRateConversion               = 'lmrc',
    kAudioCodecPropertyInputChannelLayout             = 'icl ',
    kAudioCodecPropertyOutputChannelLayout            = 'ocl ',
    kAudioCodecPropertyAvailableInputChannelLayouts   = 'aicl',
    kAudioCodecPropertyAvailableOutputChannelLayouts  = 'aocl',
    kAudioCodecPropertySettings                       = 'acs ',
    kAudioCodecBitRateFormat                          = 'acbf',
    kAudioCodecExtendFrequencies                      = 'acef',
    kAudioCodecInputFormatsForOutputFormat            = 'if4o',
    kAudioCodecOutputFormatsForInputFormat            = 'of4i',
    kAudioCodecUseRecommendedSampleRate               = 'ursr',
    kAudioCodecOutputPrecedence                       = 'oppr'
};

enum
{
    kAudioCodecQuality_Max     = 0x7F,
    kAudioCodecQuality_High    = 0x60,
    kAudioCodecQuality_Medium  = 0x40,
    kAudioCodecQuality_Low     = 0x20,
    kAudioCodecQuality_Min     = 0
};

enum
{
    kAudioCodecPrimeMethod_Pre     = 0,
    kAudioCodecPrimeMethod_Normal  = 1,
    kAudioCodecPrimeMethod_None    = 2
};

enum
{
    kAudioCodecBitRateFormat_CBR  = 0,
    kAudioCodecBitRateFormat_ABR  = 1,
    kAudioCodecBitRateFormat_VBR  = 2
};

enum
{
    kAudioCodecOutputPrecedenceNone        = 0,
    kAudioCodecOutputPrecedenceBitRate     = 1,
    kAudioCodecOutputPrecedenceSampleRate  = 2
};

typedef struct AudioCodecPrimeInfo
{
    UInt32  leadingFrames;
    UInt32  trailingFrames;
} AudioCodecPrimeInfo;

#define kAudioSettings_TopLevelKey      "name"
#define kAudioSettings_Version          "version"
#define kAudioSettings_Parameters       "parameters"
#define kAudioSettings_SettingKey       "key"
#define kAudioSettings_SettingName      "name"
#define kAudioSettings_ValueType        "value type"
#define kAudioSettings_AvailableValues  "available values"
#define kAudioSettings_LimitedValues    "limited values"
#define kAudioSettings_CurrentValue     "current value"
#define kAudioSettings_Summary          "summary"
#define kAudioSettings_Hint             "hint"
#define kAudioSettings_Unit             "unit"

enum
{
    kHintBasic     = 0,
    kHintAdvanced  = 1,
    kHintHidden    = 2
};

enum {
    kAudioSettingsFlags_ExpertParameter = (1L << 0),
    kAudioSettingsFlags_InvisibleParameter = (1L << 1),
    kAudioSettingsFlags_MetaParameter = (1L << 2),
    kAudioSettingsFlags_UserInterfaceParameter = (1L << 3)
};

enum
{
    kAudioCodecProduceOutputPacketFailure             = 1,
    kAudioCodecProduceOutputPacketSuccess             = 2,
    kAudioCodecProduceOutputPacketSuccessHasMore      = 3,
    kAudioCodecProduceOutputPacketNeedsMoreInputData  = 4,
    kAudioCodecProduceOutputPacketAtEOF               = 5
};

enum
{
    kAudioCodecGetPropertyInfoSelect    = 0x0001,
    kAudioCodecGetPropertySelect        = 0x0002,
    kAudioCodecSetPropertySelect        = 0x0003,
    kAudioCodecInitializeSelect         = 0x0004,
    kAudioCodecUninitializeSelect       = 0x0005,
    kAudioCodecAppendInputDataSelect    = 0x0006,
    kAudioCodecProduceOutputDataSelect  = 0x0007,
    kAudioCodecResetSelect              = 0x0008
};

enum
{
    kAudioCodecNoError                    = 0,
    kAudioCodecUnspecifiedError           = 'what',
    kAudioCodecUnknownPropertyError       = 'who?',
    kAudioCodecBadPropertySizeError       = '!siz',
    kAudioCodecIllegalOperationError      = 'nope',
    kAudioCodecUnsupportedFormatError     = '!dat',
    kAudioCodecStateError                 = '!stt',
    kAudioCodecNotEnoughBufferSpaceError  = '!buf'
};


#if defined(__cplusplus)
}
#endif

#endif /* __audiocodec_h__ */
