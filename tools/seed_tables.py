"""
Name:        seed_tables.py
Description: adds the code entries of DoDonPachi's object-handler table to symbols.txt
Version:     0.1.0
Created:     31-10-2026
Modified:    31-10-2026

Usage: python tools/seed_tables.py [rom folder or .zip]

DoDonPachi dispatches entities through a table of 8-byte records at 0x015A8A; field +4 of each
record is the handler's address. The static analysis can follow the table only when symbols.txt
describes it, and the table's length is not known without reading the ROM. This tool reads the
player's own ROM and writes addresses (no game data) for every handler the table holds.

The block between the markers is regenerated; the rest of symbols.txt is left alone.
"""
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path[:0] = [str(TOOLS / "core"), str(TOOLS)]

import game_profile  # noqa: E402
from m68000 import disassembler as dis  # noqa: E402

START = "# --- object handlers (tools/seed_tables.py) ---"
END = "# --- end object handlers ---"
TABLE = 0x015A8A
RECORD = 8
FIELD = 4
MAX_RECORDS = 0x110


def main():
    source = Path(sys.argv[1]) if len(sys.argv) > 1 else game_profile.ROOT / "rom"
    image = game_profile.rom.build_program(game_profile.rom.load_set(source))
    mem = dis.FlatMemory(image)

    entries = []
    seen = set()

    def add(address, name):
        if address in seen or not (0x400 <= address < len(image)) or (address & 1):
            return
        if dis.decode(mem, address).flow == dis.FLOW_INVALID:
            return
        seen.add(address)
        entries.append(f"{address:06X}  code  {name}_{address:06X}")

    for index in range(1, MAX_RECORDS):
        record = TABLE + index * RECORD
        handler = mem.r32(record + FIELD) & 0xFFFFFF
        # Only records with a plausible handler are object records; others are data tables
        # whose first field is not a word-prefixed code block.
        if not (0x400 <= handler < len(image)) or (handler & 1):
            continue
        if dis.decode(mem, handler).flow == dis.FLOW_INVALID:
            continue
        prefix = mem.r32(record) & 0xFFFFFF
        if 0x400 <= prefix < len(image) and mem.r16(prefix) <= 0x100:
            # The dispatcher at 048A88 loads field +0, reads one word from it, and calls the
            # address after that word: the field points at a word-prefixed code block.
            add(prefix + 2, "prefix")
        # sub_048B06 jumps through field +4.
        add(handler, "handler")

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
    print(f"object handlers: {len(entries)} code entries seeded into {path.name}")


if __name__ == "__main__":
    main()
