# Third-party notices

- `arcade-decomp.txt`: the shared static-translation toolchain under `tools/core/`
  (BSD-3-Clause, copyright Sebastian Toyos). It runs at build time and is not linked into
  the shipped game.
- `miniz.txt`: the ZIP/inflate reader under `runtime/third_party/miniz/` (public domain /
  Unlicense, Rich Geldreich and contributors). It is compiled into the runtime to read the
  player's ROM archive.
- MAME and ymfm are used only as development references in `tools/core/*/reference/` and in
  local development traces. Their BSD-3-Clause notices are preserved in those directories.
  No MAME or ymfm code is linked into `ddonpach` or the SDL2 port.

No ROM, extracted graphics, sample data, or translated ROM-derived C is included in this
repository or in release archives. The player supplies `ddonpach.zip`.
