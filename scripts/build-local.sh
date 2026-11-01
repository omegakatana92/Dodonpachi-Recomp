#!/usr/bin/env bash
# Build the native port locally from the player's own ddonpach.zip.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ROM="${1:-$ROOT/rom/ddonpach.zip}"
cd "$ROOT"

# Apply the local toolchain fix recorded under patches/ if this checkout does not have it.
if git -C tools/core apply --check "$ROOT/patches/0001-m68000-translate-pea-return.patch" 2>/dev/null; then
    git -C tools/core apply "$ROOT/patches/0001-m68000-translate-pea-return.patch"
fi

python3 tools/verify_rom.py "$ROM"
python3 tools/extract_program.py "$ROM"
python3 tools/seed_symbols.py "$ROM"
python3 tools/seed_tables.py "$ROM"
python3 tools/seed_dispatch.py "$ROM"
python3 tools/disassemble.py "$ROM"
python3 tools/recompile.py "$ROM"

cmake -S . -B build/cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DDDP_ROM_FILE="$ROM" -DDDP_BUILD_SDL2=ON
cmake --build build/cmake --parallel

./build/cmake/bin/ddonpach_headless --rom "$ROM" --frames 600 --hashes build/boot.hashes
echo "playable binary: build/cmake/bin/DoDonPachi"
