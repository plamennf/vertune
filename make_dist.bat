@echo off
REM make_dist.bat - Windows distribution build for Vertune

REM Clean previous build
if exist dist rmdir /s /q dist
if not exist dist mkdir dist

call build.bat 0

REM Copy executable to dist
copy build\vertune.exe dist\

REM Optional: copy other assets (textures, fonts, etc.)
copy assets.pak dist\
xcopy /y /d external\lib\*.dll dist\

echo Distribution build complete. Check the "dist" folder.
pause
