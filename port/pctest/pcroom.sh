#!/bin/sh
# pcroom.sh <room> <outdir> [extra keys]: loads the user's save, jumps to <room> at frame 1150, captures 1200..1500
# with both renderers (gl-N.png, sw-N.png).
W=/home/samuel/Projects/rg35xx-toolchain/workspace
SP=${PCTEST_OUT:-/tmp/pctest}
OUT=$2; rm -rf "$OUT"; mkdir -p "$OUT"
FR="1160 1170 1180 1200 1250 1300 1400 1500"
KEYS="700:enter,780:enter,940:z,1000:z${3:+,$3}"
for r in modern-gl software; do
    rm -rf "$OUT/save"; mkdir -p "$OUT/save"; cp ${AM2R_SAVE:-/mnt/0542ce6a-d862-491a-a4ff-2f7b9e863fde/Backups/RG35XX/Roms/APPS/am2r/save}/* "$OUT/save/"
    SHOTS=""; for f in $FR; do SHOTS="$SHOTS --screenshot-at-frame $f"; done
    DUMP=$(echo $FR | tr ' ' ',')
    rm -f /tmp/swframe-*.ppm
    cd $W/AM2R_11
    MIAUI_FORCE_ES2=1 MIAUI_DUMP_FRAMES=$DUMP AM2R_GOTO_ROOM=$1 AM2R_GOTO_FRAME=1150 AM2R_KEYS="$KEYS" SDL_VIDEODRIVER=offscreen timeout 300 \
        $W/am2r-pcgl/build/butterscotch --renderer $r --seed 1 --load-type map-file --lazy-textures --lazy-rooms --lazy-audio \
        --save-folder "$OUT/save" --exit-at-frame 1510 --screenshot "$OUT/gl-%d.png" $SHOTS data.win > "$OUT/log-$r.txt" 2>&1
    echo "$r exit=$?"
    for f in $FR; do [ -f /tmp/swframe-$f.ppm ] && python3 -c "from PIL import Image; Image.open('/tmp/swframe-$f.ppm').save('$OUT/sw-$f.png')"; done
    rm -f /tmp/swframe-*.ppm
done
cd "$OUT"; python3 -c "
from PIL import Image
fr='$FR'.split(); W,H=320,240; sheet=Image.new('RGB',(W*2,H*len(fr)))
for i,f in enumerate(fr):
    for j,p in enumerate(('gl','sw')):
        try: sheet.paste(Image.open(f'{p}-{f}.png').convert('RGB'),(j*W,i*H))
        except Exception as e: pass
sheet.save('sheet.png')"
