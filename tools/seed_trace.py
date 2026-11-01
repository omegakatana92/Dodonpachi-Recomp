"""
Name:        seed_trace.py
Description: seeds code symbols from a MAME debugger execution trace (development reference only)
Version:     0.1.0
Created:     31-10-2026
Modified:    31-10-2026

Usage: python tools/seed_trace.py TRACE.txt [--listing build/listing/ddonpach.s]

MAME's debugger `trace` command writes one line per executed instruction. This tool collects the
unique program counters, compares them with the addresses the static analysis already decoded, and
adds a `code` symbol for each one that is missing. Only addresses are written to symbols.txt.

A trace is a development aid: MAME is never linked into or shipped with the port.
"""
import re
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
ROOT = TOOLS.parent
START = "# --- execution trace gaps (tools/seed_trace.py) ---"
END = "# --- end execution trace gaps ---"
LINE = re.compile(r"^([0-9A-Fa-f]{6})(?::|\s)")


def listing_addresses(path):
    addresses = set()
    if not path.exists():
        return addresses
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = LINE.match(line.strip())
        if match:
            addresses.add(int(match.group(1), 16))
    return addresses


def main():
    if len(sys.argv) < 2:
        raise SystemExit("usage: python tools/seed_trace.py TRACE.txt [--listing LISTING]")
    trace = Path(sys.argv[1])
    listing = Path(sys.argv[3]) if len(sys.argv) > 3 and sys.argv[2] == "--listing" else \
        ROOT / "build" / "listing" / "ddonpach.s"
    known = listing_addresses(listing)

    executed = set()
    with trace.open("r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            match = LINE.match(line)
            if match:
                executed.add(int(match.group(1), 16))

    missing = sorted(address for address in executed if address not in known and 0x400 <= address < 0x100000)
    entries = [f"{address:06X}  code  trace_{address:06X}" for address in missing]

    path = ROOT / "symbols.txt"
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
    print(f"trace: {len(executed)} distinct PCs, {len(known)} decoded, {len(missing)} missing")
    print(f"seeded {len(entries)} addresses into {path.name}")


if __name__ == "__main__":
    main()
