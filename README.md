<div align="center">
<img width="256" height="256" alt="Butterscotch Logo" src="https://github.com/user-attachments/assets/ef8bdd5c-d407-4b3c-a4d5-07b25e8bbc70" />
</div>

<h1 align="center">🥧 Butterscotch 🥧</h1>

# Butterscotch for the Anbernic RG35XX (Original model).

**An unofficial port of [Butterscotch](https://github.com/ButterscotchRunner/Butterscotch), the
open-source GameMaker: Studio runner, to the Anbernic RG35XX running GarlicOS.
It's based on [cobaltgit's Miyoo Mini fork](https://github.com/cobaltgit/Butterscotch/tree/miyoomini), which provides the software renderer, at commit `bf71e243`.**

> [!NOTE]
> The fixes here have been done specifically for the RG35XX, and haven't been tested in other consoles.
> **See [The original README](README.original.md).**

## Status
| Game | Notes |
| -------- | ----- |
| AM2R | Tested with AM2R 1.1 (WAD version 14), through the first area of the game and its first Metroid fights.<br>It holds close to 60 fps at stock CPU speed in most of those rooms, and the heaviest room tested runs slightly below that. Later areas and other games are untested. |
|Other games|Untested|


## Building
### AM2R
The build uses the [rg35xx-toolchain](https://github.com/edemirkan/rg35xx-toolchain) container image,
with this repository in its `workspace/am2r-port` folder and the image tagged `rg35xx-toolchain`:

    port/build.sh arm-clean

## Installation
### AM2R
1. Copy `port/am2r.sh` to `Roms/APPS` on the SD card.
2. Create `Roms/APPS/am2r`, and copy `build/butterscotch` into it.
3. Copy `data.win`, the `lang` folder and the `.ogg` files from your own copy of the game into
   `Roms/APPS/am2r`. They aren't included here.
4. Start it from the GarlicOS menu.

## Controls
### AM2R
The layout follows Metroid Fusion. The buttons act as AM2R's default keyboard keys (the table is `rgBindings` in `src/backends/sdl1.c`), and on its first run `port/am2r.sh` sets the game to one-button aiming and hold-to-arm missiles.

| Button | Action | Button | Action |
|---|---|---|---|
| D-pad | Move | L1 | Hold to aim diagonally, with up or down |
| A | Jump, menu OK | R1 | Hold to arm missiles |
| B or Y | Fire, menu cancel | R2 | Aim lock |
| X | Morph | Select | Switch weapon |
| Start | Start | Menu | Options |
| Menu + Start | Quit to GarlicOS | Vol+ / Vol- | Master volume, saved in `volume.txt` |

## Changes from the base fork
### Bug fixes
#### AM2R
- Deactivated instances in a persistent room were deleted when leaving it. In AM2R, floors disappeared
  after pausing.
- Instances that were inactive when the spatial grid first synced never entered it, and instances in a
  restored persistent room weren't put back. In AM2R, some ledges couldn't be grabbed.
- Collisions skipped an instance destroyed earlier in the same pass. In AM2R, missiles couldn't damage
  Metroids. This one is inferred from what the game needs, not from how the original runner behaves.
- `--lazy-rooms` freed and reloaded the tiles of persistent rooms, undoing runtime tile changes. In AM2R,
  rooms had the wrong colours and missing blocks after pausing.
- `SDL_JoystickName` was called every frame on SDL 1.2. On this device it dropped the game to about 9 fps.
**Fixes in the software renderer**
- Added the missing `gpuSetTexFilter`, without which no game could start.
- `drawSpritePart` took integers where the renderer interface passes floats, which crashed tile drawing.
- A view port offset could draw past the framebuffer. AM2R's earthquake effect crashed because of it.
- Removed log calls that ran on every blend mode change and surface switch.
- Added a faster path for sprites with no tint or blending.
- Sprite rows are now tinted and blended with NEON, 8 pixels at a time.
**RG35XX support**
- A fixed 320x240 game window, doubled to the 640x480 screen in 16-bit colour.
- Button input and volume keys.
- Triple buffering through `AM2R_TRIPLEBUF`, so a slow frame doesn't halve the frame rate.
**Debugging tools.**
- These are still in the code, and most are enabled by environment variables: room
jumping, object spawning, a crash reporter, a sampling profiler, surface tracing and a spatial grid
checker. See `port/am2r-bench.sh`.

## Credits and licence
- Butterscotch is by MrPowerGamerBR and contributors.
- Software renderer and Miyoo Mini port come from cobaltgit's fork.
- This work is licensed under the AGPL-3.0, like the projects it's based on. See `LICENSE`.
- Butterscotch has no association with the games it can run, and neither does this port.

## LLM usage
- Substantial parts were written using LLMs, particularly Claude.
