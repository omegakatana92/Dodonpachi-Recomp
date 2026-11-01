# Milestone 4 — runtime repair (crash, graphics, audio, pacing)

Date: 2026-10-31

This pass targeted the four reported runtime faults: silent audio, slow gameplay, crashes and
corrupted rendering. It did not change the translated 68000 program.

## Frame pacing (slow gameplay)

Root cause: the SDL2 renderer was created with `SDL_RENDERER_PRESENTVSYNC` **and** the frame
callback also slept until 17 ms had elapsed. A 60 Hz present followed by a 17 ms delay made a
frame take about 34 ms, roughly halving the game speed. The delay was also relative to each
frame, so late frames permanently lowered the average rate.

Fix in `ports/sdl2/main.c`:

- the renderer no longer requests vsync;
- pacing uses an absolute high-resolution deadline derived from the board's 57.55 Hz
  (`perf_frequency / 57.55`), with a late-frame resynchronisation guard;
- audio and video are not used to compensate for each other.

Measured on Windows 11, 6,000-frame gameplay run:

```text
stats frame=5700 avg=17.33ms (57.70fps) p99=17.38ms max=17.42ms queue=1684
stats frame=6000 avg=17.33ms (57.70fps) p99=17.37ms max=20.19ms queue=3728
```

The first 300-frame report in a run includes the zero-initialised statistics ring; ignore its
apparent 114 fps average. The settled rate is 57.7 fps.

## Graphics corruption

Three independent decoding errors were found by rendering a captured MAME board state with
the native renderer and comparing the pixels to MAME's 320x240 output:

1. **16x16 layer offset.** MAME's `cave_v.cpp` adjusts 16x16 layers by 1/2/3 pixels; the
   renderer always applied the 8x8 adjustment (9/10/11). Every 16x16 layer was shifted eight
   pixels horizontally.
2. **Layer 2 VRAM base.** Layers 0/1 keep their 8x8 map at `+0x4000`; layer 2's entire 16 KiB
   VRAM is the 8x8 map at offset 0. The renderer read layer 2 at `+0x4000`, i.e. the unused
   scratch half, so the layer-2 text/HUD tiles were missing or garbage.
3. **Layer 2 pixel format.** MAME's layer-2 graphics use `layout_8x8x8`, an eight-plane
   nibble-interleaved layout. It is not one byte per pixel. The old linear decoder returned
   pen 0 for the layer-2 text. `tile_pen_8bpp()` now follows MAME's `gfx_element::decode`
   exactly: `readbit()` is MSB-first and plane 0 maps to pen bit 7.

Also fixed: 4bpp tile codes are reduced modulo the 65536-character element, matching MAME's
`code % elements()` rather than becoming blank when a 16x16 code exceeds that range.

Validation with MAME 0.289, state from frame 7999 and displayed pixels from frame 8000
(VBLANK latches the state before the IRQ handler updates it):

| Revision | Mean abs channel diff | Differing pixels |
| --- | ---: | ---: |
| Initial comparison | 127.79 | 74,513 / 76,800 |
| Correct video registers | 44.64 | 74,768 / 76,800 |
| Correct 16x16 offset | 6.38 | 10,364 / 76,800 |
| Correct layer-2 base + planar decode | **1.68** | **3,475 / 76,800** |

The remaining difference is mostly the approximate sprite/tile priority interaction and some
one-frame state differences, not missing or fragmented graphics. The demonstration banner,
HUD panels and layer-2 text are present.

The live SDL screenshot at frame 5000 of an auto-start run shows the player ship, HUD, stage
background and effects without white fills, missing tiles or fragmented sprites.

## Audio

The YMZ280B core and SDL queue were already generating samples. This pass added an
`--audio-wav` tee in the SDL front end so the exact bytes handed to `SDL_QueueAudio` can be
checked, and it logs the audio driver and device.

Final 6,000-frame Windows run:

```text
ddonpach: audio device 44100 Hz, 2 channels
ddonpach: audio queue started (766 frames)
...
queue=1684..5816 bytes (drained every frame)
```

Captured queued audio:

```text
frames 4,597,741 (104.26 s at 44.1 kHz stereo)
peak 10,047 of 32,767
non-zero samples 3,382,412
first non-zero sample at frame 2,905,010 (~65.9 s, i.e. the coin/start input at board frame 3800)
```

The queue remains non-empty and is consumed by the audio device, so audio is both generated
and delivered. The first audible portion is the music/SFX that starts when the game is
started; the early boot/attract screens can be silent.

A host-device problem was also found: SDL's default Windows endpoint on this machine was
**"Speakers (Steam Streaming Microphone)"**, a virtual streaming device. WASAPI consumed the
queue, but that endpoint is not a physical speaker, which can present as "no audible sound".
The front end now supports:

```text
--list-audio                 list playback endpoints
--audio-device "NAME"        open a specific endpoint
--audio-wav PATH             record exactly what SDL is given
```

It logs the driver and opened device and warns when the name looks streaming/virtual. Opening
`Speakers (Realtek(R) Audio)` was verified:

```text
ddonpach: audio driver wasapi, device Speakers (Realtek(R) Audio), 44100 Hz, 2 channels
```

A physical speaker listening test on Steam Deck hardware was not possible in this environment.

## Crash

No reproducible crash remains in the current source. The prior invalid-memory class was
removed:

- the layer-2 renderer no longer reads the scratch/adjacent half of the layer RAM;
- sprite ROM reads are bounds-checked;
- the missing dispatch entry `059CF0` and open-bus writes were already handled.

Evidence:

- ASan + UBSan Linux headless: 20,000 auto-play frames clean (and a final 5,000-frame run
  after the last graphics change), no sanitizer errors, no translated fault;
- release headless: 60,000 auto-play frames, no translated fault, 40 ignored open-bus writes;
- Windows SDL: three consecutive 4,000-frame auto-start gameplay runs completed with stable
  pacing and no fault;
- window close during a run finalised a valid 327,205-frame WAV and exited cleanly.

A sanitizer trace of the original crash sequence was not available because the failure could
not be reproduced from the current source. The remaining risk is timing/RNG-dependent game
logic, which is tracked as an accuracy issue rather than a memory-safety issue.

## Input

SDL2 now opens the first available game controller and supports hot-plug. Mapping:

- D-pad/left stick: direction
- A: shot, B: bomb, X: auto
- Start/R1: start, Back/L1: coin

The connected Windows XInput controller was detected and the game ran; button-by-button
physical testing on Steam Deck hardware remains pending.

## Linux / Steam Deck artifact

Ubuntu 22.04 `build/linux22` rebuilt successfully and
`build/release/DoDonPachi-SteamDeck-x86_64.AppImage` was produced with
`scripts/package_appimage.sh`. The AppImage ran 300 frames under WSL with SDL's dummy video
and audio drivers. It has not been run on SteamOS/Steam Deck hardware.

## Remaining limitations

- Attract/demo order and timing-seeded RNG still differ from MAME because instructions take no
  time. This is not adjusted by changing frame rate.
- Sprite/tile priority is still an approximation of MAME's `colpri_cb`; the state comparison
  differs on a small number of already-aligned pixels.
- YMZ280B end-of-voice IRQ is not wired to the board cause register.
- `tools/core` is a nested checkout at `ed4e157` with the PEA-return patch in `patches/`;
  the checkout should be converted to a pinned submodule and the patch upstreamed.
- Steam Deck display/audio/controller hardware validation is still required.
