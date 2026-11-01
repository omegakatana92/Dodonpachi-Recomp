"""
Name:        extract_program.py
Description: writes the player's interleaved 68000 program image to build/program.bin for the
             development tools that inspect it
Version:     0.1.0
Created:     31-10-2026
Modified:    31-10-2026

Usage: python tools/extract_program.py [rom folder or .zip]

The image is derived from the player's own ROM and lives only under build/ (git-ignored); it is
never part of a release.
"""
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path[:0] = [str(TOOLS / "core"), str(TOOLS)]

import game_profile  # noqa: E402


def main():
    source = Path(sys.argv[1]) if len(sys.argv) > 1 else game_profile.ROOT / "rom"
    image = game_profile.rom.build_program(game_profile.rom.load_set(source))
    out = game_profile.ROOT / "build" / "program.bin"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(image)
    print(f"program image: {len(image)} bytes -> {out}")


if __name__ == "__main__":
    main()
