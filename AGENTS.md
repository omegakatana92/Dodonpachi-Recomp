# DoDonPachi Recomp — OpenCode agent instructions

## Goal
Develop a genuinely native, ROM-dependent DoDonPachi (1997) port with Windows x64 executable and Linux x86_64 Steam Deck AppImage deliverables. Project status: **research/scaffold; no working game build is claimed**.

Upstream tooling: https://github.com/cheviman/arcade-decomp
Local development ROM: `C:\Users\omega\Downloads\ddonpach.zip`
Target GitHub repository: https://github.com/omegakatana92/Dodonpachi-Recomp

## First milestone
1. Inspect this repository and preserve existing history and LICENSE. The existing LICENSE contains GPL-2.0 text; review licensing implications before introducing dependencies or changing it.
2. Read arcade-decomp README and inspect its 68000 frontend, disassembler, game profile, ROM verifier, reference comparison and shared code. Incorporate it as a pinned `tools/core` Git submodule when appropriate; don't copy its code blindly.
3. Check that the local ROM archive exists; list entries and check sizes, CRC32 and SHA-1 against the appropriate MAME set definition. Do not assume the ROM is valid from filename alone.
4. Research the DoDonPachi Cave board's program layout, 68000 memory mapping, Cave video tilemap/sprite hardware, palette, interrupts, EEPROM and YMZ280B audio using MAME as a reference. Existing arcade-decomp support for other boards does not imply it supports Cave.
5. Build a ROM loading/verification program and automated tests as the first executable milestone. Distinguish passing ROM validation from actual native gameplay.
6. Report work with changed files, exact test commands and results, blockers, and next milestone. Work incrementally, keeping the repository buildable.

## Implementation
Implement Cave-specific runtime and board peripherals. Extend 68000 decompilation analysis and produce native C/C++ only for verified supported code. Compare deterministic frames/state/audio to a suitable reference. Never represent an emulator wrapper as a native recompilation.
Keep shared portable code, CMake, SDL-based interface where suitable; implement keyboard, controller and Steam Input mappings, aspect-preserving vertical gameplay rotation and audio/timing accuracy.

## Outputs and release criteria
- `DoDonPachi-Windows-x64.exe` built and tested on Windows 11.
- `DoDonPachi-SteamDeck-x86_64.AppImage` built on a compatible Linux environment and validated on SteamOS, including Desktop/Gaming Mode launch.
- GitHub Actions build workflows only after corresponding code can build; label experimental artifacts clearly.
- Release only when functionally tested; document incomplete systems. Do not fabricate successful builds or tests.

## Copyright and licensing
Never commit, upload or package `ddonpach.zip`, ROM data, copyrighted graphics/audio extracted from it, or game BIOS. Use local configurable ROM paths at runtime. Preserve attribution and all relevant licenses. Do not relicense upstream BSD-3-Clause or MAME components. Keep reference-only MAME code out of distributions unless applicable licenses and reuse have been reviewed. Existing repository GPL-2.0 LICENSE must not be silently replaced with PolyForm.

## Git workflow
Use the existing `main` branch on `omegakatana92/Dodonpachi-Recomp`; do not create a duplicate repository. Review `git status`, exclude ROMs/logs/build artifacts, commit tested milestones with descriptive messages, then push using the user's configured GitHub credentials. Do not force-push or rewrite history.
