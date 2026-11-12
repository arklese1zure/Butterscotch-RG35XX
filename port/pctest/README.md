# PC tests for the MiaUI GPU renderer

The MiaUI backend builds for the PC too (SDL2 + Mesa's GLES), and gives the same pictures as the device. These
scripts run it with the user's AM2R save and compare the GPU renderer with the software one.

- `pcbuild.sh`: copies `am2r-port` to `workspace/am2r-pcgl` and builds it with the host compiler.
- `pcroom.sh <room> <outdir> [keys]`: loads the save, jumps to `<room>` at frame 1150 and captures frames 1160..1500
  with both renderers (`gl-N.png`, `sw-N.png`, `sheet.png` side by side).
- `ptour.sh modern-gl|software <outdir>`: loads the save and visits every room (60 frames each), capturing frame 50 of
  each. `python3 ptourdiff.py <gl outdir> <software outdir> [count]` lists the rooms that differ most.

`AM2R_SAVE` points at the save folder (default: the GarlicOS backup). The GPU runs use `MIAUI_FORCE_ES2=1`, the
device's GLES 2.0 paths. Runs use SDL's offscreen driver, so no window opens.
