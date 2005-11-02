/*
 *  stream_vorbis.c
 *
 *    Vorbis format related part of OggImporter.
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


#include "stream_vorbis.h"

#include "debug.h"

#include "OggImport.h"

#include "fccs.h"
#include "data_types.h"


int recognize_header__vorbis(ogg_page *op)
{
    if (!strncmp("\1vorbis", (char *)op->body, 7))
        return 0;

    return 1;
};

int verify_header__vorbis(ogg_page *op) //?
{
    OSErr			 err = noErr;
    
	ogg_stream_state os;
	ogg_packet       opk;
    
	vorbis_info      vi;
	vorbis_comment   vc;
    
	ogg_stream_init(&os, ogg_page_serialno(op));
    
    vorbis_info_init(&vi);
    vorbis_comment_init(&vc);
    
	if (ogg_stream_pagein(&os, op) < 0)
		err = invalidMedia;
	else if (ogg_stream_packetout(&os, &opk) != 1)
		err = invalidMedia;
	else if (vorbis_synthesis_headerin(&vi, &vc, &opk) < 0)
		err = noSoundTrackInMovieErr;
	
	ogg_stream_clear(&os);
    
    vorbis_comment_clear(&vc);
    vorbis_info_clear(&vi);
    
    return err;
};

int initialize_stream__vorbis(StreamInfo *si)
{
    vorbis_info_init(&si->si_vorbis.vi);
    vorbis_comment_init(&si->si_vorbis.vc);
    
    si->si_vorbis.state = kVStateInitial;

    return 0;
};

void clear_stream__vorbis(StreamInfo *si)
{
	vorbis_info_clear(&si->si_vorbis.vi);
	vorbis_comment_clear(&si->si_vorbis.vc);
};

ComponentResult create_sample_description__vorbis(StreamInfo *si)
{
    ComponentResult err = noErr;
    Handle	desc = NewHandleClear(sizeof(SoundDescriptionV1));
	
    if (desc != NULL)
    {
        SoundDescriptionV1Ptr	snd = (SoundDescriptionV1Ptr)*desc;
        
        snd->desc.descSize = sizeof(SoundDescriptionV1);
        snd->desc.dataFormat = kAudioFormatXiphOggFramedVorbis;
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

int process_first_packet__vorbis(StreamInfo *si, ogg_page *op, ogg_packet *opckt)
{
    unsigned long serialnoatom[3] = { EndianU32_NtoB(sizeof(serialnoatom)), EndianU32_NtoB(kCookieTypeOggSerialNo),
        EndianS32_NtoB(ogg_page_serialno(op)) };
    unsigned long atomhead[2] = { EndianU32_NtoB(opckt->bytes + sizeof(atomhead)), EndianU32_NtoB(kCookieTypeVorbisHeader) };

    vorbis_synthesis_headerin(&si->si_vorbis.vi, &si->si_vorbis.vc, opckt); //check errors?
    
    si->numChannels = si->si_vorbis.vi.channels;
    si->rate = si->si_vorbis.vi.rate;

    PtrAndHand(serialnoatom, si->soundDescExtension, sizeof(serialnoatom)); //check errors?
    PtrAndHand(atomhead, si->soundDescExtension, sizeof(atomhead)); //check errors?
    PtrAndHand(opckt->packet, si->soundDescExtension, opckt->bytes); //check errors?

    si->si_vorbis.state = kVStateReadingComments;

    return 0;
};

ComponentResult process_stream_page__vorbis(OggImportGlobals *globals, StreamInfo *si, ogg_page *opg)
{
	ComponentResult ret = noErr;
	int ovret = 0;
	Boolean loop = true;
	Boolean movie_changed = false;
    
	ogg_packet op;
	
	switch(si->si_vorbis.state) {
		case kVStateReadingComments:
		case kVStateReadingCodebooks:
			ogg_stream_pagein(&si->os, opg);
			break;
		default:
			break;
	}
    
	do {
		switch(si->si_vorbis.state) {
			case kVStateReadingComments:
				ovret = ogg_stream_packetout(&si->os, &op);
				if (ovret < 0) {
					loop = false;
					ret = invalidMedia;
				} else if (ovret < 1) {
					loop = false;
				} else {
                    unsigned long atomhead[2] = { EndianU32_NtoB(op.bytes + sizeof(atomhead)), EndianU32_NtoB(kCookieTypeVorbisComments) };

                    PtrAndHand(atomhead, si->soundDescExtension, sizeof(atomhead));
					PtrAndHand(op.packet, si->soundDescExtension, op.bytes);
					vorbis_synthesis_headerin(&si->si_vorbis.vi, &si->si_vorbis.vc, &op);
                    
					ret = CreateTrackAndMedia(globals, si, opg);
					if (ret != noErr) {
						dprintf("??? -- CreateTrackAndMedia failed?: %ld\n", (long)ret);
					}
                    
					/*err =*/ DecodeCommentsQT(globals, si, &si->si_vorbis.vc);
					//NotifyMovieChanged(globals);
                    
					si->si_vorbis.state = kVStateReadingCodebooks;
				}
                    break;
                
			case kVStateReadingCodebooks:
				ovret = ogg_stream_packetout(&si->os, &op);
				if (ovret < 0) {
					loop = false;
					ret = invalidMedia;
				} else if (ovret < 1) {
					loop = false;
				} else {
                    unsigned long atomhead[2] = { EndianU32_NtoB(op.bytes + sizeof(atomhead)), EndianU32_NtoB(kCookieTypeVorbisCodebooks) };
                    PtrAndHand(atomhead, si->soundDescExtension, sizeof(atomhead));
					PtrAndHand(op.packet, si->soundDescExtension, op.bytes);
                    
					vorbis_synthesis_headerin(&si->si_vorbis.vi, &si->si_vorbis.vc, &op);
					{
						unsigned long endAtom[2] = { EndianU32_NtoB(sizeof(endAtom)), EndianU32_NtoB(kAudioTerminatorAtomType) };
                        
						ret = PtrAndHand(endAtom, si->soundDescExtension, sizeof(endAtom));
						if (ret == noErr) {
							ret = AddSoundDescriptionExtension((SoundDescriptionHandle) si->sampleDesc,
															   si->soundDescExtension, siDecompressionParams);
							//dprintf("??? -- Adding extension: %ld\n", ret);
						} else {
							//dprintf("??? -- Hmm, something went wrong: %ld\n", ret);
						}
					}
                    
					si->si_vorbis.state = kVStateReadingFirstPacket;
					si->startTime = 0;
					si->prevPageOffset = S64Add(globals->dataOffset, opg->header_len + opg->body_len);
					loop = false; //there should be an end of page here according to specs...
				}
                    break;
                
			case kVStateReadingFirstPacket:
				if (ogg_page_pageno(opg) > 3) {
					si->lastGranulePos = ogg_page_granulepos(opg);
					si->prevPageOffset = S64Add(globals->dataOffset, opg->header_len + opg->body_len);
					dprintf("----==< skipping: %llx, %lx\n", si->lastGranulePos, ogg_page_pageno(opg));
					loop = false;
                    
					if (si->lastGranulePos < 0)
						si->lastGranulePos = 0;
				}
				si->si_vorbis.state = kVStateReadingPackets;
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
					if (ret == noErr)
					{
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

