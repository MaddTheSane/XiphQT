/*
 *  OggImport.r
 *
 *    Information bit definitions for the 'thng' and other OggImport
 *    resources.
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


#define thng_RezTemplateVersion 2

#define cfrg_RezTemplateVersion 1

#ifdef TARGET_REZ_MAC_PPC
#define TARGET_REZ_CARBON_MACHO 1
#include <CoreServices/CoreServices.r>
#include <QuickTime/QuickTime.r>
#include <QuickTime/QuickTimeComponents.r>
#else
#include "ConditionalMacros.r"
#include "CoreServices.r"
#include "QuickTimeComponents.r"
#endif

#include "OggImport.h"

#define kImporterComponentType 'eat '

#ifndef cmpThreadSafe
#define cmpThreadSafe	0x10000000
#endif

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Ogg Importer

#define kImporterFlags canMovieImportFiles | canMovieImportValidateFile | \
    canMovieImportPartial | canMovieImportInPlace | hasMovieImportMIMEList | \
    canMovieImportDataReferences | canMovieImportWithIdle |             \
    canMovieImportAvoidBlocking | canMovieImportValidateDataReferences | \
    cmpThreadSafe


resource 'thng' (kImporterResID, OggImporterName, purgeable) {
    kImporterComponentType, kCodecFormat, 'soun',
    0, 0, 0, 0,
    'STR ', kImporterNameStringResID,
    'STR ', kImporterInfoStringResID,
    0, 0,		// no icon
    kOgg_eat__Version,
    componentDoAutoVersion|componentHasMultiplePlatforms, 0,
    {
#if TARGET_OS_MAC	       // COMPONENT PLATFORM INFORMATION ----------------------
        kImporterFlags,
        'dlle',                                 // Code Resource type - Entry point found by symbol name 'dlle' resource
        kImporterResID,                         // ID of 'dlle' resource
        platformPowerPCNativeEntryPoint,
#endif
#if TARGET_OS_WIN32
    kImporterFlags,
    'dlle',
    kImporterResID,
    platformWin32,
#endif
    },
    'thnr', kImporterResID
};

// Component Alias
resource 'thga' (kImporterResID, OggImporterName, purgeable) {
    kImporterComponentType,             // Type
    'OGG ',                             // Subtype - this must be in uppercase. It will match an ".ogg" suffix case-insensitively.
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
    'soun',                             // Manufaturer
    kImporterFlags,                     // Component Flags
    0,                                  // Component Flags Mask
    'thnr', kImporterResID, 0
};

// Component Alias
resource 'thga' (kImporterResID + 1, OggImporterName, purgeable) {
    kImporterComponentType,             // Type
    'OGM ',                             // Subtype - this must be in uppercase. It will match an ".ogm" suffix case-insensitively.
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
    'soun',                             // Manufaturer
    kImporterFlags,                     // Component Flags
    0,                                  // Component Flags Mask
    'thnr', kImporterResID, 0
};

// Component Alias
resource 'thga' (kImporterResID + 2, OggImporterName, purgeable) {
    kImporterComponentType,             // Type
    'SPX ',                             // Subtype - this must be in uppercase. It will match an ".ogm" suffix case-insensitively.
    'soun',                             // Manufacturer
//    kImporterFlags /*| movieImportSubTypeIsFileExtension */,	// The .spx extension is used by System Profiler
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
    'soun',                             // Manufaturer
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


//#if defined(TARGET_REZ_CARBON_MACHO) || defined(TARGET_REZ_WIN32)
resource 'dlle' (kImporterResID, OggImporterName) {
    "OggImportComponentDispatch"
};
//#endif

// name and info string are shared by the compressor and decompressor
resource 'STR ' (kImporterNameStringResID, OggImporterName, purgeable) {
    "Ogg Importer"
};

resource 'STR ' (kImporterInfoStringResID, OggImporterName, purgeable) {
    "Ogg " "0.1.1" " (See http://www.xiph.org)."
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
        kMimeInfoFileExtensionTag, 1, "ogg";
        kMimeInfoFileExtensionTag, 2, "ogg";
        kMimeInfoFileExtensionTag, 3, "spx";
        kMimeInfoFileExtensionTag, 4, "spx";
        kMimeInfoDescriptionTag,   1, "Ogg Vorbis";
        kMimeInfoDescriptionTag,   2, "Ogg Vorbis";
        kMimeInfoDescriptionTag,   3, "Ogg Speex";
        kMimeInfoDescriptionTag,   4, "Ogg Speex";
    };
};

resource 'mcfg' (kImporterResID, OggImporterName, purgeable) {
    kVersionDoesntMatter,
    {
        kQTMediaConfigAudioGroupID,
        kQTMediaConfigBinaryFile | \
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
            "Ogg Vorbis sound file",
            "ogg",
            "QuickTime Player",
            "Ogg Vorbis file importer",
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
            "Ogg Speex sound file",
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
    };
};

#if TARGET_OS_WIN32
read 'MDCf' (kImporterResID) "../MetaDataConfig.plist";
#endif
