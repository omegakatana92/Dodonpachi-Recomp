# AGENTS.md — DoDonPachi Native

## Mission

Build genuine native ports of **DoDonPachi (Cave/Atlus, 1997, World set `ddonpach`)** for
Windows 11 x64 and Steam Deck (Linux x86_64).

The port is **not** an emulator and must not embed an emulator in the shipped game:

- the 68000 program is translated ahead of time into C and compiled to native code by the
  shared `arcade-decomp` toolchain (`tools/core`, checked out on the `tmnt` branch);
- the runtime implements the Cave first-generation board directly (bus, RAM, tilemaps,
  sprites, palette, inputs, EEPROM, video timing, YMZ280B audio);
- the user supplies their own `ddonpach.zip`; no copyrighted ROM, graphics, sample, or
  translated ROM-derived C is committed or shipped.

## Non-negotiables

1. **Never distribute ROM data.** Do not commit `rom/*.zip`, extracted ROM files, generated
   `build/generated/` C, screenshots containing game art as release assets, or sample data.
2. **Never claim success without running the check.** Every milestone needs a command that
   was actually executed and its output recorded.
3. **Keep the shared codebase shared.** Windows and Linux use the same `runtime/`, `ports/`,
   and CMake targets. Platform differences belong in small port files, not forks.
4. **Do not edit `tools/core` unless fixing the upstream toolchain.** It is intended to be a
   submodule at a pinned commit. Record upstream license and provenance.
5. If a required directory is not writable, stop and report it rather than silently writing
   elsewhere.

## Repository layout

```
AGENTS.md                  this file
README.md                  player/build overview
CMakeLists.txt             shared Windows/Linux build
symbols.txt                68000 symbols used by the static analysis
tools/
  core/                    arcade-decomp toolchain (upstream, BSD-3-Clause)
  ddp_rom.py               ddonpach ROM hashes + region builders
  game_profile.py          core configuration for this game
  verify_rom.py            ROM verification
  disassemble.py           static analysis -> build/listing/
  recompile.py             static translation -> build/generated/ C
runtime/
  include/m68000/          68000 runtime interface used by generated C
  include/cave/            board model, video, frame, program interfaces
  src/                     board model, video, audio, runtime glue
ports/
  headless/                deterministic frame runner and test harness
  sdl2/                    window, input, audio device
tests/                     unit and boot/smoke tests
.github/workflows/         Windows + Linux CI
docs/                      architecture, milestones, licensing
rom/                       local ROM only; git-ignored
build/                     generated output; git-ignored
```

## Hardware target

Cave first-generation 68000 board, as implemented by MAME `src/mame/atlus/cave.cpp`
(`ddonpach_map`) and its video/sprite devices. Reference only; MAME code is not linked into
the game.

- CPU: Motorola 68000 at 16 MHz.
- Work RAM: 64 KiB.
- Video: 320x240 visible, rotated 270 degrees in the cabinet; 3 tile layers plus 1024
  zooming sprites from the NEC 013 generator.
- Sound: YMZ280B at 16.9344 MHz, 4 MiB sample ROM.
- Inputs: active-low IN0/IN1, 93C46 serial EEPROM.
- Interrupt: VBLANK autovector level 1; IRQ cause register at `0x800000`.

### 68000 memory map

| Address | Device |
| --- | --- |
| `0x000000-0x0FFFFF` | program ROM (1 MiB, 16-bit interleaved `b1.u27`/`b2.u26`) |
| `0x100000-0x10FFFF` | work RAM (64 KiB) |
| `0x300000-0x300003` | YMZ280B (byte-wide registers) |
| `0x400000-0x40FFFF` | sprite RAM (64 KiB) |
| `0x500000-0x507FFF` | layer 0 tile RAM |
| `0x600000-0x607FFF` | layer 1 tile RAM |
| `0x700000-0x703FFF` | layer 2 tile RAM, mirrored every 16 KiB |
| `0x800000-0x80007F` | 013 video registers |
| `0x800000-0x800007` | IRQ cause (read) |
| `0x900000-0x900005` | layer 0 control |
| `0xA00000-0xA00005` | layer 1 control |
| `0xB00000-0xB00005` | layer 2 control |
| `0xC00000-0xC0FFFF` | palette, 16-bit writes |
| `0xD00000-0xD00001` | IN0 |
| `0xD00002-0xD00003` | IN1 + EEPROM |
| `0xE00000` | EEPROM / coin counter write |

Frame timing follows MAME's screen configuration: 15.625 kHz line rate, 271.5 lines per
frame, giving 278,016 CPU clocks per frame at 16 MHz (57.55 Hz). These constants live in
`runtime/include/cave/frame.h`. The main loop's idle point is
`0x0571B2` (`tst.w $100F44` inside the frame-wait routine at `0x05718E`), which is what
`game_profile.FRAME_WAIT` names for the translator.

## Toolchain workflow

All tool invocations are from the repository root. Python 3.11+ and CMake 3.20+ are required.

```powershell
# 1. Verify the player's ROM (must be the exact MAME ddonpach set)
python tools\verify_rom.py rom\ddonpach.zip

# 2. Static analysis -> build/listing/ddonpach.s + report.txt
python tools\disassemble.py rom\ddonpach.zip

# 3. Translate the analysed program -> build/generated/program*.c
python tools\recompile.py rom\ddonpach.zip

# 4. Configure and build (Windows example)
cmake -S . -B build\cmake -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build\cmake

# 5. Run the deterministic boot smoke test
build\cmake\bin\ddonpach_headless.exe --frames 600 --hashes build\boot.hashes
```

On Linux replace `\` with `/` and use the same `--frames` invocation.

### Expected results

- `verify_rom.py`: `12/12 files` and exit code 0.
- `disassemble.py`: writes a listing and a report; a report with only a handful of functions
  is a failure, because the game's main loop and its initialisation must be present.
- `recompile.py`: writes `build/generated/program.h`, `program_NN.c`, `entries.c` and does not
  raise `Unsupported`.
- Headless: at least 600 frames, no missing translated code, no invalid bus accesses, no
  repeated reboot. Save the hash log and the last lines of stderr in the milestone note.

## Milestones

| # | Deliverable | Acceptance |
| --- | --- | --- |
| 0 | Repository, ROM check, analysis | ROM hashes match; listing contains reset, boot, main loop and frame wait |
| 1 | Static translation + board model + headless boot | 600 frames, no missing code, valid RAM/register writes |
| 2 | Video output | 320x240 frame with non-blank title/attract picture; PNG snapshots per frame |
| 3 | Input + EEPROM + sound | Coin/start controls work; EEPROM persists; YMZ280B produces audio |
| 4 | SDL2 front end | Windows `.exe` and Linux binary run from a user-supplied ROM |
| 5 | Packaging + CI | GitHub Actions build both targets; Steam Deck `DoDonPachi-SteamDeck-x86_64.AppImage` |
| 6 | Accuracy/decompilation | Frame hashes compared with MAME reference; readable C where practical |

Milestones 1-5 establish the playable port. Milestone 6 is the long-term decompilation goal.

## Testing requirements

- Every commit that changes analysis, translation, or runtime must pass:
  - ROM verification;
  - static analysis (no crash, expected reset/main-loop addresses present);
  - translation (no `Unsupported`, expected function count does not collapse);
  - headless boot (at least 60 frames in CI, 600 locally).
- Video changes need a saved PNG and a stated expected frame number.
- Audio changes need a WAV and the sample-rate/channel metadata.
- Never compare against a screenshot from a different ROM revision.
- Keep a short log under `docs/milestones/` for each completed milestone.

## Coding conventions

- C11. Big-endian 68000 values are handled explicitly; do not rely on host byte order.
- Names use lower_snake_case; board constants use `DDP_` and 68000 constants `M68K_`.
- All hardware behavior cites the MAME file/function it follows in a comment.
- Do not add an emulator core to the shipped target. The MAME reference may exist only under
  `tools/reference` for development and must never be linked into `ddonpach` or ports.
- Keep `tools/core` pristine; put game-specific settings in `tools/game_profile.py`.

## Licensing

- `tools/core` is BSD-3-Clause, copyright Sebastian Toyos; see `tools/core/LICENSE`.
- MAME and ymfm are BSD-3-Clause. They may be used for development reference tools, never
  in the released game.
- The port's own code is provided under the repository's chosen license; third-party notices
  must be preserved in `LICENSES/` and in release archives.
- Do not include the ROM or generated translated C in source or binary releases.

## Known environment note

The original assigned workspace `C:\WINDOWS\system32\DoDonPachi-Native` was not writable by
the development user. The project lives at `C:\Users\omega\DoDonPachi-Native`.
