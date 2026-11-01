"""
Name:        ddp_rom.py
Description: MAME 'ddonpach' ROM set (DoDonPachi, World, 1997 2/5 Master Ver.) and its regions
Version:     0.1.0
Created:     31-10-2026
Modified:    31-10-2026

Files are identified by CRC32 + SHA-1, never by name or by the folder inside the archive.
Region layouts follow ROM_START( ddonpach ) in MAME src/mame/atlus/cave.cpp
(BSD-3-Clause, copyright holders David Haywood, Luca Elia and others).

This module describes data, not the game: no ROM bytes live here.
"""
import hashlib
import zipfile
import zlib
from pathlib import Path

SET_NAME = "ddonpach"
SET_TITLE = "DoDonPachi (World, 1997 2/5 Master Ver.)"

# MAME name -> (crc32, sha1, size)
ROM_FILES = {
    "b1.u27": ("b5cdc8d3", "58757b50e21a27e500a82c03f62cf02a85389926", 0x080000),
    "b2.u26": ("6bbb063a", "e5de64b9c3efc0a38a2e0e16b78ee393bff63558", 0x080000),
    "u50.bin": ("14b260ec", "33bda210302428d5500115d0c7a839cdfcb67d17", 0x200000),
    "u51.bin": ("e7ba8cce", "ad74a6b7d53760b19587c4a6dbea937daa7e87ce", 0x200000),
    "u52.bin": ("02492ee0", "64d9cc64a4ad189a8b03cf6a749ddb732b4a0014", 0x200000),
    "u53.bin": ("cb4c10f0", "a622e8bd0c938b5d38b392b247400b744d8be288", 0x200000),
    "u60.bin": ("903096a7", "a243e903fef7c4a7b71383263e82e42acd869261", 0x200000),
    "u61.bin": ("d89b7631", "a66bb4955ca58fab8973ca37a0f971e9a67ce017", 0x200000),
    "u62.bin": ("292bfb6b", "11b385991ee990eb5ef36e136b988802b5f90fa4", 0x200000),
    "u6.bin": ("9dfdafaf", "f5cb450cdc78a20c3a74c6dac05c9ac3cba08327", 0x200000),
    "u7.bin": ("795b17d5", "cbfc29f1df9600c82e0fdae00edd00da5b73e14c", 0x200000),
    "eeprom-ddonpach.bin": ("315fb546", "7f597107d1610fc286413e0e93c794c80c0c554f", 0x000080),
}

# The 68000 program image: 0x000000-0x0FFFFF, big endian (b1 = even bytes, b2 = odd bytes).
PROGRAM_AREAS = [(0x000000, 0x100000)]
PROGRAM_SIZE = 0x100000

# Where the board puts each region (MAME cave.cpp memory map).
WORK_RAM = (0x100000, 0x110000)
SPRITE_RAM = (0x400000, 0x410000)
LAYER_RAM = [(0x500000, 0x508000), (0x600000, 0x608000), (0x700000, 0x704000)]
PALETTE_RAM = (0xC00000, 0xC10000)


def read_files(source):
    """Return {base name: bytes} from a folder or a .zip.

    Archives that hold the set in a directory (``ddonpach/b1.u27``) are accepted: only the base
    name is significant. Duplicate base names are an error; ROMs are matched by content anyway.
    """
    source = Path(source)
    contents = {}
    if source.is_file() and source.suffix.lower() == ".zip":
        with zipfile.ZipFile(source) as archive:
            for entry in archive.infolist():
                if entry.is_dir():
                    continue
                name = Path(entry.filename).name
                if name in contents:
                    raise SystemExit(f"{source}: two entries named {name}")
                contents[name] = archive.read(entry)
        return contents
    if source.is_dir():
        for path in sorted(source.iterdir()):
            if path.is_file() and path.suffix.lower() != ".zip":
                contents[path.name] = path.read_bytes()
        return contents
    raise SystemExit(f"{source} is not a ROM folder or .zip")


def identify_set(source):
    """Return ({mame_name: bytes}, [report lines]).

    Every file is matched by CRC32 and SHA-1, so renamed files and archives with a folder
    prefix still verify. A folder that holds no loose files is searched for a .zip that does.
    """
    found, report = _identify(source)
    if not found and Path(source).is_dir():
        for archive in sorted(Path(source).glob("*.zip")):
            found, report = _identify(archive)
            if found:
                break
    return found, report


def _identify(source):
    by_crc = {crc: name for name, (crc, _, _) in ROM_FILES.items()}
    found = {}
    report = []
    for original_name, data in read_files(source).items():
        crc = format(zlib.crc32(data) & 0xFFFFFFFF, "08x")
        mame_name = by_crc.get(crc)
        if mame_name and ROM_FILES[mame_name][1] == hashlib.sha1(data).hexdigest():
            found[mame_name] = data
            report.append(f"OK      {original_name:24} = {mame_name}")
        else:
            report.append(f"--      {original_name:24} not part of the set (crc {crc})")
    for mame_name in sorted(set(ROM_FILES) - set(found)):
        report.append(f"MISSING {mame_name}")
    return found, report


def load_set(source):
    """Like identify_set, but stops when a file is missing or a hash does not match."""
    found, report = identify_set(source)
    missing = sorted(set(ROM_FILES) - set(found))
    if missing:
        raise SystemExit("The 'ddonpach' ROM set is incomplete. Missing: " + ", ".join(missing)
                         + "\n" + "\n".join(report))
    for name, data in found.items():
        if len(data) != ROM_FILES[name][2]:
            raise SystemExit(f"{name}: {len(data)} bytes, the set has {ROM_FILES[name][2]}")
    return found


def interleave16(even, odd):
    """ROM_LOAD16_BYTE: even bytes from one chip, odd bytes from the other."""
    result = bytearray(len(even) * 2)
    result[0::2] = even
    result[1::2] = odd
    return bytes(result)


def build_program(rom):
    """maincpu region (1 MB, big endian) as the 68000 sees it at 0x000000-0x0FFFFF."""
    return interleave16(rom["b1.u27"], rom["b2.u26"])


def build_sprites(rom):
    """sprites0 region (8 MB), as MAME loads it: ROM_LOAD16_WORD_SWAP of u50-u53."""
    data = bytearray(b"".join(rom[name] for name in ("u50.bin", "u51.bin", "u52.bin", "u53.bin")))
    data[0::2], data[1::2] = data[1::2], data[0::2]
    return bytes(data)


def build_layer(rom, index):
    """A tile layer's graphics in file byte order (2 MB)."""
    return rom[("u60.bin", "u61.bin", "u62.bin")[index]]


def build_ymz(rom):
    """YMZ280B sample ROM (4 MB)."""
    return rom["u6.bin"] + rom["u7.bin"]


def build_eeprom(rom):
    """93C46 serial EEPROM image (128 bytes)."""
    return rom["eeprom-ddonpach.bin"]


def canonical_address(address):
    """Map a 68000 address to its offset in the program image, or None if it is not ROM.

    The 68000 drives 24 address lines: the top byte of an address is not decoded.
    """
    address &= 0xFFFFFF
    return address if address < PROGRAM_SIZE else None


class ProgramMemory:
    """68000 view of the program ROM for the analysis: big endian, anything outside reads 0xFF."""

    def __init__(self, image):
        self.image = image

    def r8(self, address):
        offset = canonical_address(address)
        return self.image[offset] if offset is not None else 0xFF

    def r16(self, address):
        return (self.r8(address) << 8) | self.r8(address + 1)

    def r32(self, address):
        return (self.r16(address) << 16) | self.r16(address + 2)
