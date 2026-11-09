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
- Freed surface IDs were handed out again right away, and the first surface got ID 0, which games use for
  "not created yet". AM2R frees a stale ID every step, so a new surface that got it was freed too. In AM2R,
  small item pickups showed a black screen and the Zeta Metroid was invisible.
- With `--load-type map-file`, every animation frame of a sprite used the collision mask of its first frame.
  In AM2R, missile explosions stayed tiny and couldn't push the batteries, and super missiles had the wrong hitbox.
- `SDL_JoystickName` was called every frame on SDL 1.2. On this device it dropped the game to about 9 fps.
### Fixes in the software renderer
- Added the missing `gpuSetTexFilter`, without which no game could start.
- `drawSpritePart` took integers where the renderer interface passes floats, which crashed tile drawing.
- A view port offset could draw past the framebuffer. AM2R's earthquake effect crashed because of it.
- Partly transparent texture pixels were drawn as fully solid. In AM2R, the shadow overlay in some rooms was
  an opaque bar over half the screen. About 5% of its images use partial transparency, including the HUD panels.
- The texture cache evicted pages in load order, and only after an allocation failed. It now evicts the least
  recently used page, and `SWR_TEXTURE_BUDGET_MB` can limit its size.
- Circles worked out a sine and cosine for every point, even off screen. In Undertale, a Snowdin room with
  snow piles ran at about 17 fps because of it.
- Alpha values outside 0 to 1 weren't clamped, as a GPU would. In Undertale, a fader left running after the
  Papyrus date reached a large negative alpha and blacked out the screen, with the dialogue still going.
- `surface_exists` was false for the application surface, so copies of the screen came out black. In AM2R,
  item pickups, the pause screen and room transitions had a black backdrop, and the water ripples were missing.
- Unscaled sprites clipped at the right or bottom edge lost a source pixel, which made them take the much slower
  scaled path and stretched them slightly.
- Stacked translucent layers of one color are merged into one. Undertale's bridge fall adds a full-screen layer
  every frame for its fade to white, which slowed the scene to about 3 fps.
- Removed log calls that ran on every blend mode change and surface switch.
- Added a faster path for sprites with no tint or blending.
- Sprite rows are now tinted and blended with NEON, 8 pixels at a time, and untinted solid or clear rows
  4 at a time without splitting color channels. Rectangles are blended with NEON too.
- Sprites rotated by exact quarter turns were placed up to a pixel apart, because their position was rounded
  but their rotation center wasn't. In AM2R, the super bomb and battery explosions, drawn as four quarters
  around one point, showed a cross-shaped line. Quarter turns and fractionally scaled sprites now have their
  own path, which rounds edges like a GPU and is much faster than the general rotated one.
- Pixel-precise collisions worked out a sine and cosine for every pixel they tested, now once per pair of
  instances. Together with the fix above, the worst stretch of a super bomb went from about 29 ms a frame to 11 ms.
- Adding or subtracting at full strength now handles four pixels at once, and clear parts of soft sprites are skipped.
  AM2R's light mask is kept at half size on each side, which is not a bug fix but a trade-off: the soft lights lose
  very little, and in the dark rooms of its last areas a frame went from about 28 ms to about 23 ms before this
  change, with the half-size mask saving about 3 ms more in a test without a player.
### RG35XX support
- A fixed 320x240 game window, doubled to the 640x480 screen in 16-bit color.
- Button input and volume keys.
- A native 640x480 mode through `RG35XX_FULL_RES`, for games with that window size.
- Triple buffering through `AM2R_TRIPLEBUF`, so a slow frame doesn't halve the frame rate.
- The log is buffered, since a write to the SD card for every line could stall a frame.
### Debugging tools
- These are still in the code, and most are enabled by environment variables: room
jumping, object spawning, a crash reporter, a sampling profiler, surface tracing and a spatial grid
checker. Check `port/am2r-bench.sh`.
