"""
Name:        seed_dispatch.py
Description: finds PC-relative jump/call tables in DoDonPachi's program and seeds their entries
Version:     0.1.0
Created:     31-10-2026
Modified:    31-10-2026

Usage: python tools/seed_dispatch.py [rom folder or .zip]

A 68000 `jmp (d16,PC,Dn.w)` or `jsr (d16,PC,Dn.w)` is a table of handler entry points. The static
analysis cannot follow the index register through it, so this tool finds the instruction, computes
the table base, and seeds every entry that decodes as a branch or jump. Only addresses and names
are written to symbols.txt; no game data is copied.
"""
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path[:0] = [str(TOOLS / "core"), str(TOOLS)]

import game_profile  # noqa: E402
from m68000 import disassembler as dis  # noqa: E402

START = "# --- PC-relative dispatch tables (tools/seed_dispatch.py) ---"
END = "# --- end PC-relative dispatch tables ---"
CODE_START = getattr(game_profile, "CODE_START", 0x400)
PROGRAM_SIZE = game_profile.rom.PROGRAM_SIZE
JMP_PC_INDEX = 0x4EFB
JSR_PC_INDEX = 0x4EBB


def is_entry(mem, address):
    """A table entry is a branch or a jump instruction."""
    if not (CODE_START <= address < PROGRAM_SIZE) or address & 1:
        return False
    opcode = mem.r16(address)
    if 0x6000 <= opcode <= 0x60FF:          # BRA/Bcc.W
        return True
    if opcode in (0x4EF9, 0x4EFA, 0x4EFB, 0x4EBB, 0x4EB9, 0x4EBA):
        return True
    return False


def main():
    source = Path(sys.argv[1]) if len(sys.argv) > 1 else game_profile.ROOT / "rom"
    image = game_profile.rom.build_program(game_profile.rom.load_set(source))
    mem = dis.FlatMemory(image)

    found = set()
    for address in range(CODE_START, PROGRAM_SIZE - 6, 2):
        opcode = mem.r16(address)
        if opcode not in (JMP_PC_INDEX, JSR_PC_INDEX):
            continue
        extension = mem.r16(address + 2)
        # Brief extension word: bit 8 clear, an 8-bit signed displacement and an index register.
        # (The full extension format is not used by these tables.)
        if extension & 0x0100:
            continue
        displacement = (extension & 0xFF) ^ 0x80
        displacement -= 0x80
        base = (address + 2 + displacement) & 0xFFFFFF
        if not (CODE_START <= base < PROGRAM_SIZE):
            continue
        index = 0
        while index < 64:
            entry = (base + index * 4) & 0xFFFFFF
            if is_entry(mem, entry):
                found.add(entry)
                index += 1
                continue
            # Some tables pad each 4-byte branch to an 8-byte record with four zero bytes.
            if index + 1 < 64 and is_entry(mem, (base + (index + 1) * 4) & 0xFFFFFF):
                index += 1
                continue
            break

    entries = [f"{address:06X}  code  dispatch_{address:06X}" for address in sorted(found)]
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
    path.write_text("\n".join(kept + ["", START] + entries + [END, ""]), encoding="utf-8")
    print(f"dispatch entries: {len(entries)} seeded into {path.name}")


if __name__ == "__main__":
    main()
