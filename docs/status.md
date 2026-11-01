# Status and blockers

Date: 2026-10-31

## Working

- ROM verification (12/12 files, CRC32+SHA-1).
- Static analysis, translation (2,611 functions, no unsupported instructions) and a native
  build for Windows x64.
- Deterministic 600-frame headless boot (work-RAM hash `015a518dbae6e55e`); 60,000
  auto-play frames completed clean after the graphics repairs.
- Tile layers and 013 sprites render the attract background, ships, bullets, explosion and
  HUD. SDL2 window front end builds and runs from the player's ROM.
- Runtime repair completed: the 16x16 layer offset, layer-2 VRAM base and planar 8bpp decode
  are fixed. A MAME state comparison (state 7999, displayed pixels 8000) is now
  **1.68/255 mean absolute channel difference, 3,475 of 76,800 pixels differing**, down from
  12.46/51,996 before the layer fixes. See `docs/milestones/04-runtime-repair.md`.
- The SDL texture is opaque (`0xFF` alpha) and the front end fast-forwards the board's long
  black/solid boot phase until the first frame with actual content; the ranking/attract
  screen is visible within a few seconds instead of after a minute of black.
- SDL controls were verified end-to-end: sending coin (`5`) then start (`Enter`) moved the
  running window into the playfield (planet background, player ship, enemies, HUD). SDL2
  game-controller support was added (D-pad/left stick, A shot, B bomb, X auto, Start/R1
  start, Back/L1 coin) and an XInput controller was detected on Windows.
- Frame pacing is now a single absolute 57.55 Hz deadline with no vsync: a 6,000-frame
  Windows run measured 17.33 ms average (57.70 fps), p99 17.37 ms, max 20.19 ms. The previous
  vsync-plus-17 ms loop halved the effective frame rate.
- YMZ280B audio is implemented (8 voices, ADPCM/PCM, pitch/level/pan, stereo) and the SDL
  front end plays it at 44.1 kHz. A headless WAV dump of 8,000 frames has peak 16,167 and
  2.42 million non-zero samples; the SDL device reports 44,100 Hz stereo and the queue starts.
- The SDL `--audio-wav` tee captured exactly what was queued in a 6,000-frame gameplay run:
  4,597,741 frames, peak 10,047, 3,382,412 non-zero samples, first non-zero at the coin/start
  input (~65.9 s). The queue stays non-empty and drains, so generation and delivery are both
  confirmed; the early boot/attract screens can be silent. The Windows default endpoint was a
  Steam Streaming Microphone virtual device, so `--list-audio`/`--audio-device` were added;
  opening `Speakers (Realtek(R) Audio)` was verified.
- The headless runner completed **60,000 auto-play frames** after the graphics fixes without a
  translated-code fault. Open-bus writes (the board decodes no device there) are counted but
  ignored, as the hardware does; the previous 15,000-frame control-flow gap was a missing
  dispatch entry (`059CF0`).
- ASan + UBSan Linux headless: 20,000 auto-play frames clean, plus a final 5,000-frame run
  after the last graphics change; no sanitizer errors, no translated fault.
- ROM-free runtime fixture for CI; CTest boot test passes.
- Linux x86_64 build verified in WSL Ubuntu 22.04: the headless runner reaches the same
  frame-600 work-RAM hash as Windows (`015a518dbae6e55e`). A local AppImage was rebuilt and
  launched for 300 frames with SDL's offscreen drivers. It contains translated code derived
  from the build ROM, so it is a local artifact only and is git-ignored; it is not a
  distributable release.

## Blockers / next work

1. **Timing/RNG divergence.** Instructions take no time, so timing-seeded RNG and the exact
   attract/demo order differ from MAME even though the translated logic is the same. The
   previous 15,000-frame fault was a missing dispatch entry (`059CF0`) plus an open-bus write;
   with those handled, the headless runner completed 60,000 frames without a translated-code
   fault. Matching MAME's frame-by-frame sequence needs a cycle/timing model, which is a
   later accuracy milestone rather than a control-flow bug.
2. **Inputs/EEPROM tests.** Keyboard and SDL game-controller mapping plus the 93C46 protocol
   are implemented; coin/start were verified by driving the window. There is no automated
   scripted input test or EEPROM persistence test yet. Controller button-by-button testing on
   Steam Deck hardware is pending.
3. **Sprite priority.** Sprites draw over lower-priority tiles, but the exact `colpri_cb`
   priority interaction is approximated. The MAME state comparison still differs on 3,475
   pixels, mostly from this and one-frame state differences.
4. **Crash reproduction.** No crash remains reproducible from the current source. ASan/UBSan
   runs and repeated SDL gameplay runs are clean. The earlier invalid-memory class (layer-2
   scratch/adjacent read) has been removed. If a crash is still seen with a specific input
   sequence, capture the Windows exception code and the last `--stats` line.
5. **Steam Deck packaging.** `scripts/package_appimage.sh` builds an AppDir and calls
   `appimagetool`; it was rebuilt on Ubuntu 22.04 and the AppImage launched for 300 frames
   with SDL's offscreen drivers. It has not been run on SteamOS or Steam Deck hardware, and
   its real display/audio/controller behavior on Deck is untested.
6. **tools/core provenance.** `tools/core` is currently a nested checkout at `ed4e157`, not a
   pinned submodule. It also carries one local fix in `m68000/translate.py` for PEA-pushed
   return addresses (functions that call a helper with a pushed return address and continue at
   it), recorded in `patches/0001-m68000-translate-pea-return.patch`. That fix should be
   upstreamed before a public release. The local build scripts apply the patch automatically,
   and `.gitmodules` pins `tools/core` to the `tmnt` branch.

## Local artifacts

The verified local builds are under `build/release/` (git-ignored):

- `build/release/windows/DoDonPachi-Windows-x64.exe` with `SDL2.dll`
- `build/release/DoDonPachi-SteamDeck-x86_64.AppImage`

They embed C translated from the build ROM's program. They are for the ROM owner's own use
and must not be uploaded or redistributed.

The AppImage was built on Ubuntu 22.04 (glibc 2.35) for SteamOS compatibility. SHA-256 of the
verified local artifacts:

- Windows exe: `586b259d7e48a40a0ecd00194e8f95318d12a8178a1ec547359b0f738cb58191`
- Windows SDL2.dll: `ab4af8e2e92d0582fa940127a3c3975e3e249f19af91c74e81eb65fb4c4529c6`
- AppImage: `ae03a852c18923d9e8253e9c55e7a5bf3bb8323ae7bc0f7812be1d45fb211b21`

The AppImage was launched under WSL with SDL's dummy video driver; Steam Deck hardware and
its real display/audio/input stack remain untested.

## Distribution policy

Release archives contain only the port's binaries, licenses and documentation. They never
contain ROM data, extracted assets, generated `build/generated/` C, or screenshots with game
art. The player builds from their own `ddonpach.zip`; generated code is git-ignored.
