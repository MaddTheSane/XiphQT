/*
 *  CAVorbisDecoderPublic.r
 *
 *    Information bit definitions for the 'thng' resource.
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


#include "vorbis_versions.h"
#include "fccs.h"



#define RES_ID			-17110
#define COMP_TYPE		'adec'
#define COMP_SUBTYPE	kAudioFormatXiphOggFramedVorbis
#define COMP_MANUF		'Xiph'
#define VERSION			kCAVorbis_adec_Version
#define NAME			"Xiph Vorbis (Ogg-framed) Decoder"
#define DESCRIPTION		"An AudioCodec that decodes Xiph Vorbis (Ogg-framed) into linear PCM data"
#define ENTRY_POINT		"CAOggVorbisDecoderEntry"

#include "AUResources.r"



#define RES_ID			-17114
#define COMP_TYPE		'adec'
#define COMP_SUBTYPE	kAudioFormatXiphVorbis
#define COMP_MANUF		'Xiph'
#define VERSION			kCAVorbis_adec_Version
#define NAME			"Xiph Vorbis Decoder"
#define DESCRIPTION		"An AudioCodec that decodes Xiph Vorbis into linear PCM data"
#define ENTRY_POINT		"CAVorbisDecoderEntry"

#include "AUResources.r"
