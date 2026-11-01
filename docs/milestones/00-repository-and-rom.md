# Milestone 0 — repository, ROM verification, static analysis

Date: 2026-10-31

## Verified

- ROM set: MAME `ddonpach` (World, 1997 2/5 Master Ver.).
- `python tools\verify_rom.py rom\ddonpach.zip` → `12/12 files`.
- Program image: 1 MiB, `b1.u27` even bytes, `b2.u26` odd bytes.
- Reset vector: SSP `0x00110000`, PC `0x000052EA`.
- Main loop entry: `0x00000A80`.
- Frame-wait routine: `0x0005718E`; idle instruction `0x000571B2`.

## Current work

Full static analysis must include `0x400-0xFFFFF` (`CODE_START = 0x400`). Earlier
`CODE_START = 0x1000` skipped the boot code and produced only 1,222 instructions / 48
functions, which is not sufficient.

## Notes

- The assigned path `C:\WINDOWS\system32\DoDonPachi-Native` was not writable; the project is
  at `C:\Users\omega\DoDonPachi-Native`.
- `tools/core` is currently a plain checkout of branch `tmnt`, not a submodule. It must be
  converted to a pinned submodule before the first public commit.
