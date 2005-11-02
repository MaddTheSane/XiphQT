/*
 *  importer_types.h
 *
 *    Definitions of OggImporter data structures.
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


#if !defined(__importer_types_h__)
#define __importer_types_h__

#include <QuickTime/QuickTime.h>
#include <Ogg/ogg.h>

#include "rb.h"


#include "stream_types_vorbis.h"
#include "stream_types_speex.h"


typedef enum ImportStates {
	kStateInitial,
	kStateGettingSize,
	kStateReadingPages,
	kStateReadingLastPages,
	kStateImportComplete
} ImportStates;


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//			types
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Keep your data long word aligned for best performance

struct stream_format_handle_funcs; //forward declaration

typedef struct {
    long                serialno;
    TimeValue           timeLoaded;
	
	ogg_stream_state	os;
	
    int                 numChannels;
    int                 rate;
    Handle              soundDescExtension;
	
    Track               theTrack;
    Media               theMedia;
    SampleDescriptionHandle sampleDesc;
    
    ogg_int64_t         lastGranulePos;
    SInt64              prevPageOffset;
    
    ogg_int64_t         lastSeenGranulePos;
    SInt64              lastSeenEndOffset;
    
    TimeValue           startTime;
    
	CFDictionaryRef		MDmapping;
	CFDictionaryRef		UDmapping;
    
    struct stream_format_handle_funcs *sfhf;

    union {
#if defined(_HAVE__VORBIS_SUPPORT)
        StreamInfo__vorbis si_vorbis;
#endif
#if defined(_HAVE__SPEEX_SUPPORT)
        StreamInfo__speex si_speex;
#endif
    };

} StreamInfo, *StreamInfoPtr;


typedef struct {
    ComponentInstance	    self;
    
    Movie               theMovie;
    Handle              dataRef;
    long                dataRefType;
    Boolean             dataIsStream;
    Boolean             usingIdle;
    int                 chunkSize;
    TimeValue           startTime;
    
    ImportStates            state;
    ComponentResult         errEncountered;
    
    SInt64                  dataStartOffset;
    SInt64                  dataEndOffset;
    
    IdleManager             idleManager;
    IdleManager             dataIdleManager;
    
    long                    newMovieFlags;
    
    ComponentInstance	    dataReader;
    
    DataHCompletionUPP	    dataReadCompletion;
    DataHCompletionUPP	    fileSizeCompletion;
    
    wide                    wideTempforFileSize;
    
    int	                    dataReadChunkSize;
    
    Boolean                 dataCanDoAsyncRead;
    Boolean                 dataCanDoGetFileSizeAsync;
    Boolean                 dataCanDoGetFileSize64;
    
    Boolean                 blocking;
    AliasHandle             aliasHandle;
    
    ComponentResult         readError;
    
    /* information about the data buffer */
    Ptr                     dataBuffer;
    int                     maxDataBufferSize;
    SInt64                  dataOffset;
    unsigned char           *validDataEnd;
    unsigned char           *currentData;
    
	ring_buffer				dataRB;
    
    int                     numTracksStarted;
    int                     numTracksSeen;		// completed tracks
    Track                   firstTrack;
    
    TimeValue               timeLoaded;
    
	unsigned long           startTickCount;
    
    //    Track                   ghostTrack;
    
    int                     streamCount;
    StreamInfo              **streamInfoHandle;
    
	Boolean					dataRequested;
	Boolean					sizeInitialised;

} OggImportGlobals, *OggImportGlobalsPtr;


typedef int (*recognize_header) (ogg_page *op);
typedef int (*verify_header) (ogg_page *op);

typedef int (*initialize_stream) (StreamInfo *si);
typedef void (*clear_stream) (StreamInfo *si);
typedef ComponentResult (*create_sample_description) (StreamInfo *si);

typedef int (*process_first_packet) (StreamInfo *si, ogg_page *op, ogg_packet *opckt);
typedef ComponentResult (*process_stream_page) (OggImportGlobals *globals, StreamInfo *si, ogg_page *opg);


typedef struct stream_format_handle_funcs {
    process_stream_page			process_page;

    recognize_header			recognize;
    verify_header				verify;
    
    process_first_packet		first_packet;
    create_sample_description	sample_description;

    initialize_stream			initialize;
    clear_stream				clear;
} stream_format_handle_funcs;

#define HANDLE_FUNCTIONS__NULL { NULL, NULL, NULL, NULL, NULL, NULL, NULL }

#endif /* __importer_types_h__ */