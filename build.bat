@echo off

REM Check if a command line argument was passed
if "%~1"=="" (
    REM No argument, keep the default value
    set BuildDebug=0
) else (
    REM Use the value passed as argument
    set BuildDebug=%~1
)

if not exist build mkdir build
pushd build

rc.exe /fo resources.res ..\resource.rc

set CompilerFlags= /Oi /fp:fast /fp:except- /Zi /FC /nologo /W3 /I ..\external\include /std:c++20 /Zc:strictStrings- /EHsc- 
set Defines= /D_CRT_SECURE_NO_WARNINGS /DRENDER_OPENGL /DOS_WINDOWS /DCOMPILER_MSVC /DUNICODE /D_UNICODE
set LinkerFlags= /opt:ref /incremental:no /LIBPATH:"..\external\lib"
set Libs= SDL2.lib SDL2main.lib glew32.lib opengl32.lib freetype.lib shell32.lib

if %BuildDebug%==1 set CompilerFlags= /Od /Ob0 /MTd %CompilerFlags%
if %BuildDebug%==0 set CompilerFlags= /O2 /Ob2 /MT %CompilerFlags%

if %BuildDebug%==1 set Defines= /D_DEBUG /DBUILD_DEBUG /DPFNGE_DEBUG %Defines%
if %BuildDebug%==0 set Defines= /DNDEBUG /DBUILD_RELEASE /DUSE_PACKAGE %Defines%

if %BuildDebug%==1 set LinkerFlags= /LIBPATH:"..\external\lib\Debug" /subsystem:console %LinkerFlags%
if %BuildDebug%==0 set LinkerFlags= /LIBPATH:"..\external\lib\Release" /subsystem:windows %LinkerFlags%

cl %CompilerFlags% %Defines%  /Fe:"vertune" ..\src\*.cpp ..\src\packager\packager.cpp /link %LinkerFlags% %Libs% resources.res

xcopy /y /d ..\external\lib\*.dll
if %BuildDebug%==1 xcopy /y /d ..\external\lib\Debug\*.dll
if %BuildDebug%==0 xcopy /y /d ..\external\lib\Release\*.dll

del *.obj

popd
