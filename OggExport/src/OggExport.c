/*
 *  OggExport.c
 *
 *    The main part of the OggExport component.
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


#if defined(__APPLE_CC__)
#include <QuickTime/QuickTime.h>
#else
#include <QuickTimeComponents.h>
#endif

#include <Ogg/ogg.h>

#include "OggExport.h"
#include "debug.h"
#include "exporter_types.h"

#include "stream_audio.h"

// stream-type handle functions
#include "stream_audio.h"
//#include "stream_video.h"

static stream_format_handle_funcs s_formats[] = {
#if defined(_HAVE__OE_AUDIO)
    HANDLE_FUNCTIONS__AUDIO,
#endif
#if defined(_HAVE__OE_VIDEO)
    HANDLE_FUNCTIONS__VIDEO,
#endif

    HANDLE_FUNCTIONS__NULL
};


#define USE_NIB_FILE 1

/* component selector methods */
pascal ComponentResult OggExportOpen(OggExportGlobalsPtr globals, ComponentInstance self);
pascal ComponentResult OggExportClose(OggExportGlobalsPtr globals, ComponentInstance self);
pascal ComponentResult OggExportVersion(OggExportGlobalsPtr globals);
pascal ComponentResult OggExportGetComponentPropertyInfo(OggExportGlobalsPtr   globals,
                                                         ComponentPropertyClass inPropClass,
                                                         ComponentPropertyID    inPropID,
                                                         ComponentValueType     *outPropType,
                                                         ByteCount              *outPropValueSize,
                                                         UInt32                 *outPropertyFlags);
pascal ComponentResult OggExportGetComponentProperty(OggExportGlobalsPtr  globals,
                                                     ComponentPropertyClass inPropClass,
                                                     ComponentPropertyID    inPropID,
                                                     ByteCount              inPropValueSize,
                                                     ComponentValuePtr      outPropValueAddress,
                                                     ByteCount              *outPropValueSizeUsed);
pascal ComponentResult OggExportSetComponentProperty(OggExportGlobalsPtr  globals,
                                                     ComponentPropertyClass inPropClass,
                                                     ComponentPropertyID    inPropID,
                                                     ByteCount              inPropValueSize,
                                                     ConstComponentValuePtr inPropValueAddress);
pascal ComponentResult OggExportToFile(OggExportGlobalsPtr globals, const FSSpec *theFilePtr,
                                       Movie theMovie, Track onlyThisTrack, TimeValue startTime,
                                       TimeValue duration);
pascal ComponentResult OggExportToDataRef(OggExportGlobalsPtr globals, Handle dataRef, OSType dataRefType,
                                          Movie theMovie, Track onlyThisTrack, TimeValue startTime, TimeValue duration);
pascal ComponentResult OggExportFromProceduresToDataRef(OggExportGlobalsPtr globals, Handle dataRef, OSType dataRefType);
pascal ComponentResult OggExportAddDataSource(OggExportGlobalsPtr globals, OSType trackType, TimeScale scale,
                                              long *trackIDPtr, MovieExportGetPropertyUPP getPropertyProc,
                                              MovieExportGetDataUPP getDataProc, void *refCon);
pascal ComponentResult OggExportValidate(OggExportGlobalsPtr globals, Movie theMovie, Track onlyThisTrack, Boolean *valid);
pascal ComponentResult OggExportSetProgressProc(OggExportGlobalsPtr globals, MovieProgressUPP proc, long refcon);


#if USE_NIB_FILE
static pascal OSStatus SettingsWindowEventHandler(EventHandlerCallRef inHandler, EventRef inEvent, void *inUserData);
#endif

pascal ComponentResult OggExportDoUserDialog(OggExportGlobalsPtr globals, Movie theMovie, Track onlyThisTrack,
                                             TimeValue startTime, TimeValue duration, Boolean *canceledPtr);


pascal ComponentResult OggExportGetSettingsAsAtomContainer(OggExportGlobalsPtr globals, QTAtomContainer *settings);
pascal ComponentResult OggExportSetSettingsFromAtomContainer(OggExportGlobalsPtr globals, QTAtomContainer settings);
pascal ComponentResult OggExportGetFileNameExtension(OggExportGlobalsPtr globals, OSType *extension);
pascal ComponentResult OggExportGetShortFileTypeString(OggExportGlobalsPtr globals, Str255 typeString);
pascal ComponentResult OggExportGetSourceMediaType(OggExportGlobalsPtr globals, OSType *mediaType);


/* utility functions */
static stream_format_handle_funcs* find_stream_support(OSType trackType, TimeScale scale,
                                                       MovieExportGetPropertyUPP getPropertyProc,
                                                       void *refCon);
static ComponentResult OpenStream(OggExportGlobalsPtr globals, OSType trackType, TimeScale scale,
                                  long *trackIDPtr, MovieExportGetPropertyUPP getPropertyProc,
                                  MovieExportGetDataUPP getDataProc, void *refCon,
                                  stream_format_handle_funcs *ff, StreamInfoPtr *out_si);
static void _close_stream(OggExportGlobalsPtr globals, StreamInfoPtr si);
static void CloseAllStreams(OggExportGlobalsPtr globals);

static OSErr ConfigureQuickTimeMovieExporter(OggExportGlobalsPtr globals);
static ComponentResult ConfigureStdComponents(OggExportGlobalsPtr globals);

static ComponentResult mux_streams(OggExportGlobalsPtr globals, DataHandler data_h);



#define CALLCOMPONENT_BASENAME()        OggExport
#define CALLCOMPONENT_GLOBALS()         OggExportGlobalsPtr storage

#define MOVIEEXPORT_BASENAME()          CALLCOMPONENT_BASENAME()
#define MOVIEEXPORT_GLOBALS() 	        CALLCOMPONENT_GLOBALS()

#define COMPONENT_UPP_SELECT_ROOT()	MovieExport
#define COMPONENT_DISPATCH_FILE		"OggExportDispatch.h"

#if !TARGET_OS_WIN32
#include <CoreServices/Components.k.h>
#include <QuickTime/QuickTimeComponents.k.h>
#include <QuickTime/ImageCompression.k.h>   // for ComponentProperty selectors
#include <QuickTime/ComponentDispatchHelper.c>
#else
#include <Components.k.h>
#include <QuickTimeComponents.k.h>
#include <ImageCompression.k.h>
#include <ComponentDispatchHelper.c>
#endif


pascal ComponentResult OggExportOpen(OggExportGlobalsPtr globals, ComponentInstance self) {
    ComponentDescription cd;
    ComponentResult err;

    dbg_printf("[  OE]  >> [%08lx] :: Open()\n", (UInt32) globals);

    globals = (OggExportGlobalsPtr) NewPtrClear(sizeof(OggExportGlobals));
    err = MemError();
    if (!err) {
        globals->self = self;
        globals->use_hires_audio = false;

        SetComponentInstanceStorage(self, (Handle) globals);

        // Get the QuickTime Movie export component
        // Because we use the QuickTime Movie export component, search for
        // the 'MooV' exporter using the following ComponentDescription values
        cd.componentType = MovieExportType;
        cd.componentSubType = kQTFileTypeMovie;
        cd.componentManufacturer = kAppleManufacturer;
        cd.componentFlags = canMovieExportFromProcedures | movieExportMustGetSourceMediaType;
        cd.componentFlagsMask = cd.componentFlags;

        err = OpenAComponent(FindNextComponent(NULL, &cd), &globals->quickTimeMovieExporter);
    }

    dbg_printf("[  OE] <   [%08lx] :: Open()\n", (UInt32) globals);
    return err;
}


pascal ComponentResult OggExportClose(OggExportGlobalsPtr globals, ComponentInstance self) {
    dbg_printf("[  OE]  >> [%08lx] :: Close()\n", (UInt32) globals);

    if (globals) {
        if (globals->quickTimeMovieExporter)
            CloseComponent(globals->quickTimeMovieExporter);

        if (globals->streamInfoHandle) {
            if (globals->streamCount > 0)
                CloseAllStreams(globals);
            DisposeHandle((Handle) globals->streamInfoHandle);
        }

        DisposePtr((Ptr) globals);
    }

    dbg_printf("[  OE] <   [%08lx] :: Close()\n", (UInt32) globals);
    return noErr;
}

pascal ComponentResult OggExportVersion(OggExportGlobalsPtr globals) {
#pragma unused(globals)
    dbg_printf("[  OE]  >> [%08lx] :: Version()\n", (UInt32) globals);
    dbg_printf("[  OE] <   [%08lx] :: Version()\n", (UInt32) globals);
    return kOgg_spit__Version;
}

pascal ComponentResult OggExportGetComponentPropertyInfo(OggExportGlobalsPtr   globals,
                                                         ComponentPropertyClass inPropClass,
                                                         ComponentPropertyID    inPropID,
                                                         ComponentValueType     *outPropType,
                                                         ByteCount              *outPropValueSize,
                                                         UInt32                 *outPropertyFlags)
{
    ComponentResult err = noErr;
    dbg_printf("[  OE]  >> [%08lx] :: GetComponentPropertyInfo('%4.4s', '%4.4s')\n", (UInt32) globals, (char *) &inPropClass, (char *) &inPropID);
    dbg_printf("[  OE] <   [%08lx] :: GetComponentPropertyInfo() = %ld\n", (UInt32) globals, err);
    return err;
}

pascal ComponentResult OggExportGetComponentProperty(OggExportGlobalsPtr  globals,
                                                     ComponentPropertyClass inPropClass,
                                                     ComponentPropertyID    inPropID,
                                                     ByteCount              inPropValueSize,
                                                     ComponentValuePtr      outPropValueAddress,
                                                     ByteCount              *outPropValueSizeUsed)
{
    dbg_printf("[  OE]  >> [%08lx] :: GetComponentProperty('%4.4s', '%4.4s', %ld)\n", (UInt32) globals, (char *) &inPropClass, (char *) &inPropID, inPropValueSize);
    dbg_printf("[  OE] <   [%08lx] :: GetComponentProperty()\n", (UInt32) globals);
    return noErr;
}

pascal ComponentResult OggExportSetComponentProperty(OggExportGlobalsPtr  globals,
                                                     ComponentPropertyClass inPropClass,
                                                     ComponentPropertyID    inPropID,
                                                     ByteCount              inPropValueSize,
                                                     ConstComponentValuePtr inPropValueAddress)
{
    ComponentResult err = noErr;
    dbg_printf("[  OE]  >> [%08lx] :: SetComponentProperty('%4.4s', '%4.4s', %ld)\n", (UInt32) globals, (char *) &inPropClass, (char *) &inPropID, inPropValueSize);

    switch (inPropClass) {
    case kQTPropertyClass_MovieExporter:
        switch (inPropID) {
        case kQTMovieExporterPropertyID_EnableHighResolutionAudioFeatures:
            {
                Boolean use_hqa = globals->use_hires_audio;
                if (inPropValueSize == sizeof(Boolean)) {
                    use_hqa = *(Boolean *) inPropValueAddress;
                } else if (inPropValueSize == sizeof(UInt8)) {
                    use_hqa = (Boolean) *(UInt8 *) inPropValueAddress;
                } else {
                    err = kQTPropertyBadValueSizeErr;
                    break;
                }

                if (use_hqa && use_hqa != globals->use_hires_audio) {
                    err = QTSetComponentProperty(globals->quickTimeMovieExporter, kQTPropertyClass_MovieExporter,
                                                 kQTMovieExporterPropertyID_EnableHighResolutionAudioFeatures,
                                                 inPropValueSize, inPropValueAddress);
                    if (err)
                        break;
                }
                globals->use_hires_audio = use_hqa;
            }
            break;

        default:
            err = kQTPropertyNotSupportedErr;
            break;
        }
        break;

    default:
        err = kQTPropertyNotSupportedErr;
        break;
    }

    dbg_printf("[  OE] <   [%08lx] :: SetComponentProperty() = %ld\n", (UInt32) globals, err);
    return err;
}

pascal ComponentResult OggExportValidate(OggExportGlobalsPtr globals, Movie theMovie, Track onlyThisTrack, Boolean *valid) {
    OSErr err;

    dbg_printf("[  OE]  >> [%08lx] :: Validate()\n", (UInt32) globals);

    // The QT movie export component must be cool with this before we can be
    err = MovieExportValidate(globals->quickTimeMovieExporter, theMovie, onlyThisTrack, valid);
    if (!err) {
        if (*valid == true) {

            if (onlyThisTrack == NULL) {
                if (GetMovieIndTrackType(theMovie, 1, VisualMediaCharacteristic, movieTrackCharacteristic | movieTrackEnabledOnly) == NULL &&
                    GetMovieIndTrackType(theMovie, 1, AudioMediaCharacteristic, movieTrackCharacteristic | movieTrackEnabledOnly) == NULL)
                    *valid = false;
            } else {
                MediaHandler mh = GetMediaHandler(GetTrackMedia(onlyThisTrack));
                Boolean hasIt = false;

                MediaHasCharacteristic(mh, VisualMediaCharacteristic, &hasIt);
                if (hasIt == false)
                    MediaHasCharacteristic(mh, AudioMediaCharacteristic, &hasIt);
                if (hasIt == false)
                    *valid = false;
            }
        }
    }

    dbg_printf("[  OE] <   [%08lx] :: Validate() = %d, %d\n", (UInt32) globals, err, *valid);
    return err;
}



pascal ComponentResult OggExportToFile(OggExportGlobalsPtr globals, const FSSpec *theFilePtr,
                                       Movie theMovie, Track onlyThisTrack, TimeValue startTime,
                                       TimeValue duration)
{
    AliasHandle alias;
    ComponentResult err;

    dbg_printf("[  OE]  >> [%08lx] :: ToFile(%d, %ld, %ld)\n", (UInt32) globals, onlyThisTrack != NULL, startTime, duration);

    err = QTNewAlias(theFilePtr, &alias, true);
    if (!err) {
        err = MovieExportToDataRef(globals->self, (Handle) alias, rAliasType, theMovie, onlyThisTrack, startTime, duration);

        DisposeHandle((Handle) alias);
    }

    dbg_printf("[  OE] <   [%08lx] :: ToFile()\n", (UInt32) globals);
    return err;
}

pascal ComponentResult OggExportToDataRef(OggExportGlobalsPtr globals, Handle dataRef, OSType dataRefType,
                                          Movie theMovie, Track onlyThisTrack, TimeValue startTime, TimeValue duration)
{
    TimeScale scale;
    MovieExportGetPropertyUPP getSoundPropertyProc = NULL;
    MovieExportGetDataUPP getSoundDataProc = NULL;
    void *audioRefCon;
    long trackID;
    ComponentResult err;

    dbg_printf("[  OE]  >> [%08lx] :: ToDataRef(%d, %ld, %ld)\n", (UInt32) globals, onlyThisTrack != NULL, startTime, duration);

    // TODO: loop for all tracks

    /*
    err = MovieExportNewGetDataAndPropertiesProcs(globals->quickTimeMovieExporter, VideoMediaType, &scale, theMovie,
                                                  onlyThisTrack, startTime, duration, &getVideoPropertyProc,
                                                  &getVideoDataProc, &videoRefCon);
    */

    err = MovieExportNewGetDataAndPropertiesProcs(globals->quickTimeMovieExporter, SoundMediaType, &scale, theMovie,
                                                  onlyThisTrack, startTime, duration, &getSoundPropertyProc,
                                                  &getSoundDataProc, &audioRefCon);

    dbg_printf("[  OE]   = [%08lx] :: ToDataRef() = %ld\n", (UInt32) globals, err);
    if (!err) {
        // ** Add the audio data source **
        err = MovieExportAddDataSource(globals->self, SoundMediaType, scale, &trackID, getSoundPropertyProc, getSoundDataProc, audioRefCon);
        if (!err) {
            err = MovieExportFromProceduresToDataRef(globals->self, dataRef, dataRefType);
        }
    }

    if (getSoundPropertyProc || getSoundDataProc)
        MovieExportDisposeGetDataAndPropertiesProcs(globals->quickTimeMovieExporter, getSoundPropertyProc, getSoundDataProc, audioRefCon);

    dbg_printf("[  OE] <   [%08lx] :: ToDataRef() = 0x%04lx, %ld\n", (UInt32) globals, err, trackID);
    return err;
}

pascal ComponentResult OggExportFromProceduresToDataRef(OggExportGlobalsPtr globals, Handle dataRef, OSType dataRefType)
{
    DataHandler    dataH = NULL;
    ComponentResult err;

    dbg_printf("[  OE]  >> [%08lx] :: FromProceduresToDataRef()\n", (UInt32) globals);

    if (!dataRef || !dataRefType)
        return paramErr;

    // Get and open a Data Handler Component that can write to the dataRef
    err = OpenAComponent(GetDataHandler(dataRef, dataRefType, kDataHCanWrite), &dataH);
    if (err)
        goto bail;

    DataHSetDataRef(dataH, dataRef);

    // Create the file - ?
    err = DataHCreateFile(dataH, FOUR_CHAR_CODE('TVOD'), false);
    if (err)
        goto bail;

    DataHSetMacOSFileType(dataH, FOUR_CHAR_CODE('OggS'));

    err = DataHOpenForWrite(dataH);
    if (err)
        goto bail;

    if (globals->streamCount > 0) {
        err = ConfigureQuickTimeMovieExporter(globals);
        if (err)
            goto bail;

        err = ConfigureStdComponents(globals);

        if (!err)
            err = mux_streams(globals, dataH);
    }

 bail:
    if (dataH)
        CloseComponent(dataH);

    dbg_printf("[  OE] <   [%08lx] :: FromProceduresToDataRef() = %ld\n", (UInt32) globals, err);
    return err;
}

pascal ComponentResult OggExportAddDataSource(OggExportGlobalsPtr globals, OSType trackType, TimeScale scale,
                                              long *trackIDPtr, MovieExportGetPropertyUPP getPropertyProc,
                                              MovieExportGetDataUPP getDataProc, void *refCon)
{
    ComponentResult err = noErr;
    stream_format_handle_funcs *ff = NULL;
    StreamInfo *si = NULL;

    dbg_printf("[  OE]  >> [%08lx] :: AddDataSource('%4.4s')\n", (UInt32) globals, (char *) &trackType);

    if (!scale || !trackType || !getDataProc || !getPropertyProc)
        return paramErr;

    ff = find_stream_support(trackType, scale, getPropertyProc, refCon);

    if (ff != NULL) {
        err = OpenStream(globals, trackType, scale, trackIDPtr, getPropertyProc, getDataProc, refCon, ff, &si);
    }

    dbg_printf("[  OE] <   [%08lx] :: AddDataSource() = %ld\n", (UInt32) globals, err);
    return err;
}

pascal ComponentResult OggExportSetProgressProc(OggExportGlobalsPtr globals, MovieProgressUPP proc, long refcon)
{
    dbg_printf("[  OE]  >> [%08lx] :: SetProgressProc()\n", (UInt32) globals);

    globals->progressProc = proc;
    globals->progressRefcon = refcon;

    dbg_printf("[  OE] <   [%08lx] :: SetProgressProc()\n", (UInt32) globals);
    return noErr;
}


#if USE_NIB_FILE
static pascal OSStatus SettingsWindowEventHandler(EventHandlerCallRef inHandler, EventRef inEvent, void *inUserData)
{
#pragma unused (inHandler,inUserData)

    WindowRef window = NULL;
    HICommand command;
    OSStatus result = eventNotHandledErr;
    OggExportGlobalsPtr globals = (OggExportGlobalsPtr) inUserData;

    dbg_printf("[  OE]  >> [%08lx] :: SettingsWindowEventHandler()\n", (UInt32) globals);

    window = ActiveNonFloatingWindow();
    if (window == NULL)
        goto bail;

    GetEventParameter(inEvent, kEventParamDirectObject, typeHICommand, NULL, sizeof(HICommand), NULL, &command);

    dbg_printf("[  OE]   | [%08lx] :: SettingsWindowEventHandler('%4.4s')\n", (UInt32) globals, (char *) &command.commandID);

    switch (command.commandID) {
    case kHICommandOK:
        globals->canceled = false;

        QuitAppModalLoopForWindow(window);
        result = noErr;
        break;

    case kHICommandCancel:
        globals->canceled = true;
        QuitAppModalLoopForWindow(window);
        result = noErr;
        break;

    case 'OEca':
        {
            SCExtendedProcs xProcs;
            ComponentInstance stdAudio;

            result = OpenADefaultComponent(StandardCompressionType, StandardCompressionSubTypeAudio, &stdAudio);
            if (result == noErr) {
                memset(&xProcs, 0, sizeof(xProcs));
                strcpy((char*)xProcs.customName + 1, "Select Output Format");
                xProcs.customName[0] = (unsigned char) strlen((char*) xProcs.customName + 1);
                (void) QTSetComponentProperty(stdAudio, kQTPropertyClass_SCAudio,
                                              kQTSCAudioPropertyID_ExtendedProcs,
                                              sizeof(xProcs), &xProcs);

                result = SCRequestImageSettings(stdAudio);
                if (result == userCanceledErr)
                    result = noErr; // User cancelling is ok.
                CloseComponent(stdAudio);
            }
        }

        break;

    default:
        break;
    }

 bail:
    dbg_printf("[  OE] <   [%08lx] :: SettingsWindowEventHandler() = 0x%08lx\n", (UInt32) globals, result);
    return result;
}

pascal ComponentResult OggExportDoUserDialog(OggExportGlobalsPtr globals, Movie theMovie, Track onlyThisTrack,
                                             TimeValue startTime, TimeValue duration, Boolean *canceledPtr)
{
#pragma unused(theMovie, onlyThisTrack, startTime, duration)

    CFBundleRef bundle = NULL;
    IBNibRef    nibRef = NULL;
    WindowRef   window = NULL;
    Boolean     portChanged = false;

    CGrafPtr    savedPort;
    OSErr       err = resFNotFound;

    EventTypeSpec eventList[] = {{kEventClassCommand, kEventCommandProcess}};
    EventHandlerUPP settingsWindowEventHandlerUPP = NewEventHandlerUPP(SettingsWindowEventHandler);

    dbg_printf("[  OE]  >> [%08lx] :: DoUserDialog()\n", (UInt32) globals);

    bundle = CFBundleGetBundleWithIdentifier(CFSTR(kOggExportBundleID));
    if (bundle == NULL)
        goto bail;

    err = CreateNibReferenceWithCFBundle(bundle, CFSTR("OggExport"), &nibRef);
    if (err)
        goto bail;

    err = CreateWindowFromNib(nibRef, CFSTR("Settings"), &window);
    if (err)
        goto bail;

    portChanged = QDSwapPort(GetWindowPort(window), &savedPort);

    *canceledPtr = false;

    InstallWindowEventHandler(window, settingsWindowEventHandlerUPP, GetEventTypeCount(eventList), eventList, globals, NULL);

    ShowWindow(window);

    RunAppModalLoopForWindow(window);

    *canceledPtr = globals->canceled;

 bail:
    if (window) {
        if (portChanged) {
            QDSwapPort(savedPort, NULL);
        }
        DisposeWindow(window);
    }

    if (settingsWindowEventHandlerUPP)
        DisposeEventHandlerUPP(settingsWindowEventHandlerUPP);

    if (nibRef)
        DisposeNibReference(nibRef);

    dbg_printf("[  OE] <   [%08lx] :: DoUserDialog() = 0x%04x\n", (UInt32) globals, err);
    return err;
}

#else
#error "Non-NIB user dialog not implemented."

pascal ComponentResult OggExportDoUserDialog(OggExportGlobalsPtr globals, Movie theMovie, Track onlyThisTrack,
                                             TimeValue startTime, TimeValue duration, Boolean *canceledPtr)
{
    dbg_printf("[  OE]  >> [%08lx] :: DoUserDialog() [NN]\n", (UInt32) globals);
    dbg_printf("[  OE] <   [%08lx] :: DoUserDialog() [NN]\n", (UInt32) globals);
    return noErr;
}

#endif


pascal ComponentResult OggExportGetSettingsAsAtomContainer(OggExportGlobalsPtr globals, QTAtomContainer *settings)
{
    QTAtomContainer theSettings = NULL;
    OSErr err;

    dbg_printf("[  OE]  >> [%08lx] :: GetSettingsAsAtomContainer()\n", (UInt32) globals);

    if (!settings)
        return paramErr;

    err = QTNewAtomContainer(&theSettings);
    if (err)
        goto bail;

 bail:
    if (err && theSettings) {
        QTDisposeAtomContainer(theSettings);
        theSettings = NULL;
    }

    *settings = theSettings;

    dbg_printf("[  OE] <   [%08lx] :: GetSettingsAsAtomContainer() = %d\n", (UInt32) globals, err);
    return err;
}

pascal ComponentResult OggExportSetSettingsFromAtomContainer(OggExportGlobalsPtr globals, QTAtomContainer settings)
{
    OSErr err = noErr;

    dbg_printf("[  OE]  >> [%08lx] :: SetSettingsFromAtomContainer()\n", (UInt32) globals);

    if (!settings)
        return paramErr;

    dbg_printf("[  OE] <   [%08lx] :: SetSettingsFromAtomContainer() = %d\n", (UInt32) globals, err);
    return err;
}


pascal ComponentResult OggExportGetFileNameExtension(OggExportGlobalsPtr globals, OSType *extension) {
#pragma unused(globals)
    dbg_printf("[  OE]  >> [%08lx] :: GetFileNameExtension()\n", (UInt32) globals);
    *extension = 'ogg ';
    dbg_printf("[  OE] <   [%08lx] :: GetFileNameExtension()\n", (UInt32) globals);
    return noErr;
}

pascal ComponentResult OggExportGetShortFileTypeString(OggExportGlobalsPtr globals, Str255 typeString) {
#pragma unused(globals)
    dbg_printf("[  OE]  >> [%08lx] :: GetShortFileTypeString()\n", (UInt32) globals);

    // return GetComponentIndString((Component)globals->self, typeString, kOggExportShortFileTypeNamesResID, 1);
    typeString[0] = '\x04';
    typeString[1] = 'O';
    typeString[2] = 'g';
    typeString[3] = 'g';
    typeString[4] = 'S';
    typeString[5] = '\x0';

    dbg_printf("[  OE] <   [%08lx] :: GetShortFileTypeString()\n", (UInt32) globals);
    return noErr;
}

pascal ComponentResult OggExportGetSourceMediaType(OggExportGlobalsPtr globals, OSType *mediaType) {
#pragma unused(globals)
    dbg_printf("[  OE]  >> [%08lx] :: GetSourceMediaType()\n", (UInt32) globals);


    if (!mediaType)
        return paramErr;

    // any track type
    *mediaType = 0;

    dbg_printf("[  OE] <   [%08lx] :: GetSourceMediaType()\n", (UInt32) globals);
    return noErr;
}


/* ========================================================================= */

static OSErr ConfigureQuickTimeMovieExporter(OggExportGlobalsPtr globals)
{
    ComponentInstance  stdAudioCompression = NULL;
    QTAtomContainer    movieExporterSettings = NULL;
    OSErr              err;

    /* TODO: make this function do something */

    // Open the Standard Compression component
    err = OpenADefaultComponent(StandardCompressionType, StandardCompressionSubTypeAudio, &stdAudioCompression);
    if (err)
        goto bail;

    // Get the settings atom
    err = SCGetSettingsAsAtomContainer(stdAudioCompression, &movieExporterSettings);
    if (err)
        goto bail;

    // Set the compression settings for the QT Movie Exporter
    //err = MovieExportSetSettingsFromAtomContainer(globals->quickTimeMovieExporter, movieExporterSettings);

 bail:
    if (stdAudioCompression)
        CloseComponent(stdAudioCompression);

    if (movieExporterSettings)
        DisposeHandle(movieExporterSettings);

    return err;
}

static ComponentResult ConfigureStdComponents(OggExportGlobalsPtr globals)
{
    ComponentResult err = noErr;
    StreamInfoPtr si = NULL;
    int i = 0;

    for (i = 0; i < globals->streamCount; i++)
    {
        si = &(*globals->streamInfoHandle)[i];
        if (si->sfhf->configure != NULL)
            err = (*si->sfhf->configure)(globals, si);
        if (err)
            break;
    }

    return err;
}

static stream_format_handle_funcs* find_stream_support(OSType trackType, TimeScale scale,
                                                       MovieExportGetPropertyUPP getPropertyProc,
                                                       void *refCon)
{
    stream_format_handle_funcs *ff = &s_formats[0];
    int i = 0;

    while(ff->can_handle != NULL) {
        if ((*ff->can_handle)(trackType, scale, getPropertyProc, refCon))
            break;
        i += 1;
        ff = &s_formats[i];
    }

    if (ff->can_handle == NULL)
        return NULL;

    return ff;
}

static ComponentResult OpenStream(OggExportGlobalsPtr globals, OSType trackType, TimeScale scale,
                                  long *trackIDPtr, MovieExportGetPropertyUPP getPropertyProc,
                                  MovieExportGetDataUPP getDataProc, void *refCon,
                                  stream_format_handle_funcs *ff, StreamInfoPtr *out_si)
{
    ComponentResult err = noErr;

    if (globals->streamInfoHandle) {
        globals->streamCount++;
        SetHandleSize((Handle) globals->streamInfoHandle, sizeof(StreamInfo) * globals->streamCount);
    } else {
        globals->streamInfoHandle = (StreamInfo **) NewHandleClear(sizeof(StreamInfo));
        globals->streamCount = 1;
    }

    err = MemError();

    if (!err) {
        StreamInfo *si = NULL;
        HLock((Handle) globals->streamInfoHandle);
        si = &(*globals->streamInfoHandle)[globals->streamCount - 1];

        srandomdev();
        si->serialno = random();

        ogg_stream_init(&si->os, si->serialno);

        si->packets_total = 0;
        si->acc_packets = 0;
        si->last_grpos = 0;
        si->acc_duration = 0;

        si->trackID = globals->streamCount;
        si->getPropertyProc = getPropertyProc;
        si->getDataProc = getDataProc;
        si->refCon = refCon;
        si->sourceTimeScale = scale;

        memset(&si->gdp, 0, sizeof(MovieExportGetDataParams));
        si->gdp.recordSize = sizeof(MovieExportGetDataParams);
        si->gdp.trackID = si->trackID;
        si->gdp.sourceTimeScale = scale;

        si->out_buffer = NULL;
        si->out_buffer_size = 0;

        si->og_buffer_size = kOES_init_og_size;
        si->og_buffer = calloc(1, kOES_init_og_size);
        si->og_ready = false;

        si->og_grpos = 0;

        si->src_extract_complete = false;
        si->eos = false;

        err = MemError();

        if (!err) {
            //si->MDmapping = NULL;
            //si->UDmapping = NULL;

            si->sfhf = ff;

            if (ff->initialize != NULL)
                err = (*ff->initialize)(si);
        }

        HUnlock((Handle) globals->streamInfoHandle);

        if (err) {
            ogg_stream_clear(&si->os);

            if (si->og_buffer)
                free(si->og_buffer);

            if (si->out_buffer)
                free(si->out_buffer);

            globals->streamCount--;
            SetHandleSize((Handle) globals->streamInfoHandle, sizeof(StreamInfo) * globals->streamCount);
        } else {
            *out_si = si;
            *trackIDPtr = si->trackID;
        }
    }

    return err;
}

static void _close_stream(OggExportGlobalsPtr globals, StreamInfoPtr si)
{
    ogg_stream_clear(&si->os);

    if (si->og_buffer) {
        free(si->og_buffer);
        si->og_buffer = NULL;
        si->og_buffer_size = 0;
    }

    if (si->out_buffer) {
        free(si->out_buffer);
        si->out_buffer = NULL;
        si->out_buffer_size = 0;
    }

    if (si->sfhf->clear != NULL)
        (*si->sfhf->clear)(si);

    /*
    if (si->MDmapping != NULL)
        CFRelease(si->MDmapping);
    if (si->UDmapping != NULL)
        CFRelease(si->UDmapping);
    */
}

static void CloseAllStreams(OggExportGlobalsPtr globals)
{
    int i;

    for (i = 0; i < globals->streamCount; i++)
    {
        StreamInfoPtr si = &(*globals->streamInfoHandle)[i];
        _close_stream(globals, si);
    }
    globals->streamCount = 0;
    SetHandleSize((Handle) globals->streamInfoHandle, 0);
}

ComponentResult mux_streams(OggExportGlobalsPtr globals, DataHandler data_h)
{
    ComponentResult err = noErr;

    Boolean progressOpen = false;
    wide data_h_offset = {0, 0};
    TimeValue duration = 0;
    Float64 max_page_duration = 0.8; /* dynamically adapt to the
                                        bitrates of the streams? */
    StreamInfoPtr si = &(*globals->streamInfoHandle)[0];
    StreamInfoPtr f_si = NULL;
    UInt32 i;
    Boolean all_streams_done = false;

    dbg_printf("[  OE]  >> [%08lx] :: mux_streams()\n", (UInt32) globals);

    if (globals->progressProc) {
        TimeRecord durationTimeRec;

        // TODO: loop over all the data sources and find the max duration

        // Get the track duration if it is available
        if (InvokeMovieExportGetPropertyUPP(si->refCon, si->trackID,
                                            movieExportDuration,
                                            &durationTimeRec,
                                            si->getPropertyProc) == noErr) {
            ConvertTimeScale(&durationTimeRec, si->sourceTimeScale);
            duration = durationTimeRec.value.lo;
            dbg_printf("[  OE]  <> [%08lx] :: mux_streams() = %ld\n",
                       (UInt32) globals, duration);

            InvokeMovieProgressUPP(NULL, movieProgressOpen,
                                   progressOpExportMovie, 0,
                                   globals->progressRefcon,
                                   globals->progressProc);
            progressOpen = true;
        }
    }

    HLock((Handle) globals->streamInfoHandle);

    // ident headers first
    // TODO: sort streams by type
    for (i = 0; i < globals->streamCount; i++) {
        si = &(*globals->streamInfoHandle)[i];

        err = (*si->sfhf->write_i_header)(si, data_h, &data_h_offset);
        if (err)
            break;
    }

    // then, the rest of the header packets
    for (i = 0; i < globals->streamCount; i++) {
        si = &(*globals->streamInfoHandle)[i];

        err = (*si->sfhf->write_headers)(si, data_h, &data_h_offset);
        if (err)
            break;
    }


    while (!all_streams_done) {
        // 1. fill all the streams
        // 2. a) find the earliest page
        //    b) flush it to disk
        // 3. repeat

        UInt32 earliest_sec = 0xffffffff;
        Float64 earliest_subsec = 1.0;

        wide tmp;

        UInt32 p_sec = 0;
        Float64 p_subsec = 0.0;

        all_streams_done = true;
        f_si = NULL;

        for (i = 0; i < globals->streamCount; i++) {
            si = &(*globals->streamInfoHandle)[i];

            if (!si->og_ready && !si->eos) {
                err = (*si->sfhf->fill_page)(globals, si, max_page_duration,
                                             &p_sec, &p_subsec);
                if (err)
                    break;
            }
            if (all_streams_done && si->og_ready)
                all_streams_done = false;
            if (si->og_ready) {
                if (p_sec < earliest_sec ||
                    (p_sec == earliest_sec && p_subsec < earliest_subsec)) {
                    f_si = si;
                    earliest_sec = p_sec;
                    earliest_subsec = p_subsec;
                }
            }
        }

        if (!all_streams_done) {
            err = DataHWrite64(data_h, f_si->og_buffer, &data_h_offset,
                               f_si->og.header_len + f_si->og.body_len,
                               NULL, 0);
            dbg_printf("[  OE] vvv [%08lx] :: mux_streams() = %ld, %lld\n",
                       (UInt32) globals, err, *(SInt64 *) &data_h_offset);
            if (err)
                break;

            tmp.hi = 0;
            tmp.lo = f_si->og.header_len + f_si->og.body_len;
            WideAdd(&data_h_offset, &tmp);

            f_si->og_ready = false;

            /*
            if (!f_si->eos)
                err = (*f_si->sfhf->fill_page)(globals, f_si, max_page_duration,
                                               &p_sec, &p_subsec);
            if (err)
                break;
            */
        }

        if (progressOpen) {
            Fixed percentDone = 0x010000;
            if (f_si)
                percentDone = FixDiv(f_si->time, duration);

            if (percentDone > 0x010000)
                percentDone = 0x010000;

            err = InvokeMovieProgressUPP(NULL, movieProgressUpdatePercent,
                                         progressOpExportMovie, percentDone,
                                         globals->progressRefcon,
                                         globals->progressProc);
            if (err)
                break;
        }
    }

    HUnlock((Handle) globals->streamInfoHandle);

    if (progressOpen)
        InvokeMovieProgressUPP(NULL, movieProgressClose,
                               progressOpExportMovie, 0,
                               globals->progressRefcon,
                               globals->progressProc);

    dbg_printf("[  OE] <   [%08lx] :: mux_streams() = %ld\n", (UInt32) globals, err);
    return err;
}
