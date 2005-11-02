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

#include "OggImport.h"

#include "fccs.h"
#include "data_types.h"


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
    Handle	desc = NewHandleClear(sizeof(SoundDescriptionV1));
	
    if (desc != NULL)
    {
        SoundDescriptionV1Ptr	snd = (SoundDescriptionV1Ptr)*desc;
        
        snd->desc.descSize = sizeof(SoundDescriptionV1);
        snd->desc.dataFormat = kAudioFormatXiphOggFramedSpeex;
        snd->desc.dataRefIndex = 1;
        snd->desc.version = 1;   //@@@@ use the version from the stream????
        snd->desc.revlevel = 0;
        snd->desc.vendor = kSoundComponentManufacturer;
        snd->desc.numChannels = si->numChannels;
        snd->desc.compressionID = variableCompression;
        snd->desc.sampleSize = 16;
        snd->desc.sampleRate = ((unsigned)si->rate) << 16;
		
		snd->samplesPerPacket = 0;
		snd->bytesPerSample = 2;
		snd->bytesPerFrame = 0;
		snd->bytesPerPacket = 0;
        
		si->sampleDesc = (SampleDescriptionHandle)desc;
    } else
        err = MemError();
	
    return err;
};

int process_first_packet__speex(StreamInfo *si, ogg_page *op, ogg_packet *opckt)
{
    unsigned long serialnoatom[3] = { EndianU32_NtoB(sizeof(serialnoatom)), EndianU32_NtoB(kCookieTypeOggSerialNo),
        EndianS32_NtoB(ogg_page_serialno(op)) };
    unsigned long atomhead[2] = { EndianU32_NtoB(opckt->bytes + sizeof(atomhead)), EndianU32_NtoB(kCookieTypeSpeexHeader) };
    SpeexHeader *inheader = (SpeexHeader *) opckt->packet;
    //vorbis_synthesis_headerin(&si->si_vorbis.vi, &si->si_vorbis.vc, opckt); //check errors?
    
    si->si_speex.header.bitrate					= EndianS32_LtoN(inheader->bitrate);
    si->si_speex.header.extra_headers			= EndianS32_LtoN(inheader->extra_headers);
    si->si_speex.header.frame_size				= EndianS32_LtoN(inheader->frame_size);
    si->si_speex.header.frames_per_packet		= EndianS32_LtoN(inheader->frames_per_packet);
    si->si_speex.header.header_size				= EndianS32_LtoN(inheader->header_size);
    si->si_speex.header.mode					= EndianS32_LtoN(inheader->mode);
    si->si_speex.header.mode_bitstream_version	= EndianS32_LtoN(inheader->mode_bitstream_version);
    si->si_speex.header.nb_channels				= EndianS32_LtoN(inheader->nb_channels);
    si->si_speex.header.rate					= EndianS32_LtoN(inheader->rate);
    si->si_speex.header.reserved1				= EndianS32_LtoN(inheader->reserved1);
    si->si_speex.header.reserved2				= EndianS32_LtoN(inheader->reserved2);
    si->si_speex.header.speex_version_id		= EndianS32_LtoN(inheader->speex_version_id);
    si->si_speex.header.vbr						= EndianS32_LtoN(inheader->vbr);
    //si->si_speex.header. = EndianS32_LtoN(inheader->);

    dprintf("! -- - speex_first_packet: ch: %d, rate: %ld\n", si->si_speex.header.nb_channels, si->si_speex.header.rate);
    si->numChannels = si->si_speex.header.nb_channels;
    si->rate = si->si_speex.header.rate;

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
                    //vorbis_synthesis_headerin(&si->si_vorbis.vi, &si->si_vorbis.vc, &op);
                    
					ret = CreateTrackAndMedia(globals, si, opg);
					if (ret != noErr) {
						dprintf("??? -- CreateTrackAndMedia failed?: %ld\n", (long)ret);
					}
                    
					// /*err =*/ DecodeCommentsQT(globals, si, &si->si_vorbis.vc);
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
                        //dprintf("??? -- Adding extension: %ld\n", ret);
                    } else {
                        //dprintf("??? -- Hmm, something went wrong: %ld\n", ret);
                    }
                    
					si->startTime = 0;
					si->prevPageOffset = S64Add(globals->dataOffset, opg->header_len + opg->body_len);

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
					si->prevPageOffset = S64Add(globals->dataOffset, opg->header_len + opg->body_len);
					dprintf("----==< skipping: %llx, %lx\n", si->lastGranulePos, ogg_page_pageno(opg));
					loop = false;
                    
					if (si->lastGranulePos < 0)
						si->lastGranulePos = 0;
				}
				si->si_speex.state = kSStateReadingPackets;
				break;
                
			case kVStateReadingPackets:
			    {
                    ogg_int64_t	pos 	  = ogg_page_granulepos(opg);
                    SInt64      endOffset = S64Add(globals->dataOffset, opg->header_len + opg->body_len);
                    int         len 	  = S64Subtract(endOffset, si->prevPageOffset);
                    int         duration  = pos - si->lastGranulePos;
                    TimeValue	inserted  = 0;
				
                    if (pos < 0) {
                        //dprintf("   -   :XX: not added page %ld (single, looooong packet)\n", ogg_page_pageno(opg));
                    } else {
                        dprintf("   -   :++: adding sampleRef: %lld, len: %d, dur: %d\n", si->prevPageOffset, len, duration);
                        ret = AddMediaSampleReference(si->theMedia, si->prevPageOffset,
                                                      len, duration, si->sampleDesc, 1, 0, &inserted); //@@@@ 64-bit enable
                        if (ret == noErr) {
                            dprintf("   -   :><: added page %04ld at %14ld (size: %5ld, tsize: %6d), f: %d\n",
                                    ogg_page_pageno(opg), inserted,
                                    opg->header_len + opg->body_len, len, !logg_page_last_packet_incomplete(opg));
                            dprintf("   -   :/>: inserting media: %ld, mt: %lld, dur: %d\n", si->startTime, si->lastGranulePos, duration);
                            ret = InsertMediaIntoTrack(si->theTrack, si->startTime /*inserted*/, /* si->lastGranulePos */ inserted, 
                                                       duration, fixed1);
                            si->startTime = -1;
                            si->timeLoaded = GetTrackOffset(si->theTrack) + GetTrackDuration(si->theTrack);
                            //if (globals->dataIsStream)
                            //	si->timeLoaded = (duration + inserted) * GetMovieTimeScale(globals->theMovie) / GetMediaTimeScale(si->theMedia);
                            
                            dprintf("   -   :><: added page %04ld at %14ld; offset: %ld, duration: %ld (%ld, %ld), mediats: %ld; moviets: %ld, ret = %ld\n",
                                    ogg_page_pageno(opg), inserted,
                                    GetTrackOffset(si->theTrack), GetTrackDuration(si->theTrack), si->timeLoaded,
                                    (duration * GetMovieTimeScale(globals->theMovie)) / GetMediaTimeScale(si->theMedia),
                                    GetMediaTimeScale(si->theMedia), GetMovieTimeScale(globals->theMovie), ret);
                            if (globals->timeLoaded < si->timeLoaded)
                                globals->timeLoaded = si->timeLoaded;
                            
                            movie_changed = true;
                        }
                        
                        si->prevPageOffset = endOffset;
                        si->lastGranulePos = pos;
                    }
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

