/*
 *  stream_theora.c
 *
 *    Theora format related part of OggImporter.
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


#include "stream_theora.h"
#include <Theora/theoradec.h>

#include "debug.h"
#define logg_page_last_packet_incomplete(op) (((unsigned char *)(op)->header)[26 + ((unsigned char *)(op)->header)[26]] == 255)

#include "OggImport.h"

#include "fccs.h"
#include "data_types.h"


/*
 * Euclid's GCD algorithm
 * de-recursified, with input check suitable for Theora fps a/b
 */
UInt32 gcd(UInt32 a, UInt32 b)
{
    UInt32 tmp;

    if (b == 0)
        return 0;

    while (b > 0) {
        // python: a, b = b, a % b
        tmp = a;
        a = b;
        b = tmp % b;
    }

    return a;
}


int recognize_header__theora(ogg_page *op)
{
    dbg_printf("! -- - theora_recognise_header: '%4.4s'\n", ((char *)op->body) + 1);
    if (!strncmp("\x80theora", (char *)op->body, 7))
        return 0;

    return 1;
};

int verify_header__theora(ogg_page *op) //?
{
    OSErr err = noErr;

    ogg_stream_state os;
    ogg_packet       opk;

    th_info        ti;
    th_comment     tc;
    th_setup_info *ts = NULL;

    ogg_stream_init(&os, ogg_page_serialno(op));


    th_info_init(&ti);
    th_comment_init(&tc);

    if (ogg_stream_pagein(&os, op) < 0)
        err = invalidMedia;
    else if (ogg_stream_packetout(&os, &opk) != 1)
        err = invalidMedia;
    else if (th_decode_headerin(&ti, &tc, &ts, &opk) < 0)
        err = noVideoTrackInMovieErr;

    ogg_stream_clear(&os);

    if (ts != NULL)         // theoretically this shouldn't happen, but then
        th_setup_free(ts);  // theoretically I don't know that this shouldn't happen

    th_comment_clear(&tc);
    th_info_clear(&ti);

    return err;
};

int initialize_stream__theora(StreamInfo *si)
{
    th_info_init(&si->si_theora.ti);
    th_comment_init(&si->si_theora.tc);
    si->si_theora.ts = NULL;

    si->si_theora.state = kTStateInitial;

    return 0;
};

void clear_stream__theora(StreamInfo *si)
{
    if (si->si_theora.ts != NULL)
        th_setup_free(si->si_theora.ts);
    th_info_clear(&si->si_theora.ti);
    th_comment_clear(&si->si_theora.tc);
};

ComponentResult create_sample_description__theora(StreamInfo *si)
{
    ComponentResult err = noErr;
    Handle desc = NewHandleClear(sizeof(ImageDescription));
    ImageDescriptionPtr imgdsc = (ImageDescriptionPtr) *desc;

    imgdsc->idSize = sizeof(ImageDescription);
    imgdsc->cType = 'XiTh';
    imgdsc->version = 1; //major ver num
    imgdsc->revisionLevel = 1; //minor ver num
    imgdsc->vendor = 'XiQT';
    imgdsc->temporalQuality = codecMaxQuality;
    imgdsc->spatialQuality = codecMaxQuality;
    imgdsc->width = si->si_theora.ti.frame_width;
    imgdsc->height = si->si_theora.ti.frame_height;
    imgdsc->hRes = 72<<16;
    imgdsc->vRes = 72<<16;
    imgdsc->depth = 24;
    imgdsc->clutID = -1;

    si->sampleDesc = (SampleDescriptionHandle) desc;

    return err;
};

ComponentResult create_track__theora(OggImportGlobals *globals, StreamInfo *si)
{
    ComponentResult ret = noErr;
    dbg_printf("! -T calling => NewMovieTrack()\n");
    UInt32 frame_width = si->si_theora.ti.frame_width;
    UInt32 frame_width_fraction = 0;

    if (si->si_theora.ti.aspect_numerator != si->si_theora.ti.aspect_denominator) {
        frame_width_fraction = (frame_width * si->si_theora.ti.aspect_numerator % si->si_theora.ti.aspect_denominator) * 0x10000 / si->si_theora.ti.aspect_denominator;
        frame_width = frame_width * si->si_theora.ti.aspect_numerator / si->si_theora.ti.aspect_denominator;
    }
    si->theTrack = NewMovieTrack(globals->theMovie,
                                 frame_width << 16 | (frame_width_fraction & 0xffff),
                                 si->si_theora.ti.frame_height << 16, 0);

    return ret;
};

ComponentResult create_track_media__theora(OggImportGlobals *globals, StreamInfo *si, Handle data_ref)
{
    ComponentResult ret = noErr;
    dbg_printf("! -T calling => NewTrackMedia(%lx)\n", si->rate);
    si->theMedia = NewTrackMedia(si->theTrack, VideoMediaType, si->rate, data_ref, globals->dataRefType);

    return ret;
};

#define MAX_FPS_DENOMINATOR 20
#define DESIRED_MULTIPLIER 19

int process_first_packet__theora(StreamInfo *si, ogg_page *op, ogg_packet *opckt)
{
    unsigned long serialnoatom[3] = { EndianU32_NtoB(sizeof(serialnoatom)), EndianU32_NtoB(kCookieTypeOggSerialNo),
                                      EndianS32_NtoB(ogg_page_serialno(op)) };
    unsigned long atomhead[2] = { EndianU32_NtoB(opckt->bytes + sizeof(atomhead)), EndianU32_NtoB(kCookieTypeTheoraHeader) };
    unsigned long fps_gcd = 1, multiplier = 1;
    UInt32 fps_N, fps_D;

    th_decode_headerin(&si->si_theora.ti, &si->si_theora.tc, &si->si_theora.ts, opckt); //check errors?

    si->numChannels = 0;

    fps_N = si->si_theora.ti.fps_numerator;
    fps_D = si->si_theora.ti.fps_denominator;

    fps_gcd = gcd(fps_N, fps_D);
    if (fps_gcd < 1)
        return -1; // return some reasonable error code?

    if (fps_D / fps_gcd > MAX_FPS_DENOMINATOR) {
        UInt64 remainder;
        fps_N = U32SetU(U64Divide(U64Multiply(U64Set(fps_N), U64Set(MAX_FPS_DENOMINATOR)), U64Set(fps_D), &remainder));
        if (U64Compare(remainder, U64Set(MAX_FPS_DENOMINATOR / 2)) > 0)
            fps_N += 1;
        fps_D = MAX_FPS_DENOMINATOR;
        fps_gcd = gcd(fps_N, fps_D);
    }

    multiplier = DESIRED_MULTIPLIER / (fps_D / fps_gcd) + 1;
    si->si_theora.fps_framelen = (fps_D / fps_gcd) * multiplier;
    si->rate = (fps_N / fps_gcd) * multiplier;

    dbg_printf("! -T   setting FPS values: [gcd: %8ld, mult: %8ld] fl: %8ld, rate: %8ld (N: %8ld, D: %8ld) (nN: %8ld, nD: %8ld)\n",
               fps_gcd, multiplier, si->si_theora.fps_framelen, si->rate, si->si_theora.ti.fps_numerator, si->si_theora.ti.fps_denominator,
               fps_N, fps_D);
    si->si_theora.granulepos_shift = si->si_theora.ti.keyframe_granule_shift;

    PtrAndHand(serialnoatom, si->soundDescExtension, sizeof(serialnoatom)); //check errors?
    PtrAndHand(atomhead, si->soundDescExtension, sizeof(atomhead)); //check errors?
    PtrAndHand(opckt->packet, si->soundDescExtension, opckt->bytes); //check errors?

    si->si_theora.state = kTStateReadingComments;

    return 0;
};

ComponentResult process_stream_page__theora(OggImportGlobals *globals, StreamInfo *si, ogg_page *opg)
{
    ComponentResult ret = noErr;
    int ovret = 0;
    Boolean loop = true;
    Boolean movie_changed = false;

    ogg_packet op;

    switch(si->si_theora.state) {
    case kTStateReadingComments:
    case kTStateReadingCodebooks:
    case kTStateReadingPackets:
        ogg_stream_pagein(&si->os, opg);
        break;
    default:
        break;
    }

    do {
        switch(si->si_theora.state) {
        case kTStateReadingComments:
            ovret = ogg_stream_packetout(&si->os, &op);
            if (ovret < 0) {
                loop = false;
                ret = invalidMedia;
            } else if (ovret < 1) {
                loop = false;
            } else {
                unsigned long atomhead[2] = { EndianU32_NtoB(op.bytes + sizeof(atomhead)), EndianU32_NtoB(kCookieTypeTheoraComments) };

                PtrAndHand(atomhead, si->soundDescExtension, sizeof(atomhead));
                PtrAndHand(op.packet, si->soundDescExtension, op.bytes);
                th_decode_headerin(&si->si_theora.ti, &si->si_theora.tc, &si->si_theora.ts, &op);

                ret = CreateTrackAndMedia(globals, si, opg);
                if (ret != noErr) {
                    dbg_printf("??? -- CreateTrackAndMedia failed?: %ld\n", (long)ret);
                }

                /*err =*/ DecodeCommentsQT(globals, si, &si->si_theora.tc);
                //NotifyMovieChanged(globals);

                si->si_theora.state = kTStateReadingCodebooks;
            }
            break;

        case kTStateReadingCodebooks:
            ovret = ogg_stream_packetout(&si->os, &op);
            if (ovret < 0) {
                loop = false;
                ret = invalidMedia;
            } else if (ovret < 1) {
                loop = false;
            } else {
                unsigned long atomhead[2] = { EndianU32_NtoB(op.bytes + sizeof(atomhead)), EndianU32_NtoB(kCookieTypeTheoraCodebooks) };
                PtrAndHand(atomhead, si->soundDescExtension, sizeof(atomhead));
                PtrAndHand(op.packet, si->soundDescExtension, op.bytes);

                th_decode_headerin(&si->si_theora.ti, &si->si_theora.tc, &si->si_theora.ts, &op);
                {
                    unsigned long endAtom[2] = { EndianU32_NtoB(sizeof(endAtom)), EndianU32_NtoB(kAudioTerminatorAtomType) };

                    ret = PtrAndHand(endAtom, si->soundDescExtension, sizeof(endAtom));
                    if (ret == noErr) {
                        ret = AddImageDescriptionExtension((ImageDescriptionHandle) si->sampleDesc,
                                                           si->soundDescExtension, 'XYZ?' /* siDecompressionParams */);
                        //dbg_printf("??? -- Adding extension: %ld\n", ret);
                    } else {
                        //dbg_printf("??? -- Hmm, something went wrong: %ld\n", ret);
                    }
                }

                si->si_theora.state = kTStateReadingFirstPacket;
                si->insertTime = 0;
                si->streamOffset = globals->currentGroupOffset;
                si->incompleteCompensation = 0;
                loop = false; //there should be an end of page here according to specs...
            }
            break;

        case kTStateReadingFirstPacket:
            if (ogg_page_pageno(opg) > 3) {
                si->lastGranulePos = ogg_page_granulepos(opg);
                dbg_printf("----==< skipping: %llx, %lx\n", si->lastGranulePos, ogg_page_pageno(opg));
                loop = false;

                if (si->lastGranulePos < 0)
                    si->lastGranulePos = 0;
            }
            si->si_theora.state = kTStateReadingPackets;
#if 0
            {
                Handle h = NewHandleClear(sizeof(si->si_theora));
                ret = BeginMediaEdits(si->theMedia);
                dbg_printf("! -T-XXX:: ret = %ld\n", ret);
                ret = AddMediaSample(si->theMedia, h, 0, sizeof(si->si_theora), 249, si->sampleDesc, 1, 0, NULL);
                dbg_printf("! -T-XXX:: ret = %ld\n", ret);
                ret = EndMediaEdits(si->theMedia);
                dbg_printf("! -T-XXX:: ret = %ld\n", ret);
                ret = InsertMediaIntoTrack(si->theTrack, si->insertTime /*inserted*/, /* si->lastGranulePos */ 0,
                                           249, fixed1);
                dbg_printf("! -T-XXX:: ret = %ld\n", ret);
                si->insertTime = -1;
            }
#endif /* 0 */
            break;

        case kTStateReadingPackets:
            {
                ogg_int64_t pos       = ogg_page_granulepos(opg);
                int         len       = opg->header_len + opg->body_len;
                TimeValue   duration  = 0;
                TimeValue   inserted  = -1;
                short       smp_flags = 0;

                int packet_count = 0;
                long psize = 0, poffset = 0;
                int i, segments;
                Boolean continued = ogg_page_continued(opg);
                SampleReference64Record sampleRec;
                ogg_int64_t last_packet_pos = si->lastGranulePos >> si->si_theora.granulepos_shift;
                last_packet_pos += si->lastGranulePos - (last_packet_pos << si->si_theora.granulepos_shift);

#if 0
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
#endif /* 0 */

                if (continued)
                    smp_flags |= mediaSampleNotSync;

                segments = opg->header[26];
                for (i = 0; i < segments; i++) {
                    int val = opg->header[27 + i];
                    int incomplete = (i == segments - 1) && (val == 255);
                    TimeValue pduration = 0;
                    psize += val;
                    if (val < 255 || incomplete) {
                        pduration = si->si_theora.fps_framelen;
                        if (incomplete) {
                            pduration = 1;
                            si->incompleteCompensation -= 1;
                            psize += 4; // this should allow decoder to see that if (packet_len % 255 == 4) and
                                        // last 4 bytes contain 'OggS' sync pattern then that's an incomplete packet
                        } else if (continued) {
                            pduration += si->incompleteCompensation;
                            si->incompleteCompensation = 0;
                            /* QT doesn't like zero-size samples, do this: */
                            if (val == 0) {
                                psize = 1;
                                poffset -= 1;
                            }
                            /* */
                        }
                        if ((opg->body[poffset] & 0x40) != 0)
                            smp_flags |= mediaSampleNotSync;
                        memset(&sampleRec, 0, sizeof(sampleRec));
                        sampleRec.dataOffset = SInt64ToWide(globals->dataOffset + S64Set(poffset + opg->header_len));
                        sampleRec.dataSize = psize;
                        sampleRec.sampleFlags = smp_flags;
                        sampleRec.durationPerSample = pduration;
                        sampleRec.numberOfSamples = 1;
                        dbg_printf("   T   :++: adding sampleRef: %8lld, len: %8d, dur: %8d, fl: %08x\n",
                                   sampleRec.dataOffset, psize, pduration, smp_flags);
                        ret = AddMediaSampleReferences64(si->theMedia, si->sampleDesc, 1, &sampleRec, inserted == -1 ? &inserted : NULL);
                        if (ret != noErr)
                            break;
                        duration += pduration;
                        packet_count += 1;
                        poffset += psize;

                        psize = 0;
                        continued = false;
                        smp_flags = 0;
                    }
                }

#if 0
                while ((ovret = ogg_stream_packetout(&si->os, &op)) > 0) {
                    packet_count += 1;
                    last_packet_pos += 1;
                    memset(&sampleRec, 0, sizeof(sampleRec));
                    sampleRec.dataOffset = SInt64ToWide(globals->dataOffset + S64Set(opg->header_len + op->packet - opg->body)); // + packet offset within the page! - :/
                    sampleRec.dataSize = op.bytes;
                    sampleRec.sampleFlags = smp_flags;
                    sampleRec.durationPerSample = si->si_theora.ti.fps_denominator;
                    sampleRec.numberOfSamples = 1;
                    dbg_printf("   -   :++: adding sampleRef: %lld, len: %d, dur: %d\n", globals->dataOffset, len, duration);
                    ret = AddMediaSampleReferences64(si->theMedia, si->sampleDesc, 1, &sampleRec, inserted == -1 ? &inserted : NULL);
                    if (ret != noErr)
                        break;
                }
#endif /* 0 */
                loop = false;
                if (ovret < 0) {
                    ret = invalidMedia;
                    break;
                }

                
                if (ret == noErr && packet_count > 0) {
                    TimeValue timeLoaded;

                    dbg_printf("   -   :><: added page %04ld at %14ld (size: %5ld, tsize: %6d), f: %d\n",
                               ogg_page_pageno(opg), inserted,
                               opg->body_len, len, !logg_page_last_packet_incomplete(opg));
                    //dbg_printf("   -   :/>: inserting media: %ld, mt: %lld, dur: %d\n", si->insertTime, si->lastGranulePos, duration);
                    dbg_printf("   -   :/>: inserting media: %ld, mt: %ld, dur: %ld\n", si->insertTime, inserted, duration);
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
                        if (GetMovieTimeScale(globals->theMovie) < GetMediaTimeScale(si->theMedia)) {
                            dbg_printf("   # - changing movie time scale: %ld --> %ld\n",
                                       GetMovieTimeScale(globals->theMovie), GetMediaTimeScale(si->theMedia));
                            SetMovieTimeScale(globals->theMovie, GetMediaTimeScale(si->theMedia));
                        }
                    }
                    si->insertTime = -1;
                    timeLoaded = GetTrackDuration(si->theTrack);

                    dbg_printf("   -   :><: added page %04ld at %14ld; offset: %ld, duration: %ld (%ld, %ld), mediats: %ld; moviets: %ld, ret = %ld\n",
                               ogg_page_pageno(opg), inserted,
                               GetTrackOffset(si->theTrack), GetTrackDuration(si->theTrack), timeLoaded,
                               (duration * GetMovieTimeScale(globals->theMovie)) / GetMediaTimeScale(si->theMedia),
                               GetMediaTimeScale(si->theMedia), GetMovieTimeScale(globals->theMovie), ret);
                    if (globals->timeLoaded < timeLoaded)
                        globals->timeLoaded = timeLoaded;

                    movie_changed = true;
                }
                
#if 0
                dbg_printf("   -   :++: adding sampleRef: %lld, len: %d, dur: %d\n", globals->dataOffset, len, duration);
                ret = AddMediaSampleReference(si->theMedia, S32Set(globals->dataOffset),
                                              len, duration, si->sampleDesc, 1, smp_flags, &inserted); //@@@@ 64-bit enable
                if (ret == noErr) {
                    TimeValue timeLoaded;

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
                        if (GetMovieTimeScale(globals->theMovie) < GetMediaTimeScale(si->theMedia)) {
                            dbg_printf("   # - changing movie time scale: %ld --> %ld\n",
                                       GetMovieTimeScale(globals->theMovie), GetMediaTimeScale(si->theMedia));
                            SetMovieTimeScale(globals->theMovie, GetMediaTimeScale(si->theMedia));
                        }
                    }
                    si->insertTime = -1;
                    timeLoaded = GetTrackDuration(si->theTrack);

                    dbg_printf("   -   :><: added page %04ld at %14ld; offset: %ld, duration: %ld (%ld, %ld), mediats: %ld; moviets: %ld, ret = %ld\n",
                               ogg_page_pageno(opg), inserted,
                               GetTrackOffset(si->theTrack), GetTrackDuration(si->theTrack), timeLoaded,
                               (duration * GetMovieTimeScale(globals->theMovie)) / GetMediaTimeScale(si->theMedia),
                               GetMediaTimeScale(si->theMedia), GetMovieTimeScale(globals->theMovie), ret);
                    if (globals->timeLoaded < timeLoaded)
                        globals->timeLoaded = timeLoaded;

                    movie_changed = true;
                }
#endif /* 0 */
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
