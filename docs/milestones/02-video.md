# Milestone 2 — video (in progress)

Date: 2026-10-31

## What is implemented

- `cave_video_render()` draws the three 038 layers into the 320x240 framebuffer:
  - 8x8 and 16x16 tile modes, the 64x64 map, row scroll/row select, per-tile priority,
    layer priority, and MAME's visible-area flip handling.
  - layer 0/1 are 4bpp packed; layer 2 is 8bpp stored in MAME's planar, nibble-interleaved
    `layout_8x8x8`, decoded MSB-first with plane 0 mapped to pen bit 7.
  - palette index = `0x4000 + color*256 + pen` for the layers, matching MAME's
    `granularity(256)` on DoDonPachi's layer graphics.
  - 16x16 layers use MAME's 1/2/3-pixel x adjustment; 8x8 layers add 8. Layer 2's 8x8 VRAM
    starts at offset 0, not +0x4000.
- The 013 sprite generator is decoded from the player's 8 MiB sprite ROM:
  - DoDonPachi's no-zoom format (`m_alt_format = true`), 16-byte records, 1024 sprites,
    4 banks selected by video register 4.
  - 4bpp 16x16-granularity sprite data, word-swapped exactly as MAME's
    `ROM_LOAD16_WORD_SWAP` loads u50-u53.
  - global and per-sprite X/Y flip, 10-bit signed positions, palette index
    `color*256 + pen`.
- `--png PATH` writes the last rendered frame; the headless frame hash includes the
  framebuffer.

## Observed output

At frame 8000 of an unattended attract run the framebuffer contains the planet background,
ships, bullets, an explosion and the HUD. The PNG is 320x240 in the board's native
orientation; the SDL2 port rotates it 270 degrees for the cabinet's 240x320 window.
At frame 12000 the high-score/ranking screen renders with its tile panels and rank labels.

These images are development output only and are not committed (game art belongs to the
rights holders).

## Not yet done

- Sprite priority against the tile priority bitmap is approximated (sprites are drawn over a
  tile unless the tile's category is higher). Exact `colpri_cb` semantics are a later pass.
- The sprite ROM is word-swapped at load time, matching MAME's `ROM_LOAD16_WORD_SWAP`; without
  that swap the sprites decode as noise.
- The Linux x86_64 build renders the same frame-600 work-RAM hash as Windows, and a local
  Steam Deck AppImage was built and launched under WSL with SDL's offscreen driver.
- The attract sequence order does not match MAME because instruction timing is not modelled.
  This is expected for the translation model and is tracked for the accuracy milestone.
- Audio (YMZ280B) is implemented and the SDL queue was verified with a WAV tee; see
  `03-audio.md` and `04-runtime-repair.md`.
- The old 15,000-frame control-flow gap was a missing dispatch entry (`059CF0`); it is now
  seeded and the headless runner completed 60,000 frames after the graphics repairs without a
  translated-code fault. The remaining difference from MAME is timing/RNG order, not a
  missing function.
- A MAME state comparison (state 7999, screen 8000) is 1.68/255 mean absolute channel
  difference and 3,475/76,800 differing pixels. The remaining mismatch is mostly the
  approximate sprite/tile priority.

## Commands

```powershell
build\cmake\bin\ddonpach_headless.exe --rom ...ddonpach.zip --frames 8000 --png build\attract.png
```
