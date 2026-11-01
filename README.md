# DoDonPachi Native

A genuine native port of **DoDonPachi** (Cave/Atlus, 1997, World set `ddonpach`) to Windows
and Linux/Steam Deck. The 68000 game program is statically translated to C with the
[arcade-decomp](https://github.com/cheviman/arcade-decomp) toolchain and compiled for the
host. The board hardware is implemented by this project's runtime; no emulator core is
shipped and no 68000 instruction fetch/decode remains in the game binary.

> **You must own the arcade board or a legally obtained ROM.** This repository contains no
> ROM data, no extracted graphics or audio, and no translated ROM-derived C. The build tools
> read `rom/ddonpach.zip` from your machine and write generated output under the ignored
> `build/` directory only. Release archives must never contain generated C or ROM data.

## Requirements

- Python 3.11+
- CMake 3.20+ and a C11 compiler (MSVC 2022/2026, clang, or gcc)
- Ninja (recommended on Linux; on Windows the Visual Studio generator is used)
- SDL2 development package for the windowed port
  (Windows: the SDL2 VC development package; Linux: `libsdl2-dev`)
- The exact MAME `ddonpach` ROM set (12 files, CRC32/SHA-1 checked)
- The `arcade-decomp` core under `tools/core` at the commit recorded in
  `patches/README.md`. A clean public clone must initialise the pinned submodule and apply the
  one local patch:

  ```bash
  git submodule update --init tools/core
  git -C tools/core apply ../../patches/0001-m68000-translate-pea-return.patch
  ```

  `scripts/build-local.sh` and `scripts/build-local.ps1` apply the patch automatically when it
  has not been applied yet.

## Quick start

Windows:

```powershell
.\scripts\build-local.ps1 -Rom C:\Users\you\Downloads\ddonpach.zip
build\cmake\bin\ddonpach_headless.exe --rom C:\Users\you\Downloads\ddonpach.zip --frames 600 --hashes build\boot.hashes
build\cmake\bin\DoDonPachi-Windows-x64.exe --rom C:\Users\you\Downloads\ddonpach.zip
```

Linux / Steam Deck:

```bash
./scripts/build-local.sh /path/to/ddonpach.zip
./build/cmake/bin/ddonpach_headless --rom /path/to/ddonpach.zip --frames 600 --hashes build/boot.hashes
./build/cmake/bin/DoDonPachi --rom /path/to/ddonpach.zip
```

## Controls

| Key | Action |
| --- | --- |
| Arrows / WASD | Move |
| Z / J | Shot |
| X / K | Bomb |
| C / L | Third button |
| Enter | Start 1P |
| 5 | Coin 1P |
| 1 | Service |
| 2 / 6 | Start 2P / Coin 2P |
| Esc | Quit |

SDL2 game controller (first connected device, hot-plug supported):

| Control | Action |
| --- | --- |
| D-pad / left stick | Move |
| A | Shot |
| B | Bomb |
| X | Auto |
| Start / R1 | Start 1P |
| Back / L1 | Coin 1P |

The front end fast-forwards the board's black boot phase and shows the ranking/attract screen
within a few seconds. Press **5** then **Enter** (or Back then Start on a controller) to start.
The early boot/attract screens can legitimately be silent; music/SFX begin with the game.

If audio is not audible, list the host endpoints and select a physical one explicitly:

```powershell
build\cmake\bin\DoDonPachi-Windows-x64.exe --list-audio
build\cmake\bin\DoDonPachi-Windows-x64.exe --rom ...ddonpach.zip `
  --audio-device "Speakers (Realtek(R) Audio)"
```

On this development host the Windows default endpoint was a **Steam Streaming Microphone**
virtual device: the queue drained, but it was not a physical speaker. The front end warns when
the opened endpoint looks streaming/virtual. `--audio-wav PATH` records exactly what was queued
for diagnosis.

The scripts run ROM verification, the static analysis, the translator and the build. To run
the checks without building:

```bash
python tools/check_all.py /path/to/ddonpach.zip
```

## Status

- **Milestone 0 — repository, ROM verification, static analysis: complete.**
  `verify_rom.py` reports `12/12 files`; the listing contains the reset vector (`0052EA`),
  the main loop (`000A80`) and the frame wait (`05718E`/`0571B2`).
- **Milestone 1 — static translation, board model, headless boot: complete.**
  The current symbol seeds produce 620 analysed functions and 67,796 distinct instructions;
  the translator writes 2,611 C functions with `not translated: 0`. The headless runner
  completes 600 frames deterministically with no unmapped access (and has run 15,000 frames
  clean during development).
- **Milestone 2 — video: runtime repair complete.** The 038 tile layers (8x8/16x16) and the
  013 sprite generator render the attract background, ships, bullets, explosion, HUD and
  layer-2 text. The 16x16 scroll offset, layer-2 VRAM base and planar 8bpp decoding were fixed.
  A MAME state comparison is within a 1.68/255 mean channel difference (3,475/76,800 pixels)
  from MAME. The headless runner can save a PNG (`--png`).
- **Frame pacing:** the front end uses one absolute 57.55 Hz high-resolution deadline and no
  vsync. The previous vsync-plus-17 ms loop ran at roughly half speed. A 6,000-frame Windows
  run measured 17.33 ms average (57.70 fps), p99 17.37 ms.
- **Milestone 3 — audio: first working pass.** The YMZ280B's eight ADPCM/PCM voices,
  pitch/level/pan and stereo output are implemented; the SDL front end plays them at
  44.1 kHz. The SDL `--audio-wav` tee captured 3.38 million non-zero samples (peak 10,047)
  from a gameplay run, and the queue is consumed every frame. The early attract screens can
  be silent; music/SFX begin with gameplay.
- **Crash status:** no crash is reproducible from the current source. ASan/UBSan completed
  20,000 auto-play frames clean (plus a final 5,000 after the last graphics change), release
  headless completed 60,000 frames, and three consecutive Windows SDL gameplay runs completed.
- **Linux/Steam Deck:** the Linux x86_64 build reaches the same frame-600 work-RAM hash
  `015a518dbae6e55e` as Windows (verified in WSL Ubuntu 22.04), and
  `scripts/package_appimage.sh` produced `DoDonPachi-SteamDeck-x86_64.AppImage`. The AppImage
  launched for 300 frames under WSL with SDL's dummy drivers; real Steam Deck hardware is
  still untested.
- **Not yet done:** exact MAME instruction timing/RNG agreement, scripted input/EEPROM
  persistence tests, physical Steam Deck audio/display/controller validation, and exact sprite
  priority matching.

See `docs/milestones/` for the verified commands and outputs, and `docs/status.md` for the
current blockers.

## Legal

The repository is distributed under **GPL-2.0** (see `LICENSE`), preserving the upstream
repository's license. Individual port files retain their BSD-3-Clause notices where marked;
BSD-3-Clause is GPL-compatible. `tools/core` is BSD-3-Clause, copyright Sebastian Toyos.
`runtime/third_party/miniz` is public domain/Unlicense. MAME and ymfm are used only as
development references under their BSD-3-Clause terms; their code is not linked into
`ddonpach` or the SDL2 port. DoDonPachi is a trademark of its respective owners; this project
is not affiliated with Cave or Atlus.
