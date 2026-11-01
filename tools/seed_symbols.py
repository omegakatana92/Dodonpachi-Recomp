"""
Name:        seed_symbols.py
Description: adds the program's jump instructions as code symbols, so the static analysis starts
             from every JUMP and follows where it goes
Version:     0.1.0
Created:     31-10-2026
Modified:    31-10-2026

Usage: python tools/seed_symbols.py [rom folder or .zip]

The 68000 program keeps handler tables of JMP instructions (a slot per entity type, one jump to
the routine that handles it), and the analysis cannot read them as tables of addresses: a JMP is
code, not a pointer. Seeding one code symbol per JMP instruction lets the analysis follow every
target, which is how the handlers of DoDonPachi's entity dispatch (0043A-00621) and the game's
other jump tables are found.

Only addresses and names are written to symbols.txt: no game data. The block between the markers
is regenerated on each run; the rest of the file is left alone.
"""
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path[:0] = [str(TOOLS / "core"), str(TOOLS)]

import game_profile  # noqa: E402
from m68000 import disassembler as dis  # noqa: E402

START = "# --- jump instructions (tools/seed_symbols.py) ---"
END = "# --- end jump instructions ---"
JMP_ABSOLUTE_SHORT = 0x4EF8
JMP_ABSOLUTE_LONG = 0x4EF9
JMP_PC_DISPLACEMENT = 0x4EFA
JMP_PC_INDEX = 0x4EFB
CODE_START = getattr(game_profile, "CODE_START", 0x400)
PROGRAM_SIZE = game_profile.rom.PROGRAM_SIZE


def jump_targets(mem, program):
    """(address, target) of every JMP in the program that goes to a place inside it"""
    for address in range(CODE_START, PROGRAM_SIZE - 6, 2):
        opcode = mem.r16(address)
        if opcode == JMP_ABSOLUTE_LONG:
            target = mem.r32(address + 2) & 0xFFFFFF
            length = 6
        elif opcode == JMP_ABSOLUTE_SHORT:
            target = (mem.r16(address + 2) ^ 0x8000) - 0x8000
            target &= 0xFFFFFF
            length = 4
        elif opcode == JMP_PC_DISPLACEMENT:
            target = (address + 2 + (mem.r16(address + 2) ^ 0x8000) - 0x8000) & 0xFFFFFF
            length = 4
        else:
            continue
        if not CODE_START <= target < PROGRAM_SIZE or target % 2:
            continue
        if dis.decode(mem, target).flow == dis.FLOW_INVALID:
            continue
        yield address, target


def main():
    source = Path(sys.argv[1]) if len(sys.argv) > 1 else game_profile.ROOT / "rom"
    image = game_profile.rom.build_program(game_profile.rom.load_set(source))
    mem = dis.FlatMemory(image)

    found = sorted(set(jump_targets(mem, image)))
    entries = [f"{address:06X}  code  jump_{address:06X}" for address, _ in found]

    path = game_profile.ROOT / "symbols.txt"
    lines = path.read_text(encoding="utf-8").splitlines()
    kept, inside = [], False
    for line in lines:
        if line.strip() == START:
            inside = True
            continue
        if line.strip() == END:
            inside = False
            continue
        if not inside:
            kept.append(line)
    while kept and not kept[-1].strip():
        kept.pop()
    text = "\n".join(kept + ["", START] + entries + [END, ""])
    path.write_text(text, encoding="utf-8")

    targets = len({target for _, target in found})
    print(f"jump instructions: {len(found)} ({targets} distinct targets) seeded into {path.name}")


if __name__ == "__main__":
    main()
