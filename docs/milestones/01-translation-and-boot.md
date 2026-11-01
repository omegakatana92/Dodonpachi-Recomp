# Milestone 1 — static translation, board model, headless boot

Date: 2026-10-31

## Verified

Commands run from the repository root against
`C:\Users\omega\Downloads\ddonpach.zip` (the player's own World set):

| Step | Command | Result |
| --- | --- | --- |
| ROM | `python tools\verify_rom.py ...ddonpach.zip` | `12/12 files` |
| Analysis | `python tools\disassemble.py ...ddonpach.zip` | 67,796 instructions, 620 functions, 8,695 labels |
| Translation | `python tools\recompile.py ...ddonpach.zip` | 2,611 C functions, 74,011 instructions written, `not translated: 0` |
| Build | `cmake --build build\cmake --config Release` | `ddonpach_headless.exe`, `DoDonPachi-Windows-x64.exe`, `SDL2.dll` |
| Boot | `ddonpach_headless --frames 600 --hashes build\boot.hashes` | 600/600 frames, no fault, 0 unmapped reads, 0 unmapped writes |
| Determinism | the same command twice, `Compare-Object` on the hash logs | identical for all 600 frames |

The boot log ends with work-RAM hash `015a518dbae6e55e` at frame 600 for this symbol set and
the current runtime. (The earlier `590e6a42d8e17b32` value predates the missing-dispatch and
open-bus handling.) The hash is derived only from board memory; it is not a ROM hash.

## What runs

- The reset vectors are read from the player's ROM (`SSP 00110000`, `PC 0052EA`).
- The translated reset routine initialises the board, waits on the frame routine at
  `05718E`, initialises the EEPROM-backed settings, and enters the main loop at `000A80`.
- The board bus implements the MAME `ddonpach_map`: 1 MiB ROM, 64 KiB work RAM, YMZ280B
  latch, 64 KiB sprite RAM, three layer RAMs, video registers, layer controls, palette,
  inputs and the 93C46 EEPROM.
- The VBLANK raises the level-1 autovector (`000622`). The handler reads the IRQ cause at
  `800000`/`800004`, sets `100F44`, and returns with RTE.
- `program_frame_wait()` advances the board once per idle instruction; the translated
  program's C call stack continues across frames.

## ROM-free CI

`-DDDP_FIXTURE=ON` builds `tests/fixture`, a hand-written program that exercises the bus,
frame wait and hashing without any ROM. The fixture runs on Windows and Linux in CI.

## Known limits at this milestone

- Instructions take no time; the game's RNG/attract order therefore diverges from MAME over
  long runs (see `docs/status.md`).
- The static analysis needs the seeding tools in `tools/seed_*.py`; the runtime gap collector
  (`DDP_COLLECT_GAPS=1`) remains available for new computed entries. No gap is reported in
  the first 120,000 unattended frames with the current seeds.
- The video and audio milestones are separate; see `02-video.md`.
