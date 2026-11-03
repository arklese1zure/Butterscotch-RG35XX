#!/bin/sh
# GarlicOS launcher: copy this and the am2r/ folder to Roms/APPS.
# Menu + Start quits. Saves go to am2r/save, the log to am2r/log.txt.
cd "$(dirname "$0")/am2r" || exit 1
mkdir -p save
[ -f log.txt ] && mv log.txt log-prev.txt
date > log.txt
AM2R_TRIPLEBUF=1 AM2R_PROFILE_ROOM=rm_a0h04d ./butterscotch --trace-frames --load-type map-file --lazy-textures --lazy-rooms --lazy-audio \
    --save-folder ./save data.win >> log.txt 2>&1
echo "exit=$?" >> log.txt
date >> log.txt
sync
