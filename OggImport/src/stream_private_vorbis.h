/*
 *  stream_types_vorbis.h
 *
 *    Definition of private Vorbis specific data structures.
 *
 *
 *  Copyright (c) 2009  Arek Korbik
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


#if !defined(__stream_private_vorbis_h__)
#define __stream_private_vorbis_h__


/* The following two structures were almost literally copied from
   codec_internal.h in libvorbis, at version 1.2.3, and need to be in
   sync with the libvorbis linked against - otherwise things will
   break horribly.

   (Yes, I wish there was a cleaner way of getting the needed info...)
*/

typedef struct {
    int blockflag;
    int windowtype;
    int transformtype;
    int mapping;
} _vorbis_info_mode;

typedef struct _codec_setup_info {

    /* Vorbis supports only short and long blocks, but allows the
       encoder to choose the sizes */

    long blocksizes[2];

    /* modes are the primary means of supporting on-the-fly different
       blocksizes, different channel mappings (LR or M/A),
       different residue backends, etc.  Each mode consists of a
       blocksize flag and a mapping (along with the mapping setup */

    int        modes;
    int        maps;
    int        floors;
    int        residues;
    int        books;
    int        psys;     /* encode only */

    _vorbis_info_mode       *mode_param[64];
    int                     map_type[64];

#if 0  /* we don't need the rest, so... */
    vorbis_info_mapping    *map_param[64];
    int                     floor_type[64];
    vorbis_info_floor      *floor_param[64];
    int                     residue_type[64];
    vorbis_info_residue    *residue_param[64];
    static_codebook        *book_param[256];
    codebook               *fullbooks;

    vorbis_info_psy        *psy_param[4]; /* encode only */
    vorbis_info_psy_global psy_g_param;

    bitrate_manager_info   bi;
    highlevel_encode_setup hi; /* used only by vorbisenc.c.  It's a
                                  highly redundant structure, but
                                  improves clarity of program flow. */
    int         halfrate_flag; /* painless downsample for decode */
#endif /* 0 */
} _codec_setup_info;


typedef struct {
    int num_modes;
    int mask_modes;
    long mode_sizes[64];
} StreamPrivate__vorbis;


#endif /* __stream_private_vorbis_h__ */
