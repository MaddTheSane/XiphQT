/*
 *  OggImport.r
 *
 *    Information bit definitions for the 'thng' and other OggImport
 *    resources.
 *
 *
 *  Copyright (c) 2005-2006  Arek Korbik
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


#define thng_RezTemplateVersion 2

#define cfrg_RezTemplateVersion 1

#ifdef TARGET_REZ_MAC_PPC
#include <CoreServices/CoreServices.r>
#include <QuickTime/QuickTime.r>
#include <QuickTime/QuickTimeComponents.r>
#else
#include "ConditionalMacros.r"
#include "CoreServices.r"
#include "QuickTimeComponents.r"
#endif /* TARGET_REZ_MAC_PPC */

#include "OggImport.h"

#define kImporterComponentType 'eat '


/* How do I do this properly... anybody? */
#if defined(BUILD_UNIVERSAL)
  #define TARGET_CPU_PPC 1
  #define TARGET_CPU_X86 1
#endif


#ifndef cmpThreadSafe
#define cmpThreadSafe	0x10000000
#endif

#if TARGET_OS_MAC
  #if TARGET_CPU_PPC && TARGET_CPU_X86
    #define TARGET_REZ_FAT_COMPONENTS 1
    #define Target_PlatformType       platformPowerPCNativeEntryPoint
    #define Target_SecondPlatformType platformIA32NativeEntryPoint
  #elif TARGET_CPU_X86
    #define Target_PlatformType       platformIA32NativeEntryPoint
  #else
    #define Target_PlatformType       platformPowerPCNativeEntryPoint
  #endif
#elif TARGET_OS_WIN32
  #define Target_PlatformType platformWin32
#else
  #error get a real platform type
#endif /* TARGET_OS_MAC */

#if !defined(TARGET_REZ_FAT_COMPONENTS)
  #define TARGET_REZ_FAT_COMPONENTS 0
#endif

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Ogg Importer

#define kImporterFlags canMovieImportFiles | canMovieImportValidateFile | \
    canMovieImportPartial | canMovieImportInPlace | hasMovieImportMIMEList | \
    canMovieImportDataReferences | canMovieImportWithIdle |             \
    canMovieImportAvoidBlocking | canMovieImportValidateDataReferences | \
    cmpThreadSafe


resource 'thng' (kImporterResID, OggImporterName, purgeable) {
    kImporterComponentType, kCodecFormat, 'vide',
    0, 0, 0, 0,
    'STR ', kImporterNameStringResID,
    'STR ', kImporterInfoStringResID,
    0, 0,		// no icon
    kOgg_eat__Version,
    componentDoAutoVersion|componentHasMultiplePlatforms, 0,
    {
        // COMPONENT PLATFORM INFORMATION ----------------------
        kImporterFlags,
        'dlle',                                 // Code Resource type - Entry point found by symbol name 'dlle' resource
        kImporterResID,                         // ID of 'dlle' resource
        Target_PlatformType,
#if TARGET_REZ_FAT_COMPONENTS
        kImporterFlags,
        'dlle',
        kImporterResID,
        Target_SecondPlatformType,
#endif
    },
    'thnr', kImporterResID
};

// Component Alias
resource 'thga' (kImporterResID, OggImporterName, purgeable) {
    kImporterComponentType,             // Type
    'OGG ',                             // Subtype - this must be in uppercase.
                                        // It will match an ".ogg" suffix case-insensitively.
    'vide',                             // Manufacturer
    kImporterFlags | movieImportSubTypeIsFileExtension,	// The subtype is a file name suffix
    0,                                  // Component Flags Mask
    0,                                  // Code Type
    0,                                  // Code ID
    'STR ',                             // Name Type
    kImporterNameStringResID,           // Name ID
    'STR ',                             // Info Type
    kImporterInfoStringResID,           // Info ID
    0,                                  // Icon Type
    0,                                  // Icon ID
                // TARGET COMPONENT ---------------
    kImporterComponentType,             // Type
    kCodecFormat,                       // SubType
    'vide',                             // Manufaturer
    kImporterFlags,                     // Component Flags
    0,                                  // Component Flags Mask
    'thnr', kImporterResID, 0
};

// Component Alias
resource 'thga' (kImporterResID + 1, OggImporterName, purgeable) {
    kImporterComponentType,             // Type
    'OGM ',                             // Subtype - this must be in uppercase.
                                        // It will match an ".ogm" suffix case-insensitively.
    'vide',                             // Manufacturer
    kImporterFlags | movieImportSubTypeIsFileExtension,	// The subtype is a file name suffix
    0,                                  // Component Flags Mask
    0,                                  // Code Type
    0,                                  // Code ID
    'STR ',                             // Name Type
    kImporterNameStringResID,           // Name ID
    'STR ',                             // Info Type
    kImporterInfoStringResID,           // Info ID
    0,                                  // Icon Type
    0,                                  // Icon ID
                // TARGET COMPONENT ---------------
    kImporterComponentType,             // Type
    kCodecFormat,                       // SubType
    'vide',                             // Manufaturer
    kImporterFlags,                     // Component Flags
    0,                                  // Component Flags Mask
    'thnr', kImporterResID, 0
};

// Component Alias
resource 'thga' (kImporterResID + 2, OggImporterName, purgeable) {
    kImporterComponentType,             // Type
    'SPX ',                             // Subtype - this must be in uppercase.
                                        // It will match an ".spx" suffix case-insensitively.
    'soun',                             // Manufacturer
    kImporterFlags | movieImportSubTypeIsFileExtension,	// The subtype is a file name suffix
    0,                                  // Component Flags Mask
    0,                                  // Code Type
    0,                                  // Code ID
    'STR ',                             // Name Type
    kImporterNameStringResID,           // Name ID
    'STR ',                             // Info Type
    kImporterInfoStringResID,           // Info ID
    0,                                  // Icon Type
    0,                                  // Icon ID
                                        // TARGET COMPONENT ---------------
    kImporterComponentType,             // Type
    kCodecFormat,                       // SubType
    'vide',                             // Manufaturer
    kImporterFlags,                     // Component Flags
    0,                                  // Component Flags Mask
    'thnr', kImporterResID, 0
};

// Component Alias
resource 'thga' (kImporterResID + 3, OggImporterName, purgeable) {
    kImporterComponentType,             // Type
    'ANX ',                             // Subtype - this must be in uppercase.
                                        // It will match an ".anx" suffix case-insensitively.
    'vide',                             // Manufacturer
    kImporterFlags | movieImportSubTypeIsFileExtension,	// The subtype is a file name suffix
    0,                                  // Component Flags Mask
    0,                                  // Code Type
    0,                                  // Code ID
    'STR ',                             // Name Type
    kImporterNameStringResID,           // Name ID
    'STR ',                             // Info Type
    kImporterInfoStringResID,           // Info ID
    0,                                  // Icon Type
    0,                                  // Icon ID
                // TARGET COMPONENT ---------------
    kImporterComponentType,             // Type
    kCodecFormat,                       // SubType
    'vide',                             // Manufaturer
    kImporterFlags,                     // Component Flags
    0,                                  // Component Flags Mask
    'thnr', kImporterResID, 0
};

resource 'thnr' (kImporterResID, OggImporterName, purgeable) {
    {
        'mime', 1, 0, 'mime', kImporterResID, cmpResourceNoFlags,
        'mcfg', 1, 0, 'mcfg', kImporterResID, cmpResourceNoFlags,
    };
};


resource 'dlle' (kImporterResID, OggImporterName) {
    "OggImportComponentDispatch"
};

// name and info string are shared by the compressor and decompressor
resource 'STR ' (kImporterNameStringResID, OggImporterName, purgeable) {
    "Ogg Importer"
};

resource 'STR ' (kImporterInfoStringResID, OggImporterName, purgeable) {
    "Ogg " "0.1.6" " (See http://www.xiph.org)."
};


/*
    This is an example of how to build an atom container resource to hold mime types.
    This component's GetMIMETypeList implementation simply loads this resource and returns it.
    Please note that atoms of the same type MUST be grouped together within an atom container.
*/
resource 'mime' (kImporterResID, OggImporterName, purgeable) {
    {
        kMimeInfoMimeTypeTag,      1, "application/ogg";
        kMimeInfoMimeTypeTag,      2, "application/x-ogg";
        kMimeInfoMimeTypeTag,      3, "audio/x-speex";
        kMimeInfoMimeTypeTag,      4, "audio/speex";
        kMimeInfoMimeTypeTag,      5, "application/x-annodex";
        kMimeInfoFileExtensionTag, 1, "ogg";
        kMimeInfoFileExtensionTag, 2, "ogg";
        kMimeInfoFileExtensionTag, 3, "spx";
        kMimeInfoFileExtensionTag, 4, "spx";
        kMimeInfoFileExtensionTag, 5, "anx";
        kMimeInfoDescriptionTag,   1, "Ogg Multimedia Bitstream";
        kMimeInfoDescriptionTag,   2, "Ogg Multimedia Bitstream";
        kMimeInfoDescriptionTag,   3, "Ogg Speex";
        kMimeInfoDescriptionTag,   4, "Ogg Speex";
        kMimeInfoDescriptionTag,   5, "Annodex Media";
    };
};

resource 'mcfg' (kImporterResID, OggImporterName, purgeable) {
    kVersionDoesntMatter,
    {
        kQTMediaConfigVideoGroupID,
        kQTMediaConfigBinaryFile | \
            kQTMediaConfigTakeFileAssociationByDefault | \
            kQTMediaConfigCanUseApp | kQTMediaConfigCanUsePlugin | \
            kQTMediaConfigUsePluginByDefault,
        'OggS',
        'TVOD',	/* we don't have a creator code for our files, hijack QT player */
        kImporterComponentType, kCodecFormat, kSoundComponentManufacturer,
        0, 0,
        'OGG ',
        kQTMediaInfoNetGroup,

        /* no synonyms */
        {
        },

        {
            "Ogg Multimedia Bitstream",
            "ogg",
            "QuickTime Player",
            "Ogg file importer",
            "",
        },

        /* mime types array */
        {
            "application/x-ogg";
            "application/ogg";
        };
//    };
//    {
        kQTMediaConfigAudioGroupID,
        kQTMediaConfigBinaryFile | \
            kQTMediaConfigTakeFileAssociationByDefault | \
            kQTMediaConfigCanUseApp | kQTMediaConfigCanUsePlugin | \
            kQTMediaConfigUsePluginByDefault,
        'OggS',
        'TVOD',	/* we don't have a creator code for our files, hijack QT player */
        kImporterComponentType, kCodecFormat, kSoundComponentManufacturer,
        0, 0,
        'SPX ',
        kQTMediaInfoNetGroup,

        /* no synonyms */
        {
        },

        {
            "Ogg Speex audio",
            "spx",
            "QuickTime Player",
            "Ogg Speex file importer",
            "",
        },

        /* mime types array */
        {
            "audio/x-speex";
            "audio/speex";
        };
//    };
//    {
        kQTMediaConfigVideoGroupID,
        kQTMediaConfigBinaryFile | \
            kQTMediaConfigTakeFileAssociationByDefault | \
            kQTMediaConfigCanUseApp | kQTMediaConfigCanUsePlugin | \
            kQTMediaConfigUsePluginByDefault,
        'OggS',
        'TVOD',	/* we don't have a creator code for our files, hijack QT player */
        kImporterComponentType, kCodecFormat, kSoundComponentManufacturer,
        0, 0,
        'ANX ',
        kQTMediaInfoNetGroup,

        /* no synonyms */
        {
        },

        {
            "Annodex Media",
            "anx",
            "QuickTime Player",
            "Annodex file importer",
            "",
        },

        /* mime types array */
        {
            "application/x-annodex";
        };
    };
};

#if TARGET_OS_WIN32
read 'MDCf' (kImporterResID) "../MetaDataConfig.plist";
#endif
