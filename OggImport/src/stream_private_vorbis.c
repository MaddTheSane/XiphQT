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


#include "stream_vorbis.h"
#include "debug.h"


int
vorbis_private_extract_info(StreamInfo *si)
{
    StreamPrivate__vorbis *info = &(si->si_vorbis.private);
    _codec_setup_info *ci = (_codec_setup_info *) si->si_vorbis.vi.codec_setup;
    int i, size;

    long short_size = ci->blocksizes[0];
    long long_size = ci->blocksizes[1];

    size = info->num_modes = ci->modes;

    i = -1;
    while ((1 << (++i)) < size);
    info->mask_modes = (1 << i) - 1;

    for (i = 0; i < size; i++) {
	info->mode_sizes[i] = (ci->mode_param[i]->blockflag ?
			       long_size : short_size);
    }

    return 0;
}

static long
_vorbis_private_packet_duration(StreamInfo *si, ogg_packet *current,
                               ogg_packet *previous)
{
    StreamPrivate__vorbis *info = &(si->si_vorbis.private);
    int mode = (current->packet[0] >> 1) & info->mask_modes;
    int current_size = info->mode_sizes[mode];

    mode = (previous->packet[0] >> 1) & info->mask_modes;

    return (info->mode_sizes[mode] + current_size) / 4;
}

long
vorbis_private_page_duration(StreamInfo *si /*, ogg_page *og */)
{
    long duration = 0;
    int ovret = -1;
    ogg_packet p1, p2;
    ogg_packet *previous = NULL;
    ogg_packet *current = &p1;
    ogg_packet *other = &p2;

    while (ovret < 0) {
        ovret = ogg_stream_packetout(&si->os, previous);
    }

    if (ovret == 0)
        return -1;

    while (1) {
        ovret = ogg_stream_packetout(&si->os, current);
        if (ovret < 0)
            continue;
        else if (ovret < 1)
            break;
        if (previous != NULL) {
            duration += _vorbis_private_packet_duration(si, current, previous);
        }

        previous = current;
        current = other;
        other = (other == &p1 ? &p2 : &p1);
    };

    return duration;
}
