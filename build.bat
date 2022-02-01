@echo off
REM		Build Script

REM Set Compiler Settings Here

cls

set CPP=c++
set GPP=g++
set WINDRES=windres
set OUTPUT=wol.exe
set DEBUGMODE=1

set LINK_ONLY=0
set VERBOSE=0

set ASYNC_BUILD=1

set COMPILER_FLAGS=-std=c++20
set ADDITIONAL_LIBRARIES=-static-libstdc++ -static -lws2_32 -liphlpapi
set ADDITIONAL_LIBDIRS=
set ADDITIONAL_INCLUDEDIRS=

set ICO_RESOURCE=icon.rc
set RES_FILE=.objs64\icon.res.o

del %OUTPUT% 2>nul

setlocal enabledelayedexpansion


if %LINK_ONLY% GTR 0 (
	goto linker
)

if %DEBUGMODE% GTR 0 (
	set DEBUG_INFO=-ggdb -g
) else (
	set DEBUG_INFO=-s
)

if %ASYNC_BUILD% GTR 0 (
	set WAIT=
) else (
	set WAIT=/WAIT
)

del /S /Q ".objs64\*" 2>nul

if not exist .objs64 (
	echo Creating Object Directory Structure...
	mkdir .objs64
)

echo Building Files...
for %%F in (*.cpp) do (
	if not exist .objs64\%%~nF.o (
		echo Building %%~nF.o
		start /B %WAIT% "%%~nF.o" %CPP% %ADDITIONAL_INCLUDEDIRS% %COMPILER_FLAGS% %DEBUG_INFO% -c %%F -o .objs64\%%~nF.o

		if %VERBOSE% GTR 0 (
			echo %CPP% %ADDITIONAL_INCLUDEDIRS% %COMPILER_FLAGS% %DEBUG_INFO% -c %%F -o .objs64\%%~nF.o
		)
	)
)

echo Making Icon...
%WINDRES% %ICO_RESOURCE% -J rc -O coff -o %RES_FILE%

REM Wait for building process to finish
:loop
for /f %%G in ('tasklist ^| find /c "%CPP%"') do ( set count=%%G )
if %count%==0 (
	goto linker
) else (
	timeout /t 2 /nobreak>nul
	goto loop
)

:linker

set "files="
for /f "delims=" %%A in ('dir /b /a-d ".objs64\*.o" ') do set "files=!files! .objs64\%%A"

:link
echo Linking Executable...

if %DEBUGMODE% GTR 0 (
	set MWINDOWS=
) else (
	set MWINDOWS=-mwindows
)

if %VERBOSE% GTR 0 (
	echo %GPP% %ADDITIONAL_LIBDIRS% -o %OUTPUT% %files% %ADDITIONAL_LIBRARIES% %MWINDOWS%
)

%GPP% %ADDITIONAL_LIBDIRS% -o %OUTPUT% %files% %ADDITIONAL_LIBRARIES% %MWINDOWS%

:finish
if exist .\%OUTPUT% (
	echo Build Success!
) else (
	echo Build Failed!
)