#!/bin/sh
# MiaUI launcher: put this folder in /userdata/Ports with AM2R 1.1's files (data.win, the .ogg files) and
# the MiaUI build of butterscotch. Menu + Start quits. Saves go to save/, the log to log.txt.
GAMEDIR="$(cd "$(dirname "$0")" >/dev/null 2>&1 && pwd)"
cd "$GAMEDIR" || exit 1
mkdir -p save
# First run: Fusion-style aiming and missiles. The game fills in the rest of its settings.
[ -f save/config.ini ] || printf '[Control]\nAimStyle="0"\nMissileStyle="0"\n' > save/config.ini
[ -f log.txt ] && mv log.txt log-prev.txt
date > log.txt
## CPU: MinUI's game speed, the same as MiaUI's emulators and ports
overclock.elf $CPU_SPEED_GAME
## vsync off: steady 60 fps pacing (with it on, about 1 frame in 50 waits an extra refresh); 1 turns it on
export MIAUI_VSYNC=0
AM2R_PROFILE_ROOM=rm_a0h04d ./butterscotch --trace-frames --load-type map-file --lazy-textures --lazy-rooms --lazy-audio \
    --save-folder ./save data.win >> log.txt 2>&1
echo "exit=$?" >> log.txt
date >> log.txt
sync
