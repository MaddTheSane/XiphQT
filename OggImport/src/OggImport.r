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

#ifdef TARGET_REZ_MAC
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
		#if defined(ppc_YES)
		kImporterFlags,
		'dlle',
		kImporterResID,
		platformPowerPCNativeEntryPoint
		#define NeedLeadingComma 1
		#endif
		#if defined(ppc64_YES)
		#if defined(NeedLeadingComma)
		,
		#endif
		kImporterFlags,
		'dlle',
		kImporterResID,
		platformPowerPC64NativeEntryPoint
		#define NeedLeadingComma 1
		#endif
		#if defined(i386_YES)
		#if defined(NeedLeadingComma)
		,
		#endif
		kImporterFlags,
		'dlle',
		kImporterResID,
		platformIA32NativeEntryPoint
		#define NeedLeadingComma 1
		#endif
		#if defined(x86_64_YES)
		#if defined(NeedLeadingComma)
		,
		#endif
		kImporterFlags,
		'dlle',
		kImporterResID,
		platformX86_64NativeEntryPoint
		#define NeedLeadingComma 1
		#endif
		#if defined(TARGET_OS_WIN32) && TARGET_OS_WIN32
		kImporterFlags,
		'dlle',
		kImporterResID,
		platformWin32
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

// support the new proposed file extensions, see:
// http://wiki.xiph.org/index.php/MIME_Types_and_File_Extensions

//   .ogx - Ogg Multiplex Profile (anything in Ogg)
resource 'thga' (kImporterResID + 4, OggImporterName, purgeable) {
    kImporterComponentType,             // Type
    'OGX ',                             // Subtype
    'vide',                             // Manufacturer
    kImporterFlags | movieImportSubTypeIsFileExtension,
    0,                                  // Component Flags Mask
    0, 0,                               // Code Type, Code ID
    'STR ',                             // Name Type
    kImporterNameStringResID,           // Name ID
    'STR ',                             // Info Type
    kImporterInfoStringResID,           // Info ID
    0, 0,                               // Icon Type, Icon ID
                // TARGET COMPONENT ---------------
    kImporterComponentType,             // Type
    kCodecFormat,                       // SubType
    'vide',                             // Manufaturer
    kImporterFlags, 0,                  // Component Flags and Mask
    'thnr', kImporterResID, 0
};

//   .oga - Ogg Audio Profile (audio in Ogg container)
resource 'thga' (kImporterResID + 5, OggImporterName, purgeable) {
    kImporterComponentType,             // Type
    'OGA ',                             // Subtype
    'vide',                             // Manufacturer
    kImporterFlags | movieImportSubTypeIsFileExtension,
    0,                                  // Component Flags Mask
    0, 0,                               // Code Type, Code ID
    'STR ',                             // Name Type
    kImporterNameStringResID,           // Name ID
    'STR ',                             // Info Type
    kImporterInfoStringResID,           // Info ID
    0, 0,                               // Icon Type, Icon ID
                // TARGET COMPONENT ---------------
    kImporterComponentType,             // Type
    kCodecFormat,                       // SubType
    'vide',                             // Manufaturer
    kImporterFlags, 0,                  // Component Flags and Mask
    'thnr', kImporterResID, 0
};

//   .ogv - Ogg Video Profile (a/v in Ogg container)
resource 'thga' (kImporterResID + 6, OggImporterName, purgeable) {
    kImporterComponentType,             // Type
    'OGV ',                             // Subtype
    'vide',                             // Manufacturer
    kImporterFlags | movieImportSubTypeIsFileExtension,
    0,                                  // Component Flags Mask
    0, 0,                               // Code Type, Code ID
    'STR ',                             // Name Type
    kImporterNameStringResID,           // Name ID
    'STR ',                             // Info Type
    kImporterInfoStringResID,           // Info ID
    0, 0,                               // Icon Type, Icon ID
                // TARGET COMPONENT ---------------
    kImporterComponentType,             // Type
    kCodecFormat,                       // SubType
    'vide',                             // Manufaturer
    kImporterFlags, 0,                  // Component Flags and Mask
    'thnr', kImporterResID, 0
};

//   .axa - Profile for audio in Annodex
resource 'thga' (kImporterResID + 7, OggImporterName, purgeable) {
    kImporterComponentType,             // Type
    'AXA ',                             // Subtype
    'vide',                             // Manufacturer
    kImporterFlags | movieImportSubTypeIsFileExtension,
    0,                                  // Component Flags Mask
    0, 0,                               // Code Type, Code ID
    'STR ',                             // Name Type
    kImporterNameStringResID,           // Name ID
    'STR ',                             // Info Type
    kImporterInfoStringResID,           // Info ID
    0, 0,                               // Icon Type, Icon ID
                // TARGET COMPONENT ---------------
    kImporterComponentType,             // Type
    kCodecFormat,                       // SubType
    'vide',                             // Manufaturer
    kImporterFlags, 0,                  // Component Flags and Mask
    'thnr', kImporterResID, 0
};

//   .axv - Profile for video in Annodex
resource 'thga' (kImporterResID + 8, OggImporterName, purgeable) {
    kImporterComponentType,             // Type
    'AXV ',                             // Subtype
    'vide',                             // Manufacturer
    kImporterFlags | movieImportSubTypeIsFileExtension,
    0,                                  // Component Flags Mask
    0, 0,                               // Code Type, Code ID
    'STR ',                             // Name Type
    kImporterNameStringResID,           // Name ID
    'STR ',                             // Info Type
    kImporterInfoStringResID,           // Info ID
    0, 0,                               // Icon Type, Icon ID
                // TARGET COMPONENT ---------------
    kImporterComponentType,             // Type
    kCodecFormat,                       // SubType
    'vide',                             // Manufaturer
    kImporterFlags, 0,                  // Component Flags and Mask
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
    "Ogg " "0.1.12" " (See http://xiph.org)."
};


/*
    This is an example of how to build an atom container resource to hold mime types.
    This component's GetMIMETypeList implementation simply loads this resource and returns it.
    Please note that atoms of the same type MUST be grouped together within an atom container.
*/
resource 'mime' (kImporterResID, OggImporterName, purgeable) {
    {
        kMimeInfoMimeTypeTag,       1, "audio/ogg";
        kMimeInfoMimeTypeTag,       2, "audio/ogg";
        kMimeInfoMimeTypeTag,       3, "application/x-annodex";
        kMimeInfoMimeTypeTag,       4, "application/annodex";
        kMimeInfoMimeTypeTag,       5, "application/ogg";
        kMimeInfoMimeTypeTag,       6, "audio/ogg";
        kMimeInfoMimeTypeTag,       7, "video/ogg";
        kMimeInfoMimeTypeTag,       8, "audio/x-annodex";
        kMimeInfoMimeTypeTag,       9, "audio/annodex";
        kMimeInfoMimeTypeTag,      10, "video/x-annodex";
        kMimeInfoMimeTypeTag,      11, "video/annodex";

        kMimeInfoFileExtensionTag,  1, "ogg";
        kMimeInfoFileExtensionTag,  2, "spx";
        kMimeInfoFileExtensionTag,  3, "anx";
        kMimeInfoFileExtensionTag,  4, "anx";
        kMimeInfoFileExtensionTag,  5, "ogx";
        kMimeInfoFileExtensionTag,  6, "oga";
        kMimeInfoFileExtensionTag,  7, "ogv";
        kMimeInfoFileExtensionTag,  8, "axa";
        kMimeInfoFileExtensionTag,  9, "axa";
        kMimeInfoFileExtensionTag, 10, "axv";
        kMimeInfoFileExtensionTag, 11, "axv";

        kMimeInfoDescriptionTag,    1, "Ogg Vorbis I";
        kMimeInfoDescriptionTag,    2, "Ogg Speex";
        kMimeInfoDescriptionTag,    3, "Annodex Media";
        kMimeInfoDescriptionTag,    4, "Annodex Media";
        kMimeInfoDescriptionTag,    5, "Ogg Multimedia Bitstream";
        kMimeInfoDescriptionTag,    6, "Ogg Audio";
        kMimeInfoDescriptionTag,    7, "Ogg Video";
        kMimeInfoDescriptionTag,    8, "Annodex Audio";
        kMimeInfoDescriptionTag,    9, "Annodex Audio";
        kMimeInfoDescriptionTag,   10, "Annodex Video";
        kMimeInfoDescriptionTag,   11, "Annodex Video";
    };
};

// Not using proper new file types (OggX, OggA, OggV) [RFC5334] below until better
// times (like, when iTunes supports anything beside OggS)
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
        'OGX ',
        kQTMediaInfoNetGroup,

        /* no synonyms */
        {
        },

        {
            "Ogg Multimedia Bitstream",
            "ogx",
            "QuickTime Player",
            "Ogg file importer",
            "",
        },

        /* mime types array */
        {
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
        'OGG ',
        kQTMediaInfoNetGroup,

        /* no synonyms */
        {
        },

        {
            "Ogg Vorbis I audio",
            "ogg",
            "QuickTime Player",
            "Ogg Vorbis I file importer",
            "",
        },

        /* mime types array */
        {
            "audio/ogg";
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
        'OGV ',
        kQTMediaInfoNetGroup,

        /* no synonyms */
        {
        },

        {
            "Ogg Video",
            "ogv",
            "QuickTime Player",
            "Ogg Video file importer",
            "",
        },

        /* mime types array */
        {
            "video/ogg";
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
        'OGA ',
        kQTMediaInfoNetGroup,

        /* no synonyms */
        {
        },

        {
            "Ogg Audio",
            "oga",
            "QuickTime Player",
            "Ogg Audio file importer",
            "",
        },

        /* mime types array */
        {
            "audio/ogg";
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
            "audio/ogg";
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
            "application/annodex";
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
        'AXA ',
        kQTMediaInfoNetGroup,

        /* no synonyms */
        {
        },

        {
            "Annodex Audio",
            "axa",
            "QuickTime Player",
            "Annodex Audio file importer",
            "",
        },

        /* mime types array */
        {
            "audio/x-annodex";
            "audio/annodex";
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
        'AXV ',
        kQTMediaInfoNetGroup,

        /* no synonyms */
        {
        },

        {
            "Annodex Video",
            "axv",
            "QuickTime Player",
            "Annodex Video file importer",
            "",
        },

        /* mime types array */
        {
            "video/x-annodex";
            "video/annodex";
        };
    };
};

#if TARGET_OS_WIN32
read 'MDCf' (kImporterResID) "../MetaDataConfig.plist";
#endif
