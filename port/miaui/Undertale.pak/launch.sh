#!/bin/sh
# MiaUI launcher: put this folder in /userdata/Ports with Undertale's Steam Linux files (game.unx, the .ogg files)
# and the MiaUI build of butterscotch. Menu + Start quits. Saves go to save/, the log to log.txt.
GAMEDIR="$(cd "$(dirname "$0")" >/dev/null 2>&1 && pwd)"
cd "$GAMEDIR" || exit 1
mkdir -p save
[ -f log.txt ] && mv log.txt log-prev.txt
date > log.txt
## CPU: MinUI's game speed, the same as MiaUI's emulators and ports
overclock.elf $CPU_SPEED_GAME
## vsync off: steady 60 fps pacing (with it on, about 1 frame in 50 waits an extra refresh); 1 turns it on
export MIAUI_VSYNC=0
RG35XX_FULL_RES=1 SWR_TEXTURE_BUDGET_MB=96 ./butterscotch --trace-frames --load-type map-file --lazy-textures --lazy-rooms --lazy-audio \
    --save-folder ./save game.unx >> log.txt 2>&1
echo "exit=$?" >> log.txt
date >> log.txt
sync
