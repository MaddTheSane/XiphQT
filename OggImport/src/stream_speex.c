/*
 *  stream_speex.c
 *
 *    Speex format related part of OggImporter.
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


#include <Ogg/ogg.h>

#include "stream_speex.h"

#include "debug.h"
#define logg_page_last_packet_incomplete(op) (((unsigned char *)(op)->header)[26 + ((unsigned char *)(op)->header)[26]] == 255)

#include "OggImport.h"

#include "fccs.h"
#include "data_types.h"
#include "utils.h"

int recognize_header__speex(ogg_page *op)
{
    if (!strncmp("Speex   ", (char *)op->body, 8))
        return 0;

    return 1;
};

int verify_header__speex(ogg_page *op) //?
{
    return 0;
};

int initialize_stream__speex(StreamInfo *si)
{
    memset(&si->si_speex.header, 0, sizeof(SpeexHeader));
    vorbis_comment_init(&si->si_speex.vc);

    si->si_speex.skipped_headers = 0;
    si->si_speex.state = kSStateInitial;

    return 0;
};

void clear_stream__speex(StreamInfo *si)
{
    vorbis_comment_clear(&si->si_speex.vc);
};

ComponentResult create_sample_description__speex(StreamInfo *si)
{
    ComponentResult err = noErr;
    Handle desc;
    AudioStreamBasicDescription asbd;
    AudioChannelLayout acl;
    AudioChannelLayout *pacl = &acl;
    ByteCount acl_size = sizeof(acl);

    asbd.mSampleRate = si->rate;
    asbd.mFormatID = kAudioFormatXiphOggFramedSpeex;
    asbd.mFormatFlags = 0;
    asbd.mBytesPerPacket = 0;
    asbd.mFramesPerPacket = 0;
    //asbd.mBytesPerFrame = 2 * si->numChannels;
    asbd.mBytesPerFrame = 0;
    asbd.mChannelsPerFrame = si->numChannels;
    //asbd.mBitsPerChannel = 16;
    asbd.mBitsPerChannel = 0;
    asbd.mReserved = 0;

    if (si->numChannels == 1)
        acl.mChannelLayoutTag = kAudioChannelLayoutTag_Mono;
    else if (si->numChannels == 2)
        acl.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;
    else {
        pacl = NULL;
        acl_size = 0;
    }
    acl.mChannelBitmap = 0;
    acl.mNumberChannelDescriptions = 0;

    err = QTSoundDescriptionCreate(&asbd, pacl, acl_size, NULL, 0, kQTSoundDescriptionKind_Movie_Version2, (SoundDescriptionHandle*) &desc);

    if (err == noErr) {
        si->sampleDesc = (SampleDescriptionHandle) desc;
    }

    return err;
};

int process_first_packet__speex(StreamInfo *si, ogg_page *op, ogg_packet *opckt)
{
    unsigned long serialnoatom[3] = { EndianU32_NtoB(sizeof(serialnoatom)), EndianU32_NtoB(kCookieTypeOggSerialNo),
                                      EndianS32_NtoB(ogg_page_serialno(op)) };
    unsigned long atomhead[2] = { EndianU32_NtoB(opckt->bytes + sizeof(atomhead)), EndianU32_NtoB(kCookieTypeSpeexHeader) };
    SpeexHeader *inheader = (SpeexHeader *) opckt->packet;

    si->si_speex.header.bitrate =                 EndianS32_LtoN(inheader->bitrate);
    si->si_speex.header.extra_headers =           EndianS32_LtoN(inheader->extra_headers);
    si->si_speex.header.frame_size =              EndianS32_LtoN(inheader->frame_size);
    si->si_speex.header.frames_per_packet =       EndianS32_LtoN(inheader->frames_per_packet);
    si->si_speex.header.header_size =             EndianS32_LtoN(inheader->header_size);
    si->si_speex.header.mode =                    EndianS32_LtoN(inheader->mode);
    si->si_speex.header.mode_bitstream_version =  EndianS32_LtoN(inheader->mode_bitstream_version);
    si->si_speex.header.nb_channels =             EndianS32_LtoN(inheader->nb_channels);
    si->si_speex.header.rate =                    EndianS32_LtoN(inheader->rate);
    si->si_speex.header.reserved1 =               EndianS32_LtoN(inheader->reserved1);
    si->si_speex.header.reserved2 =               EndianS32_LtoN(inheader->reserved2);
    si->si_speex.header.speex_version_id =        EndianS32_LtoN(inheader->speex_version_id);
    si->si_speex.header.vbr =                     EndianS32_LtoN(inheader->vbr);
    //si->si_speex.header. = EndianS32_LtoN(inheader->);

    dbg_printf("! -- - speex_first_packet: ch: %d, rate: %ld\n", si->si_speex.header.nb_channels, si->si_speex.header.rate);
    si->numChannels = si->si_speex.header.nb_channels;
    si->rate = si->si_speex.header.rate;
    //si->lastMediaInserted = 0;
    si->mediaLength = 0;

    PtrAndHand(serialnoatom, si->soundDescExtension, sizeof(serialnoatom)); //check errors?
    PtrAndHand(atomhead, si->soundDescExtension, sizeof(atomhead)); //check errors?
    PtrAndHand(opckt->packet, si->soundDescExtension, opckt->bytes); //check errors?

    si->si_speex.state = kSStateReadingComments;

    return 0;
};

ComponentResult process_stream_page__speex(OggImportGlobals *globals, StreamInfo *si, ogg_page *opg)
{
    ComponentResult ret = noErr;
    int ovret = 0;
    Boolean loop = true;
    Boolean movie_changed = false;

    TimeValue movieTS = GetMovieTimeScale(globals->theMovie);
    TimeValue mediaTS = 0;
    TimeValue mediaTS_fl = 0.0;

    ogg_packet op;

    switch(si->si_speex.state) {
    case kSStateReadingComments:
    case kSStateReadingAdditionalHeaders:
        ogg_stream_pagein(&si->os, opg);
        break;
    default:
        break;
    }

    do {
        switch(si->si_speex.state) {
        case kSStateReadingComments:
            ovret = ogg_stream_packetout(&si->os, &op);
            if (ovret < 0) {
                loop = false;
                ret = invalidMedia;
            } else if (ovret < 1) {
                loop = false;
            } else {
                unsigned long atomhead[2] = { EndianU32_NtoB(op.bytes + sizeof(atomhead)), EndianU32_NtoB(kCookieTypeSpeexComments) };

                PtrAndHand(atomhead, si->soundDescExtension, sizeof(atomhead));
                PtrAndHand(op.packet, si->soundDescExtension, op.bytes);

                ret = CreateTrackAndMedia(globals, si, opg);
                if (ret != noErr) {
                    dbg_printf("??? -- CreateTrackAndMedia failed?: %ld\n", (long)ret);
                }

                unpack_vorbis_comments(&si->si_speex.vc, op.packet, op.bytes);
                /*err =*/ DecodeCommentsQT(globals, si, &si->si_speex.vc);
                //NotifyMovieChanged(globals);

                si->si_speex.state = kSStateReadingAdditionalHeaders;
            }

            break;

        case kSStateReadingAdditionalHeaders:
            if (si->si_speex.skipped_headers >= si->si_speex.header.extra_headers) {
                unsigned long endAtom[2] = { EndianU32_NtoB(sizeof(endAtom)), EndianU32_NtoB(kAudioTerminatorAtomType) };

                ret = PtrAndHand(endAtom, si->soundDescExtension, sizeof(endAtom));
                if (ret == noErr) {
                    ret = AddSoundDescriptionExtension((SoundDescriptionHandle) si->sampleDesc,
                                                       si->soundDescExtension, siDecompressionParams);
                    //dbg_printf("??? -- Adding extension: %ld\n", ret);
                } else {
                    //dbg_printf("??? -- Hmm, something went wrong: %ld\n", ret);
                }

                si->insertTime = 0;
                si->streamOffset = globals->currentGroupOffset;
                mediaTS = GetMediaTimeScale(si->theMedia);
                mediaTS_fl = (Float64) mediaTS;
                si->streamOffsetSamples = (TimeValue) (mediaTS_fl * globals->currentGroupOffsetSubSecond) -
                    ((globals->currentGroupOffset % movieTS) * mediaTS / movieTS);
                dbg_printf("---/  / streamOffset: [%ld, %ld], %lg\n", si->streamOffset, si->streamOffsetSamples, globals->currentGroupOffsetSubSecond);
                si->incompleteCompensation = 0;
                si->si_speex.state = kSStateReadingFirstPacket;

                loop = false; // ??!
                break;
            }

            ovret = ogg_stream_packetout(&si->os, &op);
            if (ovret < 0) {
                loop = false;
                ret = invalidMedia;
            } else if (ovret < 1) {
                loop = false;
            } else {
                // not much here so far, basically just skip the extra header packet
                unsigned long atomhead[2] = { EndianU32_NtoB(op.bytes + sizeof(atomhead)), EndianU32_NtoB(kCookieTypeSpeexExtraHeader) };
                PtrAndHand(atomhead, si->soundDescExtension, sizeof(atomhead));
                PtrAndHand(op.packet, si->soundDescExtension, op.bytes);

                si->si_speex.skipped_headers += 1;
            }

            break;

        case kSStateReadingFirstPacket:
            if (ogg_page_pageno(opg) > 2) {
                si->lastGranulePos = ogg_page_granulepos(opg);
                dbg_printf("----==< skipping: %llx, %lx\n", si->lastGranulePos, ogg_page_pageno(opg));
                loop = false;

                if (si->lastGranulePos < 0)
                    si->lastGranulePos = 0;
            }
            si->si_speex.state = kSStateReadingPackets;
            break;

        case kVStateReadingPackets:
            {
                ogg_int64_t pos       = ogg_page_granulepos(opg);
                int         len       = opg->header_len + opg->body_len;
                TimeValue   duration  = pos - si->lastGranulePos;
                TimeValue   inserted  = 0;
                short       smp_flags = 0;

                if (ogg_page_continued(opg) || si->incompleteCompensation != 0)
                    smp_flags |= mediaSampleNotSync;

                if (duration <= 0) {
                    duration = INCOMPLETE_PAGE_DURATION;
                    si->incompleteCompensation -= INCOMPLETE_PAGE_DURATION;
                } else if (si->incompleteCompensation != 0) {
                    duration += si->incompleteCompensation;
                    si->incompleteCompensation = 0;
                    if (duration <= 0) {
                        ret = badFileFormat;
                        loop = false;
                        break;
                    }
                }

                if (si->insertTime == 0 && si->streamOffsetSamples > 0) {
                    dbg_printf("   -   :++: increasing duration (%ld) by sampleOffset: %ld\n", duration, si->streamOffsetSamples);
                    duration += si->streamOffsetSamples;
                }

                dbg_printf("   -   :++: adding sampleRef: %lld, len: %d, dur: %d\n", globals->dataOffset, len, duration);
                ret = AddMediaSampleReference(si->theMedia, S32Set(globals->dataOffset),
                                              len, duration, si->sampleDesc, 1, smp_flags, &inserted); //@@@@ 64-bit enable
                if (ret == noErr) {
                    TimeValue timeLoaded;
                    Float64 timeLoadedSubSecond;

                    si->mediaLength += duration;

                    dbg_printf("   -   :><: added page %04ld at %14ld (size: %5ld, tsize: %6d), f: %d\n",
                               ogg_page_pageno(opg), inserted,
                               opg->header_len + opg->body_len, len, !logg_page_last_packet_incomplete(opg));
                    dbg_printf("   -   :/>: inserting media: %ld, mt: %lld, dur: %d\n", si->insertTime, si->lastGranulePos, duration);
                    ret = InsertMediaIntoTrack(si->theTrack, si->insertTime /*inserted*/, /* si->lastGranulePos */ inserted,
                                               duration, fixed1);
                    if (si->insertTime == 0) {
                        if (si->streamOffset != 0) {
                            SetTrackOffset(si->theTrack, si->streamOffset);
                            dbg_printf("   # -- SetTrackOffset(%ld) = %ld --> %ld\n",
                                       si->streamOffset, GetMoviesError(),
                                       GetTrackOffset(si->theTrack));
                            if (globals->dataIsStream) {
                                SetTrackEnabled(si->theTrack, false);
                                SetTrackEnabled(si->theTrack, true);
                            }
                        }
                    }
                    si->insertTime = -1;

                    mediaTS = GetMediaTimeScale(si->theMedia);
                    mediaTS_fl = (Float64) mediaTS;
                    timeLoaded = si->streamOffset + si->mediaLength / mediaTS * movieTS + (si->mediaLength % mediaTS) * movieTS / mediaTS;
                    timeLoadedSubSecond = (Float64) ((si->streamOffset % movieTS * mediaTS / movieTS + si->mediaLength) % mediaTS) / mediaTS_fl;

                    dbg_printf("   -   :><: added page %04ld at %14ld; offset: %ld, duration: %ld (%ld(%lg); %ld; ml: %ld), mediats: %ld; moviets: %ld, ret = %ld\n",
                               ogg_page_pageno(opg), inserted,
                               GetTrackOffset(si->theTrack), GetTrackDuration(si->theTrack), timeLoaded, timeLoadedSubSecond,
                               (duration * movieTS) / mediaTS, si->mediaLength,
                               mediaTS, movieTS, ret);
                    if (globals->timeLoaded < timeLoaded || (globals->timeLoaded == timeLoaded && globals->timeLoadedSubSecond < timeLoadedSubSecond)) {
                        globals->timeLoaded = timeLoaded;
                        globals->timeLoadedSubSecond = timeLoadedSubSecond;
                    }

                    movie_changed = true;
                }

                if (pos != -1)
                    si->lastGranulePos = pos;
            }
            loop = false;
            break;

        default:
            loop = false;
        }
    } while(loop);

    if (movie_changed)
        NotifyMovieChanged(globals);

    return ret;
};
