# Build the native port locally from the player's own ddonpach.zip.
param(
    [string]$Rom = "rom\ddonpach.zip"
)
$ErrorActionPreference = "Stop"
Set-Location (Split-Path -Parent $PSScriptRoot)

# Apply the local toolchain fix recorded under patches/ if this checkout does not have it.
$toolchainPatch = Join-Path (Get-Location) 'patches\0001-m68000-translate-pea-return.patch'
git -C tools\core apply --check $toolchainPatch 2>$null
if ($LASTEXITCODE -eq 0) {
    git -C tools\core apply $toolchainPatch
}

python tools\verify_rom.py $Rom
python tools\extract_program.py $Rom
python tools\seed_symbols.py $Rom
python tools\seed_tables.py $Rom
python tools\seed_dispatch.py $Rom
python tools\disassemble.py $Rom
python tools\recompile.py $Rom

cmake -S . -B build\cmake -G "Visual Studio 18 2026" -A x64 `
      -DDDP_ROM_FILE="$Rom" -DDDP_BUILD_SDL2=ON
cmake --build build\cmake --config Release --parallel 8

build\cmake\bin\ddonpach_headless.exe --rom $Rom --frames 600 --hashes build\boot.hashes
Write-Host "playable binary: build\cmake\bin\DoDonPachi-Windows-x64.exe"
