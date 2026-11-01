"""
Name:        check_all.py
Description: runs the ROM verification, static analysis, translation and (optionally) the headless
             boot gate, and fails if any milestone acceptance condition is not met
Version:     0.1.0
Created:     31-10-2026
Modified:    31-10-2026

Usage: python tools/check_all.py [ROM] [--build] [--frames 600]

The ROM is the player's own. Nothing read from it is written outside build/ (git-ignored).
"""
import argparse
import subprocess
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
ROOT = TOOLS.parent


def run(command, capture=True):
    print("+", " ".join(str(part) for part in command))
    result = subprocess.run(command, cwd=ROOT, text=True,
                            stdout=subprocess.PIPE if capture else None,
                            stderr=subprocess.STDOUT if capture else None)
    if result.returncode != 0:
        if capture:
            print(result.stdout)
        raise SystemExit(f"command failed ({result.returncode}): {' '.join(map(str, command))}")
    return result.stdout or ""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("rom", nargs="?", default=str(ROOT / "rom" / "ddonpach.zip"))
    parser.add_argument("--build", action="store_true", help="configure, build and run the headless gate")
    parser.add_argument("--frames", type=int, default=600)
    args = parser.parse_args()
    rom = Path(args.rom)

    output = run([sys.executable, str(TOOLS / "verify_rom.py"), str(rom)])
    if "12/12 files" not in output:
        raise SystemExit("ROM verification did not find 12/12 files")

    run([sys.executable, str(TOOLS / "extract_program.py"), str(rom)])
    for tool in ("seed_symbols.py", "seed_tables.py", "seed_dispatch.py"):
        run([sys.executable, str(TOOLS / tool), str(rom)])

    run([sys.executable, str(TOOLS / "disassemble.py"), str(rom)])
    listing = (ROOT / "build" / "listing" / "ddonpach.s").read_text(encoding="utf-8", errors="replace")
    for address in ("00042A", "0052EA", "000A80", "05718E", "0571B2"):
        if address not in listing:
            raise SystemExit(f"static analysis did not reach {address}")
    report = (ROOT / "build" / "listing" / "report.txt").read_text(encoding="utf-8")
    functions = int(report.split("functions:")[1].split()[0].rstrip(","))
    if functions < 500:
        raise SystemExit(f"translation collapsed to {functions} functions")
    print(f"static analysis: {functions} functions; reset, main loop and frame wait present")

    output = run([sys.executable, str(TOOLS / "recompile.py"), str(rom)])
    if "not translated: 0" not in output:
        raise SystemExit("the translator reported unsupported instructions")
    if "Unsupported" in output:
        raise SystemExit("the translator raised Unsupported")
    print("translation: no unsupported instructions")

    if args.build:
        build_dir = ROOT / "build" / "cmake"
        if not (build_dir / "CMakeCache.txt").exists():
            if sys.platform == "win32":
                run(["cmake", "-S", str(ROOT), "-B", str(build_dir),
                     "-G", "Visual Studio 18 2026", "-A", "x64",
                     f"-DDDP_ROM_FILE={rom}"], capture=False)
            else:
                run(["cmake", "-S", str(ROOT), "-B", str(build_dir), "-G", "Ninja",
                     "-DCMAKE_BUILD_TYPE=Release", f"-DDDP_ROM_FILE={rom}"], capture=False)
        run(["cmake", "--build", str(build_dir), "--config", "Release", "--parallel"], capture=False)
        executable = build_dir / "bin" / ("ddonpach_headless.exe" if sys.platform == "win32" else "ddonpach_headless")
        run([str(executable), "--rom", str(rom), "--frames", str(args.frames),
             "--hashes", str(ROOT / "build" / "boot.hashes")], capture=False)
        print(f"headless boot: {args.frames} frames ran")

    print("all checks passed")


if __name__ == "__main__":
    main()
