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
> [!NOTE]
> There are no AM2R files in this repository, and no links to them. Finding your own copy of the game is up to you.
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
**See [bug_fixes.md](bug_fixes.md).**

## Credits and licence
- Butterscotch is by MrPowerGamerBR and contributors.
- Software renderer and Miyoo Mini port come from cobaltgit's fork.
- This work is licensed under the AGPL-3.0, like the projects it's based on. See `LICENSE`.
- Butterscotch has no association with the games it can run, and neither does this port.

## LLM usage
- Substantial parts were written using LLMs, particularly Claude.
