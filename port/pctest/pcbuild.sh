#!/bin/sh
# Copies am2r-port to am2r-pcgl and builds the MiaUI backend for the PC (GLES via Mesa).
W=/home/samuel/Projects/rg35xx-toolchain/workspace
rsync -a --exclude build --exclude compat/config.mk --exclude compat/tmp/cc --exclude 'build-*.log' $W/am2r-port/ $W/am2r-pcgl/
cd $W/am2r-pcgl
make -j$(nproc) CC=gcc BACKEND=miaui AUDIO_BACKEND=miniaudio PKG_CONFIG=true MIAUI_CFLAGS="$(sdl2-config --cflags)" \
    MIAUI_LIBS="$(sdl2-config --libs) -lGLESv2" DISABLE_LEGACY_GL=1 DISABLE_WAD17=1 DISABLE_PHYSICS=1 DISABLE_VM_TRACING=1 \
    DISABLE_VM_GML_PROFILER=1 DISABLE_VM_OPCODE_PROFILER=1 CFLAGS="-O2 -g" > build-pc.log 2>&1
status=$?
echo "build=$status"; grep -E ' error|warning: impl' build-pc.log | head
exit $status
