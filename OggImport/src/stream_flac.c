/*
 *  stream_flac.c
 *
 *    FLAC format related part of OggImporter.
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


#include <Ogg/ogg.h>

#include "stream_flac.h"

#include "debug.h"
#define logg_page_last_packet_incomplete(op) (((unsigned char *)(op)->header)[26 + ((unsigned char *)(op)->header)[26]] == 255)

#include "OggImport.h"

#include "fccs.h"
#include "data_types.h"
#include "utils.h"

#undef INCOMPLETE_PAGE_DURATION
#define INCOMPLETE_PAGE_DURATION 10

int recognize_header__flac(ogg_page *op)
{
    char fh[] = {0x7f, 'F', 'L', 'A', 'C', '\0'};
    dbg_printf("! -- - flac_recognise_header: '%4.4s'\n", ((char *)op->body) + 1);
    if (!strncmp(fh, (char *)op->body, 5))
        return 0;

    return 1;
};

int verify_header__flac(ogg_page *op)
{
    char nfh[] = "fLaC";
    dbg_printf("! -- - flac_verify_header: mapping ver: %d.%d\n", (int)((char *)op->body)[5], (int)((char *)op->body)[6]);
    if ((int)((char *)op->body)[5] != FLAC_MAPPING_SUPPORTED_MAJOR)
        return -1;
    if (strncmp(nfh, ((char *)op->body) + 9, 4))
        return -1;

    // anything else to check?
    return 0;
};

int initialize_stream__flac(StreamInfo *si)
{
    vorbis_comment_init(&si->si_flac.vc);

    si->si_flac.metablocks = 0;
    si->si_flac.skipped = 0;
    si->si_flac.state = kFStateInitial;

    si->si_flac.sample_refs_count = 0;
    si->si_flac.sample_refs_duration = 0;
    //si->si_flac.sample_refs_size = kSRefsInitial;
    //si->si_flac.sample_refs_size = 64;
    si->si_flac.sample_refs_size = 1;
    si->si_flac.sample_refs = calloc(si->si_flac.sample_refs_size, sizeof(SampleReference64Record));

    if (si->si_flac.sample_refs == NULL)
        return -1;

    return 0;
};

void clear_stream__flac(StreamInfo *si)
{
    if (si->si_flac.sample_refs != NULL)
        free(si->si_flac.sample_refs);

    vorbis_comment_clear(&si->si_flac.vc);
};

ComponentResult create_sample_description__flac(StreamInfo *si)
{
    ComponentResult err = noErr;
    Handle desc;
    AudioStreamBasicDescription asbd;
    AudioChannelLayout acl;
    AudioChannelLayout *pacl = &acl;
    ByteCount acl_size = sizeof(acl);

    asbd.mSampleRate = si->rate;
    asbd.mFormatID = kAudioFormatXiphOggFramedFLAC;
    asbd.mFormatFlags = 0;
    asbd.mBytesPerPacket = 0;
    asbd.mFramesPerPacket = 0;
    //asbd.mBytesPerFrame = 2 * si->numChannels;
    asbd.mBytesPerFrame = 0;
    asbd.mChannelsPerFrame = si->numChannels;
    //asbd.mBitsPerChannel = 16;
    //asbd.mBitsPerChannel = 0;
    asbd.mBitsPerChannel = si->si_flac.bps;
    asbd.mReserved = 0;

    acl.mChannelBitmap = 0;
    acl.mNumberChannelDescriptions = 0;
    switch (si->numChannels) {
    case 1:
        acl.mChannelLayoutTag = kAudioChannelLayoutTag_Mono;
        break;
    case 2:
        acl.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;
        break;
    case 3:
        acl.mChannelLayoutTag = kAudioChannelLayoutTag_ITU_3_0;
        break;
    case 4:
        acl.mChannelLayoutTag = kAudioChannelLayoutTag_Quadraphonic;
        break;
    case 5:
        acl.mChannelLayoutTag = kAudioChannelLayoutTag_ITU_3_2;
        break;
    case 6:
        acl.mChannelLayoutTag = kAudioChannelLayoutTag_ITU_3_2_1;
        break;
    default:
        pacl = NULL;
        acl_size = 0;
    }

    err = QTSoundDescriptionCreate(&asbd, pacl, acl_size, NULL, 0, kQTSoundDescriptionKind_Movie_Version2, (SoundDescriptionHandle*) &desc);

    if (err == noErr) {
        si->sampleDesc = (SampleDescriptionHandle) desc;
    }

    return err;
};

int process_first_packet__flac(StreamInfo *si, ogg_page *op, ogg_packet *opckt)
{
    unsigned long serialnoatom[3] = { EndianU32_NtoB(sizeof(serialnoatom)), EndianU32_NtoB(kCookieTypeOggSerialNo),
                                      EndianS32_NtoB(ogg_page_serialno(op)) };
    unsigned long atomhead[2] = { EndianU32_NtoB(opckt->bytes + sizeof(atomhead) - 13), EndianU32_NtoB(kCookieTypeFLACStreaminfo) };

    UInt32 sib = EndianU32_BtoN(* (UInt32 *) (((char *)opckt->packet) + 27));
    si->si_flac.metablocks =  (SInt32) EndianU16_BtoN(* (UInt16 *) (((char *)opckt->packet) + 7));

    sib >>= 4;
    si->si_flac.bps = (sib & 0x1f) + 1;
    sib >>= 5;
    si->numChannels = (sib & 0x07) + 1;
    si->rate = (sib >> 3) & 0xfffff;

    //si->lastMediaInserted = 0;
    si->mediaLength = 0;

    dbg_printf("! -- - flac_first_packet: ch: %d, rate: %ld, bps: %ld\n", si->numChannels, si->rate, si->si_flac.bps);

    PtrAndHand(serialnoatom, si->soundDescExtension, sizeof(serialnoatom)); //check errors?
    PtrAndHand(atomhead, si->soundDescExtension, sizeof(atomhead)); //check errors?
    PtrAndHand((((char *)opckt->packet) + 13), si->soundDescExtension, opckt->bytes - 13); //check errors?

    si->si_flac.state = kFStateReadingComments;

    return 0;
};

ComponentResult process_stream_page__flac(OggImportGlobals *globals, StreamInfo *si, ogg_page *opg)
{
    ComponentResult ret = noErr;
    int ovret = 0;
    Boolean loop = true;
    Boolean movie_changed = false;

    TimeValue movieTS = GetMovieTimeScale(globals->theMovie);
    TimeValue mediaTS = 0;
    TimeValue mediaTS_fl = 0.0;

    ogg_packet op;

    switch(si->si_flac.state) {
    case kFStateReadingComments:
    case kFStateReadingAdditionalMDBlocks:
        ogg_stream_pagein(&si->os, opg);
        break;
    default:
        break;
    }

    do {
        switch(si->si_flac.state) {
        case kFStateReadingComments:
            ovret = ogg_stream_packetout(&si->os, &op);
            if (ovret < 0) {
                loop = false;
                ret = invalidMedia;
            } else if (ovret < 1) {
                loop = false;
            } else {
                ret = CreateTrackAndMedia(globals, si, opg);
                if (ret != noErr) {
                    dbg_printf("??? -- CreateTrackAndMedia failed?: %ld\n", (long)ret);
                    loop = false;
                    break;
                }

                if (si->si_flac.metablocks == 0 && (*((unsigned char*) op.packet) == 0xff)) {
                    si->si_flac.metablocks = si->si_flac.skipped;
                    si->si_flac.state = kFStateReadingAdditionalMDBlocks;
                    break;
                }

                {
                    unsigned long atomhead[2] = { EndianU32_NtoB(op.bytes + sizeof(atomhead)), EndianU32_NtoB(kCookieTypeFLACMetadata) };

                    PtrAndHand(atomhead, si->soundDescExtension, sizeof(atomhead));
                    PtrAndHand(op.packet, si->soundDescExtension, op.bytes);
                }

                if (((* (char *) op.packet) & 0x7f) == 4) {
                    dbg_printf("!  > - flac_stream_page - mb: %ld, skipped: %ld, h: %02x\n", si->si_flac.metablocks, si->si_flac.skipped,
                               (*(char *) op.packet) & 0x7f);
                    unpack_vorbis_comments(&si->si_flac.vc, ((char *) op.packet) + 4, op.bytes - 4);
                    /*err =*/ DecodeCommentsQT(globals, si, &si->si_flac.vc);
                    //NotifyMovieChanged(globals);
                }

                si->si_flac.skipped += 1;
                si->si_flac.state = kFStateReadingAdditionalMDBlocks;
            }

            break;

        case kFStateReadingAdditionalMDBlocks:
            dbg_printf("! -- - flac_stream_page - mb: %ld, skipped: %ld\n", si->si_flac.metablocks, si->si_flac.skipped);
            if (si->si_flac.metablocks > 0 && si->si_flac.skipped >= si->si_flac.metablocks) {
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
                si->si_flac.state = kFStateReadingFirstPacket;

                loop = false; // the audio data is supposed to start on a fresh page
                break;
            }

            ovret = ogg_stream_packetout(&si->os, &op);
            dbg_printf("! -- - flac_stream_page - ovret: %d\n", ovret);
            if (ovret < 0) {
                loop = false;
                ret = invalidMedia;
            } else if (ovret < 1) {
                loop = false;
            } else {
                // not much here so far, basically just skip the extra header packet
                unsigned long atomhead[2] = { EndianU32_NtoB(op.bytes + sizeof(atomhead)), EndianU32_NtoB(kCookieTypeFLACMetadata) };

                if (si->si_flac.metablocks == 0 && (* (unsigned char*) op.packet) == 0xff) {
                    si->si_flac.metablocks = si->si_flac.skipped;
                    break;
                }

                PtrAndHand(atomhead, si->soundDescExtension, sizeof(atomhead));
                PtrAndHand(op.packet, si->soundDescExtension, op.bytes);

                if (((* (unsigned char *) op.packet) & 0x7f) == 4) {
                    dbg_printf("!  > - flac_stream_page - mb: %ld, skipped: %ld, h: %02x\n", si->si_flac.metablocks, si->si_flac.skipped,
                               (*(char *) op.packet) & 0x7f);
                    unpack_vorbis_comments(&si->si_flac.vc, ((char *) op.packet) + 4, op.bytes - 4);
                    /*err =*/ DecodeCommentsQT(globals, si, &si->si_flac.vc);
                    //NotifyMovieChanged(globals);
                }

                si->si_flac.skipped += 1;
            }

            break;

        case kFStateReadingFirstPacket:
            // what to do with this one? is it needed at all??
            if (ogg_page_pageno(opg) > 2 && false) {
                si->lastGranulePos = ogg_page_granulepos(opg);
                dbg_printf("----==< skipping: %llx, %lx\n", si->lastGranulePos, ogg_page_pageno(opg));
                loop = false;

                if (si->lastGranulePos < 0)
                    si->lastGranulePos = 0;
            }
            si->si_flac.state = kFStateReadingPackets;
            break;

        case kFStateReadingPackets:
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

                
                if (si->si_flac.sample_refs_count >= si->si_flac.sample_refs_size) {
                    //resize the sample_refs array
                    SampleReference64Record *srptr = NULL;
                    si->si_flac.sample_refs_size += kSRefsIncrement;
                    srptr = realloc(si->si_flac.sample_refs, si->si_flac.sample_refs_size * sizeof(SampleReference64Record));
                    //if (srptr == NULL)
                    //    ; // signal error here
                    si->si_flac.sample_refs = srptr;
                }

                {
                    SampleReference64Record *srptr = &si->si_flac.sample_refs[si->si_flac.sample_refs_count];
                    memset(srptr, 0, sizeof(SampleReference64Record));
                    srptr->dataOffset = SInt64ToWide(globals->dataOffset);
                    srptr->dataSize = len;
                    srptr->durationPerSample = duration;
                    srptr->numberOfSamples = 1;
                    srptr->sampleFlags = smp_flags;

                    dbg_printf("   -   :++: storing sampleRef: %lld, len: %d, dur: %d [%08lx, %08lx]\n", globals->dataOffset, len, duration,
                               (UInt32) si->si_flac.sample_refs, (UInt32) srptr);
                    si->si_flac.sample_refs_count += 1;
                    si->si_flac.sample_refs_duration += duration;
                }

                // change the condition...?
                //if (si->si_flac.sample_refs_count >= si->si_flac.sample_refs_size || ogg_page_eos(opg)) {
                if (/*si->si_flac.sample_refs_count >= si->si_flac.sample_refs_size ||*/ ogg_page_eos(opg)) {
                    dbg_printf("   -   :++: adding sampleRefs: %lld, count: %ld, dur: %ld\n", globals->dataOffset, si->si_flac.sample_refs_count,
                               si->si_flac.sample_refs_duration);
                    ret = AddMediaSampleReferences64(si->theMedia, si->sampleDesc, si->si_flac.sample_refs_count, si->si_flac.sample_refs, &inserted);

                    if (ret == noErr) {
                        TimeValue timeLoaded;
                        Float64 timeLoadedSubSecond;

                        si->mediaLength += si->si_flac.sample_refs_duration;

                        dbg_printf("   -   :><: added page %04ld at %14ld, f: %d\n",
                                   ogg_page_pageno(opg), inserted, !logg_page_last_packet_incomplete(opg));
                        dbg_printf("   -   :><: d:%ld, dd:%lld, ds:%ld\n", GetMediaDuration(si->theMedia), GetMediaDecodeDuration(si->theMedia),
                                   GetMediaDataSize(si->theMedia, 0, inserted + duration));
                        dbg_printf("   -   :/>: inserting media: %ld, mt: %ld, dur: %d\n", si->insertTime, /* si->lastGranulePos */ inserted,
                                   si->si_flac.sample_refs_duration);
                        ret = InsertMediaIntoTrack(si->theTrack, si->insertTime /*inserted*/, /* si->lastGranulePos */ inserted,
                                                   si->si_flac.sample_refs_duration, fixed1);
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

                        si->si_flac.sample_refs_duration = 0;
                        si->si_flac.sample_refs_count = 0;
                    }
                }
#if 0
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
                    dbg_printf("   -   :><: d:%ld, dd:%lld, ds:%ld\n", GetMediaDuration(si->theMedia), GetMediaDecodeDuration(si->theMedia),
                               GetMediaDataSize(si->theMedia, 0, inserted + duration));
                    dbg_printf("   -   :/>: inserting media: %ld, mt: %ld, dur: %d\n", si->insertTime, /* si->lastGranulePos */ inserted, duration);
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
#endif

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
