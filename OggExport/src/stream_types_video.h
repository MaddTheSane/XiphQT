/*
 *  stream_types_video.h
 *
 *    Definition of video stream data structures for OggExport.
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


#if !defined(__stream_types_video_h__)
#define __stream_types_video_h__

enum {
    kOES_V_init_op_size = 1024,
};

typedef struct {
    ComponentInstance stdVideo;

    ICMCompressionSessionRef cs;
    ICMCompressionSessionOptionsRef cs_opts;

    ICMDecompressionSessionRef ds;
    ICMDecompressionSessionOptionsRef ds_opts;

    ImageDescriptionHandle cs_imdsc;

    float frames_time;

    Fixed width;
    Fixed height;
    Fixed fps;
    SInt16 depth;

    ogg_packet op;
    UInt32 op_duration;
    void * op_buffer;
    UInt32 op_buffer_size;
    MediaSampleFlags op_flags;

    UInt32 max_packet_size;

    UInt32 grpos_shift;
} StreamInfo__video;

#define _HAVE__OE_VIDEO 1
#endif /* __stream_types_video_h__ */
