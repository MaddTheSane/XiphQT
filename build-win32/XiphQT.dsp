# Microsoft Developer Studio Project File - Name="XiphQT" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=XiphQT - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "XiphQT.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "XiphQT.mak" CFG="XiphQT - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "XiphQT - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "XiphQT - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "XiphQT - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "XIPHQT_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "C:\Program Files\QuickTime SDK\CIncludes" /I "..\AppleSDK\CoreAudio\PublicUtility" /I "..\AppleSDK\CoreAudio\AudioCodecs\ACPublic" /I "..\..\ogg\include" /I "..\..\ogg\include\ogg" /I "..\..\vorbis\include" /I "..\..\speex\include" /I "..\common" /I "..\utils" /I "." /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "XIPHQT_EXPORTS" /D inline=__inline /D "QT_WIN32__VBR_BROKEN" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib qtmlClient.lib libspeex.lib ogg_static.lib vorbis_static.lib /nologo /dll /machine:I386 /nodefaultlib:"libcmt.lib" /nodefaultlib:"libcd.lib" /def:".\XiphQT.def" /libpath:"C:\Program Files\QuickTime SDK\Libraries" /libpath:"..\..\speex\win32\libspeex\Release" /libpath:"..\..\ogg\win32\Static_Release" /libpath:"..\..\vorbis\win32\Vorbis_Static_Release"
# SUBTRACT LINK32 /pdb:none
# Begin Special Build Tool
IntDir=.\Release
TargetName=XiphQT
SOURCE="$(InputPath)"
PostBuild_Desc=qtx-ing
PostBuild_Cmds=if exist "$(IntDir)\$(TargetName).qtx" Attrib -R "$(IntDir)\$(TargetName).qtx"	RezWack -f -d "$(IntDir)\$(TargetName).dll" -r "$(IntDir)\$(TargetName).qtr" -o "$(IntDir)\$(TargetName).qtx"	copy /y "$(IntDir)\$(TargetName).qtx" "c:\Program Files\QuickTime\QTComponents\"
# End Special Build Tool

!ELSEIF  "$(CFG)" == "XiphQT - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "XIPHQT_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "C:\Program Files\QuickTime SDK\CIncludes" /I "..\AppleSDK\CoreAudio\PublicUtility" /I "..\AppleSDK\CoreAudio\AudioCodecs\ACPublic" /I "..\..\ogg\include" /I "..\..\ogg\include\ogg" /I "..\..\vorbis\include" /I "..\..\speex\include" /I "..\common" /I "..\utils" /I "." /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "XIPHQT_EXPORTS" /D inline=__inline /D "QT_WIN32__VBR_BROKEN" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib qtmlClient.lib libspeex.lib ogg_static_d.lib vorbis_static_d.lib /nologo /dll /debug /machine:I386 /nodefaultlib:"libcmt.lib" /nodefaultlib:"libcd.lib" /def:".\XiphQT.def" /pdbtype:sept /libpath:"C:\Program Files\QuickTime SDK\Libraries" /libpath:"..\..\speex\win32\libspeex\Debug" /libpath:"..\..\ogg\win32\Static_Debug" /libpath:"..\..\vorbis\win32\Vorbis_Static_Debug"
# SUBTRACT LINK32 /pdb:none
# Begin Special Build Tool
IntDir=.\Debug
TargetName=XiphQT
SOURCE="$(InputPath)"
PostBuild_Desc=qtx-ing
PostBuild_Cmds=if exist "$(IntDir)\$(TargetName).qtx" Attrib -R "$(IntDir)\$(TargetName).qtx"	RezWack -f -d "$(IntDir)\$(TargetName).dll" -r "$(IntDir)\$(TargetName).qtr" -o "$(IntDir)\$(TargetName).qtx"	copy /y "$(IntDir)\$(TargetName).qtx" "c:\Program Files\QuickTime\QTComponents\"
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "XiphQT - Win32 Release"
# Name "XiphQT - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "OggImport"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\OggImport\src\common.c
# End Source File
# Begin Source File

SOURCE=..\OggImport\src\common.h
# End Source File
# Begin Source File

SOURCE=..\OggImport\src\importer_types.h
# End Source File
# Begin Source File

SOURCE=..\OggImport\src\OggImport.c
# End Source File
# Begin Source File

SOURCE=..\OggImport\src\OggImport.h
# End Source File
# Begin Source File

SOURCE=..\OggImport\src\OggImportDispatch.h
# End Source File
# Begin Source File

SOURCE=..\OggImport\src\rb.c
# End Source File
# Begin Source File

SOURCE=..\OggImport\src\rb.h
# End Source File
# Begin Source File

SOURCE=..\OggImport\src\stream_speex.c
# End Source File
# Begin Source File

SOURCE=..\OggImport\src\stream_speex.h
# End Source File
# Begin Source File

SOURCE=..\OggImport\src\stream_types_speex.h
# End Source File
# Begin Source File

SOURCE=..\OggImport\src\stream_types_vorbis.h
# End Source File
# Begin Source File

SOURCE=..\OggImport\src\stream_vorbis.c
# End Source File
# Begin Source File

SOURCE=..\OggImport\src\stream_vorbis.h
# End Source File
# Begin Source File

SOURCE=..\OggImport\src\versions.h
# End Source File
# End Group
# Begin Group "CAVorbis"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\CAVorbis\src\CAOggVorbisDecoder.cpp
# End Source File
# Begin Source File

SOURCE=..\CAVorbis\src\CAOggVorbisDecoder.h
# End Source File
# Begin Source File

SOURCE=..\CAVorbis\src\CAVorbisDecoder.cpp
# End Source File
# Begin Source File

SOURCE=..\CAVorbis\src\CAVorbisDecoder.h
# End Source File
# Begin Source File

SOURCE=..\CAVorbis\src\vorbis_entrypoints.cpp
# End Source File
# Begin Source File

SOURCE=..\CAVorbis\src\vorbis_versions.h
# End Source File
# End Group
# Begin Group "CASpeex"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\CASpeex\src\CAOggSpeexDecoder.cpp
# End Source File
# Begin Source File

SOURCE=..\CASpeex\src\CAOggSpeexDecoder.h
# End Source File
# Begin Source File

SOURCE=..\CASpeex\src\CASpeexDecoder.cpp
# End Source File
# Begin Source File

SOURCE=..\CASpeex\src\CASpeexDecoder.h
# End Source File
# Begin Source File

SOURCE=..\CASpeex\src\speex_entrypoints.cpp
# End Source File
# Begin Source File

SOURCE=..\CASpeex\src\speex_versions.h
# End Source File
# End Group
# Begin Group "common"

# PROP Default_Filter ""
# Begin Group "AppleSDK"

# PROP Default_Filter ""
# Begin Group "CoreAudio"

# PROP Default_Filter ""
# Begin Group "AudioCodecs"

# PROP Default_Filter ""
# Begin Group "ACPublic"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\AppleSDK\CoreAudio\AudioCodecs\ACPublic\ACBaseCodec.cpp
# End Source File
# Begin Source File

SOURCE=..\AppleSDK\CoreAudio\AudioCodecs\ACPublic\ACBaseCodec.h
# End Source File
# Begin Source File

SOURCE=..\AppleSDK\CoreAudio\AudioCodecs\ACPublic\ACCodec.cpp
# End Source File
# Begin Source File

SOURCE=..\AppleSDK\CoreAudio\AudioCodecs\ACPublic\ACCodec.h
# End Source File
# Begin Source File

SOURCE=..\AppleSDK\CoreAudio\AudioCodecs\ACPublic\ACCodecDispatch.h
# End Source File
# Begin Source File

SOURCE=..\AppleSDK\CoreAudio\AudioCodecs\ACPublic\ACCodecDispatchTypes.h
# End Source File
# Begin Source File

SOURCE=..\AppleSDK\CoreAudio\AudioCodecs\ACPublic\ACConditionalMacros.h
# End Source File
# End Group
# End Group
# Begin Group "PublicUtility"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\AppleSDK\CoreAudio\PublicUtility\CAConditionalMacros.h
# End Source File
# Begin Source File

SOURCE=..\AppleSDK\CoreAudio\PublicUtility\CADebugMacros.cpp
# End Source File
# Begin Source File

SOURCE=..\AppleSDK\CoreAudio\PublicUtility\CADebugMacros.h
# End Source File
# Begin Source File

SOURCE=..\AppleSDK\CoreAudio\PublicUtility\CAMath.h
# End Source File
# Begin Source File

SOURCE=..\AppleSDK\CoreAudio\PublicUtility\CAStreamBasicDescription.cpp
# End Source File
# Begin Source File

SOURCE=..\AppleSDK\CoreAudio\PublicUtility\CAStreamBasicDescription.h
# End Source File
# End Group
# End Group
# End Group
# Begin Source File

SOURCE=.\AudioCodec.h
# End Source File
# Begin Source File

SOURCE=..\common\config.h
# End Source File
# Begin Source File

SOURCE=..\common\data_types.h
# End Source File
# Begin Source File

SOURCE=..\utils\debug.h
# End Source File
# Begin Source File

SOURCE=.\DllMain.c
# End Source File
# Begin Source File

SOURCE=..\common\fccs.h
# End Source File
# Begin Source File

SOURCE=.\pxml.c
# End Source File
# Begin Source File

SOURCE=.\pxml.h
# End Source File
# Begin Source File

SOURCE=..\utils\ringbuffer.cpp
# End Source File
# Begin Source File

SOURCE=..\utils\ringbuffer.h
# End Source File
# Begin Source File

SOURCE=..\utils\wrap_ogg.cpp
# End Source File
# Begin Source File

SOURCE=..\utils\wrap_ogg.h
# End Source File
# Begin Source File

SOURCE=..\common\XCACodec.cpp
# End Source File
# Begin Source File

SOURCE=..\common\XCACodec.h
# End Source File
# End Group
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=..\CASpeex\src\CASpeexDecoderPublic.r
# End Source File
# Begin Source File

SOURCE=..\CAVorbis\src\CAVorbisDecoderPublic.r
# End Source File
# Begin Source File

SOURCE=..\OggImport\src\OggImport.r
# End Source File
# Begin Source File

SOURCE=.\resources.r

!IF  "$(CFG)" == "XiphQT - Win32 Release"

# Begin Custom Build
TargetDir=.\Release
TargetName=XiphQT
InputPath=.\resources.r

"$(TargetDir)\$(TargetName).qtr" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	Rez.exe -p -i "C:\Program Files\QuickTime SDK\RIncludes" -i ..\OggImport\src -i ..\CAVorbis\src -i ..\CASpeex\src -i ..\common -i ..\utils -i ..\resources -o "$(TargetDir)\$(TargetName).qtr" <  "$(InputPath)"

# End Custom Build

!ELSEIF  "$(CFG)" == "XiphQT - Win32 Debug"

# Begin Custom Build
TargetDir=.\Debug
TargetName=XiphQT
InputPath=.\resources.r

"$(TargetDir)\$(TargetName).qtr" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	Rez.exe -p -i "C:\Program Files\QuickTime SDK\RIncludes" -i ..\OggImport\src -i ..\CAVorbis\src -i ..\CASpeex\src -i ..\common -i ..\utils -i ..\resources -o "$(TargetDir)\$(TargetName).qtr" <  "$(InputPath)"

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\resources\XCAResources.r
# End Source File
# End Group
# End Target
# End Project
