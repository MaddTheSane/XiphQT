@echo off
echo ---+++--- Building XiphQT ---+++---

set BTARGET=build

if NOT z%1==z (set BTARGET=%1)

set OLDPATH=%PATH%
set OLDINCLUDE=%INCLUDE%
set OLDLIB=%LIB%

call "c:\program files\microsoft visual studio\vc98\bin\vcvars32.bat"
echo Setting include/lib paths for XiphQT
set PATH=%PATH%;"C:\Program Files\QuickTime SDK\Tools"
set INCLUDE=%INCLUDE%;"C:\Program Files\QuickTime SDK\CIncludes";"C:\Program Files\QuickTime SDK\ComponentIncludes"
set LIB=%LIB%;"C:\Program Files\QuickTime SDK\Libraries"
echo Compiling...
msdev XiphQT.dsp /useenv /make "XiphQT - Win32 Debug" /%BTARGET%

set PATH=%OLDPATH%
set INCLUDE=%OLDINCLUDE%
set LIB=%OLDLIB%
