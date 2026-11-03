#!/bin/sh
# GarlicOS launcher: test run with no input needed. Checks the crash reporter (crashtest.log),
# then jumps to the first Metroid room and triggers its earthquake (bench.log). Exits by itself.
cd "$(dirname "$0")/am2r" || exit 1
mkdir -p save
AM2R_CRASH_TEST=1 ./butterscotch --headless --save-folder ./save data.win > crashtest.log 2>&1
echo "exit=$?" >> crashtest.log
date > bench.log
AM2R_TRIPLEBUF=1 AM2R_GOTO_ROOM=rm_a0h05 AM2R_SPAWN=oBigQuake ./butterscotch --trace-frames \
    --load-type map-file --lazy-textures --lazy-rooms --lazy-audio \
    --save-folder ./save --exit-at-frame 2400 data.win >> bench.log 2>&1
echo "exit=$?" >> bench.log
date >> bench.log
sync
