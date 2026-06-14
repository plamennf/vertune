@echo off

if not exist build mkdir build
pushd build

cl /Oi /fp:fast /fp:except- /Zi /FC /nologo /W3 /I ..\external\include /std:c++20 /Zc:strictStrings- /EHsc- /O2 /Ob2 /MT /D_CRT_SECURE_NO_WARNINGS /DNDEBUG /DBUILD_RELEASE /DUSE_PACKAGE /DSTB_IMAGE_IMPLEMENTATION /DPACKAGER_STANDALONE  /Fe:"packager" ..\src\general.cpp ..\src\packager\packager.cpp /link /opt:ref /incremental:no /LIBPATH:"..\external\lib" /subsystem:console SDL2.lib SDL2main.lib shell32.lib Advapi32.lib

popd

build\packager.exe
del build\packager.*

em++ -std=c++20 -O2 -DUSE_PACKAGE -DNEBUG -Wno-return-type -Wno-unused-value -Wno-switch -Wno-writable-strings -Iexternal/include src/audio.cpp src/camera.cpp src/entity.cpp src/font.cpp src/general.cpp src/main.cpp src/main_menu.cpp src/memory_arena.cpp src/mt19937-64.cpp src/particles.cpp src/rendering.cpp src/rendering_opengl.cpp src/resource_manager.cpp src/text_file_handler.cpp src/tilemap.cpp src/world.cpp src/packager/packager.cpp -s USE_SDL=2 -s USE_FREETYPE=1 -s USE_WEBGL2=1 -s MIN_WEBGL_VERSION=1 -s MAX_WEBGL_VERSION=2 -s FULL_ES3=1 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s GL_DEBUG=1 -s FORCE_FILESYSTEM=1 --preload-file assets.pak@/assets.pak -o build/index.html --shell-file shell.html

copy assets.pak build
