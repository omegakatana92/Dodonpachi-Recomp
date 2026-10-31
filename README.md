# DoDonPachi Recomp

Experimental, ROM-dependent native-port research project for **DoDonPachi (1997)**. Target platforms: **Windows 11 x64** and **Steam Deck Linux x86_64 (AppImage)**.

**Status:** Project scaffolding only. No gameplay, native port, Windows executable or Steam Deck AppImage has been built or tested here.

## Development
Tooling reference: [cheviman/arcade-decomp](https://github.com/cheviman/arcade-decomp). Cave arcade hardware support will require additional game-specific implementation.

OpenCode agents should read [AGENTS.md](AGENTS.md). The development ROM is expected locally at `C:\Users\omega\Downloads\ddonpach.zip` and must **never** be committed or uploaded.

## Goals
1. Validate the local ROM set and audit Cave hardware compatibility.
2. Implement a Cave reference/runtime and native code analysis.
3. Produce and test Windows x64 builds.
4. Produce and test a Steam Deck-compatible Linux AppImage.
5. Distribute source and ROM-free builds only, with applicable license notices.

This repository already contains a GPL-2.0 LICENSE; any licensing changes require review and must preserve third-party obligations.
