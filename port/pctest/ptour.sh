#!/bin/sh
# ptour.sh <renderer> <outdir>: loads the user's save, then tours every room (60 frames each, from frame 1150),
# capturing 50 frames into each room.
W=/home/samuel/Projects/rg35xx-toolchain/workspace
OUT=$2; rm -rf "$OUT"; mkdir -p "$OUT/save"; cp ${AM2R_SAVE:-/mnt/0542ce6a-d862-491a-a4ff-2f7b9e863fde/Backups/RG35XX/Roms/APPS/am2r/save}/* "$OUT/save/"
FRAMES=""; SHOTS=""; k=0
while [ $k -lt 400 ]; do f=$((1150 + k*60 + 50)); FRAMES="$FRAMES,$f"; SHOTS="$SHOTS --screenshot-at-frame $f"; k=$((k+1)); done
KEYS="700:enter,780:enter,940:z,1000:z,1120:tab"
cd $W/AM2R_11
export AM2R_ROOM_TOUR=60 AM2R_GOTO_FRAME=1150 AM2R_KEYS="$KEYS" SDL_VIDEODRIVER=offscreen
if [ "$1" = software ]; then
    rm -f /tmp/swframe-*.ppm
    MIAUI_DUMP_FRAMES="${FRAMES#,}" $W/am2r-pcgl/build/butterscotch --renderer software --seed 1 --fast-forward-speed 20 \
        --load-type map-file --lazy-textures --lazy-rooms --lazy-audio --save-folder "$OUT/save" data.win > "$OUT/log.txt" 2>&1
    echo "exit=$?" >> "$OUT/log.txt"; mv /tmp/swframe-*.ppm "$OUT/"
else
    MIAUI_FORCE_ES2=1 $W/am2r-pcgl/build/butterscotch --renderer modern-gl --seed 1 --fast-forward-speed 20 \
        --load-type map-file --lazy-textures --lazy-rooms --lazy-audio --save-folder "$OUT/save" \
        --screenshot "$OUT/shot-%d.png" $SHOTS data.win > "$OUT/log.txt" 2>&1
    echo "exit=$?" >> "$OUT/log.txt"
fi
tail -1 "$OUT/log.txt"
