# Bug fixes
## AM2R
- Deactivated instances in a persistent room were deleted when leaving it. In AM2R, floors disappeared
  after pausing.
- Instances that were inactive when the spatial grid first synced never entered it, and instances in a
  restored persistent room weren't put back. In AM2R, some ledges couldn't be grabbed.
- Collisions skipped an instance destroyed earlier in the same pass. In AM2R, missiles couldn't damage
  Metroids. This one is inferred from what the game needs, not from how the original runner behaves.
- `--lazy-rooms` freed and reloaded the tiles of persistent rooms, undoing runtime tile changes. In AM2R,
  rooms had the wrong colors and missing blocks after pausing.
- `layer` was always treated as the built-in variable that GameMaker Studio 2 added, even in older games,
  where it's an ordinary variable. In AM2R, the walls hiding secret rooms never faded out.
- `SDL_JoystickName` was called every frame on SDL 1.2. On this device it dropped the game to about 9 fps.
### Fixes in the software renderer
- Added the missing `gpuSetTexFilter`, without which no game could start.
- `drawSpritePart` took integers where the renderer interface passes floats, which crashed tile drawing.
- A view port offset could draw past the framebuffer. AM2R's earthquake effect crashed because of it.
- Partly transparent texture pixels were drawn as fully solid. In AM2R, the shadow overlay in some rooms was
  an opaque bar over half the screen. About 5% of its images use partial transparency, including the HUD panels.
- Removed log calls that ran on every blend mode change and surface switch.
- Added a faster path for sprites with no tint or blending.
- Sprite rows are now tinted and blended with NEON, 8 pixels at a time.
### RG35XX support
- A fixed 320x240 game window, doubled to the 640x480 screen in 16-bit color.
- Button input and volume keys.
- Triple buffering through `AM2R_TRIPLEBUF`, so a slow frame doesn't halve the frame rate.
### Debugging tools
- These are still in the code, and most are enabled by environment variables: room
jumping, object spawning, a crash reporter, a sampling profiler, surface tracing and a spatial grid
checker. Check `port/am2r-bench.sh`.
