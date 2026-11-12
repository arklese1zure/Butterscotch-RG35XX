<div align="center">
<img width="256" height="256" alt="Butterscotch Logo" src="https://github.com/user-attachments/assets/ef8bdd5c-d407-4b3c-a4d5-07b25e8bbc70" />
</div>

<h1 align="center">🥧 Butterscotch 🥧</h1>

# Butterscotch for the Anbernic RG35XX (Original model).

**An unofficial port of [Butterscotch](https://github.com/ButterscotchRunner/Butterscotch), the
open-source GameMaker: Studio runner, to the Anbernic RG35XX, with drawing on its GPU (PowerVR SGX544, OpenGL ES 2.0).
It's based on [cobaltgit's Miyoo Mini fork](https://github.com/cobaltgit/Butterscotch/tree/miyoomini), which provides the software renderer, at commit `bf71e243`.**

> [!NOTE]
> This build was made and tested on [MiaUI](https://github.com/miaui-cfw/MiaUI-GPU-RG35XX) 2.0, the only system it has been tried on.
> It only needs SDL2 and OpenGL ES 2.0 with the GPU driver, so other systems for this console that have them
> (based on MinUI or Batocera, for example) might run it too, possibly with different button numbers.
> The fixes here have been done specifically for the RG35XX, and haven't been tested in other consoles.
> One build runs both games below, from this branch (`rg35xx`). Each game has its own launcher and folder.
> **See [The original README](README.original.md).**

> [!TIP]
> The earlier version for GarlicOS (software rendering only) is at
> [Butterscotch-RG35XX-GarlicOS](https://github.com/arklese1zure/Butterscotch-RG35XX-GarlicOS).
> This branch still builds it (`port/build.sh arm`), but it's no longer tested.

## Status
| Game | Notes |
| -------- | ----- |
| AM2R | Tested with AM2R 1.1 (WAD version 14), drawn by the GPU. Every area was visited from a save near the end of the game, including the rooms with lava, water, darkness and fading walls.<br>It holds 60 fps in nearly all rooms at MiaUI's game CPU speed. The heaviest room tested, the sand room in area 3, takes about 13 ms a frame, against 15 ms with the software renderer. |
| Undertale | Tested with the Steam Linux version (WAD version 16, `game.unx`), from the start through Waterfall. It still uses the software renderer.<br>It runs at the game's 30 fps at the full 640x480 in the areas tested, with short pauses when new graphics load. Later areas are untested. |
|Other games|Untested|

## Building
The build uses a container image with the public Batocera RG35XX SDK, the same base as MiaUI's own ports.
Its `Dockerfile` is in `port/miaui`. The script expects this repository in a folder named `am2r-port`, inside a
folder of your choice (the workspace), and uses rootless Podman:

    podman build -t miaui-toolchain port/miaui/
    port/build.sh miaui-clean

The result is `butterscotch` in `am2r-miaui/build`, next to `am2r-port` in the workspace.

## Installation
### AM2R
> [!NOTE]
> There are no AM2R files in this repository, and no links to them. Finding your own copy of the game is up to you.
1. Copy the `port/miaui/AM2R.pak` folder to `Ports` on the SD card (`/userdata/Ports`), and copy `butterscotch` into it.
2. Copy `data.win`, the `lang` folder and the `.ogg` files from your own copy of the game into `Ports/AM2R.pak`.
   They aren't included here.
3. Start it from the Ports menu.

### Undertale
> [!NOTE]
> There are no Undertale files in this repository, and no links to them. Finding your own copy of the game is up to you.
1. Copy the `port/miaui/Undertale.pak` folder to `Ports` on the SD card, and copy `butterscotch` into it.
2. Copy the contents of the `assets` folder from your own copy of the game (`game.unx` and the `.ogg` files)
   into `Ports/Undertale.pak`. Other versions, such as Windows, use `data.win` instead. They're untested,
   but changing the last argument in its `launch.sh` to match should be enough.
3. Start it from the Ports menu.

### Settings in `launch.sh`
- `RENDERER`: `modern-gl` draws with the GPU (AM2R's default), `software` with the CPU (Undertale's default).
- `GL_TEXTURE_BUDGET_MB`: how much of the game's graphics the GPU keeps loaded (48 for AM2R). Older ones are
  unloaded and reloaded when needed, since the console only has 256 MB of memory.
- `MIAUI_VSYNC=0`: frames are shown without waiting for the screen refresh, which keeps the pacing steady.
  No tearing has been seen. `1` turns vsync on.
- The CPU runs at MiaUI's game speed, like its emulators and ports.

## Controls
### AM2R
The layout follows Metroid Fusion. The buttons act as AM2R's default keyboard keys (the table is `rgBindings` in `src/backends/miaui.c`), and on its first run the launcher sets the game to one-button aiming and hold-to-arm missiles.

| Button | Action | Button | Action |
|---|---|---|---|
| D-pad | Move | L1 | Hold to aim diagonally, with up or down |
| A | Jump, menu OK | R1 | Hold to arm missiles |
| B or Y | Fire, menu cancel | R2 | Aim lock |
| X | Morph | Select | Switch weapon |
| Start | Start | Menu | Options |
| Menu + Start | Quit to the system menu | Vol+ / Vol- | System volume |

### Undertale
Undertale uses the same buttons as AM2R, which map to its keys like this.

| Button | Action | Button | Action |
|---|---|---|---|
| D-pad | Move | X | Menu |
| A or Start | Confirm | B, Y or L1 | Cancel, hold to run |
| Menu + Start | Quit to the system menu | Vol+ / Vol- | System volume |

Holding Menu on its own acts as Esc, which Undertale uses to quit after a moment.

## Changes from the base fork
### GPU rendering
- A backend for SDL2 and OpenGL ES 2.0 (`src/backends/miaui.c`). With the software renderer, frames go to the
  GPU as 16-bit textures at the game's size and are scaled there. With the GPU renderer, the game draws into an
  image of the same size, so it sees the same screen size either way.
- Butterscotch's GL renderer was twice as slow as the software one on this GPU: every batch of sprites asked the
  driver for a new vertex buffer. Batches now reuse a ring of buffers.
- Fades, darkness, and the water and lava tints drew blue: blending lowered the alpha of solid pixels, so the
  room's background showed through. Alpha now builds up as in the software renderer.
- Graphics the game hasn't used for a while are unloaded, and surfaces left unused for a minute are freed
  (AM2R leaks one in every dark room). Without this, a long session ran out of memory.
- `port/pctest` runs the same build on a PC, loads a save, and compares both renderers room by room.

### Bug fixes
**See [bug_fixes.md](bug_fixes.md).**

## Credits and licence
- Butterscotch is by MrPowerGamerBR and contributors.
- Software renderer and Miyoo Mini port come from cobaltgit's fork.
- The build image follows MiaUI's (by DSkywalk), which uses the Batocera RG35XX SDK.
- This work is licensed under the AGPL-3.0, like the projects it's based on. See `LICENSE`.
- Butterscotch has no association with the games it can run, and neither does this port.

## LLM usage
- Substantial parts were written using LLMs, particularly Claude.
