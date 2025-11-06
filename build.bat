@echo off

set BuildDebug=0

if not exist build mkdir build
pushd build

set CompilerFlags= /Oi /fp:fast /fp:except- /Zi /FC /nologo /W3 /I ..\external\include /std:c++20 /Zc:strictStrings- /EHsc- 
set Defines= /D_CRT_SECURE_NO_WARNINGS /DRENDER_OPENGL /DOS_WINDOWS /DCOMPILER_MSVC /DUNICODE /D_UNICODE /DGLFW_INCLUDE_NONE
set LinkerFlags= /opt:ref /incremental:no /subsystem:console /LIBPATH:"..\external\lib"
set Libs= SDL2.lib SDL2main.lib glew32.lib opengl32.lib freetype.lib shell32.lib

if %BuildDebug%==1 set CompilerFlags= /Od /Ob0 /MTd %CompilerFlags%
if %BuildDebug%==0 set CompilerFlags= /O2 /Ob2 /MT %CompilerFlags%

if %BuildDebug%==1 set Defines= /D_DEBUG /DBUILD_DEBUG /DPFNGE_DEBUG %Defines%
if %BuildDebug%==0 set Defines= /DNDEBUG /DBUILD_RELEASE /DUSE_PACKAGE %Defines%

if %BuildDebug%==1 set LinkerFlags= /LIBPATH:"..\external\lib\Debug" %LinkerFlags%
if %BuildDebug%==0 set LinkerFlags= /LIBPATH:"..\external\lib\Release" %LinkerFlags%

REM if %BuildDebug%==1 set Libs= soloud_static_x64_d.lib %Libs%
REM if %BuildDebug%==0 set Libs= soloud_static_x64.lib %Libs%

cl %CompilerFlags% %Defines%  /Fe:"platformer" ..\src\*.cpp ..\src\packager\packager.cpp ..\external\src\fswatcher.cpp /link %LinkerFlags% %Libs%

xcopy /y /d ..\external\lib\*.dll
if %BuildDebug%==1 xcopy /y /d ..\external\lib\Debug\*.dll
if %BuildDebug%==0 xcopy /y /d ..\external\lib\Release\*.dll

del *.obj

popd
