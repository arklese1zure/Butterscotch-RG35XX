#!/bin/sh
# GarlicOS launcher: copy this and the undertale/ folder to Roms/APPS.
# Menu + Start quits. Saves go to undertale/save, the log to undertale/log.txt.
cd "$(dirname "$0")/undertale" || exit 1
mkdir -p save
[ -f log.txt ] && mv log.txt log-prev.txt
date > log.txt
AM2R_TRIPLEBUF=1 RG35XX_FULL_RES=1 SWR_TEXTURE_BUDGET_MB=96 ./butterscotch --trace-frames --load-type map-file --lazy-textures --lazy-rooms --lazy-audio \
    --save-folder ./save game.unx >> log.txt 2>&1
echo "exit=$?" >> log.txt
date >> log.txt
sync
