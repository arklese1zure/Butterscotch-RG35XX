#!/bin/sh
# Builds and tests inside the rootless toolchain container, with no network.
# Only workspace/ is mounted.
#   build.sh arm          RG35XX build in am2r-port/build
#   build.sh arm-clean    the same, from scratch
#   build.sh asan [room] [object]  PC build with AddressSanitizer in workspace/am2r-asan, then a
#                         headless run in [room] (default rm_a0h01), creating [object] once
#                         there if given, logged to am2r-logs/
set -eu

WORKSPACE=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
MODE=${1:-}
ROOM=${2:-rm_a0h01}
SPAWN=${3:-}

run() {
    podman run --rm --network=none -v "$WORKSPACE":/root/workspace \
        -e MODE="$MODE" -e ROOM="$ROOM" -e SPAWN="$SPAWN" rg35xx-toolchain /bin/bash -c "$1"
}

case "$MODE" in
arm|arm-clean)
    run '
        set -e
        export PATH="$PATH:/opt/miyoo/bin"
        CC=/opt/miyoo/bin/arm-miyoo-linux-uclibcgnueabi-gcc
        cd /root/workspace/am2r-port
        [ "$MODE" = arm-clean ] && make clean PKG_CONFIG=true
        set -o pipefail
        make -j"$(nproc)" CC="$CC" BACKEND=sdl1 AUDIO_BACKEND=miniaudio PKG_CONFIG=true \
            SDL1_CFLAGS="$(/opt/miyoo/bin/sdl-config --cflags)" SDL1_LIBS="$(/opt/miyoo/bin/sdl-config --libs)" \
            DISABLE_LEGACY_GL=1 DISABLE_MODERN_GL=1 DISABLE_WAD16=1 DISABLE_WAD17=1 DISABLE_PHYSICS=1 \
            DISABLE_VM_TRACING=1 DISABLE_VM_GML_PROFILER=1 DISABLE_VM_OPCODE_PROFILER=1 \
            CFLAGS="-O3 -g -DNDEBUG -DTARGET_RG35XX -marm -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=softfp" \
            2>&1 | tee build-arm.log | grep -v "linker input file unused" | tail -5
        ls -l build/butterscotch
    '
    ;;
asan)
    run '
        set -e
        W=/root/workspace; L=$W/am2r-logs
        mkdir -p "$L" "$W/am2r-asan"
        rsync -a --exclude build --exclude compat/config.mk --exclude compat/tmp/cc --exclude build-arm.log "$W/am2r-port/" "$W/am2r-asan/"
        cd "$W/am2r-asan"
        set -o pipefail
        make -j"$(nproc)" CC=gcc BACKEND=noop AUDIO_BACKEND=miniaudio PKG_CONFIG=true DISABLE_PHYSICS=1 \
            CFLAGS="-O1 -g -fsanitize=address -fno-omit-frame-pointer" LDFLAGS="-fsanitize=address" \
            2>&1 | tee "$L/build-asan.log" | grep -v "linker input file unused" | tail -3
        set +o pipefail +e
        rm -rf /tmp/am2r /tmp/save; cp -r "$W/AM2R_11" /tmp/am2r; mkdir -p /tmp/save; cp -r "$W/am2r-port/port/test-save/." /tmp/save/ 2>/dev/null || true
        cd /tmp/am2r
        [ -n "$SPAWN" ] && export AM2R_SPAWN="$SPAWN"
        [ "$SPAWN" = shake ] && unset AM2R_SPAWN && export AM2R_TEST_SHAKE=1
        ASAN_OPTIONS=detect_leaks=0 AM2R_HEADLESS_AUDIO=1 AM2R_GOTO_ROOM="$ROOM" "$W/am2r-asan/build/butterscotch" --headless \
            --load-type map-file --lazy-textures --lazy-rooms --lazy-audio --save-folder /tmp/save \
            --exit-at-frame 3000 data.win > "$L/asan-$ROOM.txt" 2>&1
        echo "exit=$?" | tee -a "$L/asan-$ROOM.txt"
    '
    ;;
*)
    sed -n 2,8p "$0"
    exit 1
    ;;
esac
