/*
 *  CAVorbisEncoderPublic.r
 *
 *    Information bit definitions for the 'thng' resource.
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
 *  Last modified: $Id: CAVorbisEncoderPublic.r 12093 2006-11-12 14:44:51Z arek $
 *
 */


#include "vorbis_versions.h"
#include "fccs.h"



#define RES_ID			-17148
#define COMP_TYPE		'aenc'
#define COMP_SUBTYPE	kAudioFormatXiphOpus
#define COMP_MANUF		'Xiph'
#define VERSION			kCAVorbis_aenc_Version
#define NAME			"Xiph Opus Encoder"
#define DESCRIPTION		"An AudioCodec that encodes linear PCM data into Xiph Opus"
#define ENTRY_POINT		"CAOpusEncoderEntry"

#define kPrimaryResourceID               -17148
#define kComponentType                   'aenc'
#define kComponentSubtype                kAudioFormatXiphOpus
#define kComponentManufacturer           'Xiph'
#define kComponentFlags                  0
#define kComponentVersion                kCAVorbis_aenc_Version
#define kComponentName                   "Xiph Opus"
#define kComponentInfo                   "An AudioCodec that encodes linear PCM data into Xiph Opus"
#define kComponentEntryPoint             "CAOpusEncoderEntry"
#define kComponentPublicResourceMapType	 0
#define kComponentIsThreadSafe           1

#include "XCAResources.r"
