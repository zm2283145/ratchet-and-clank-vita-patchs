$ErrorActionPreference = 'Stop'
$toolbin = if ($env:VITASDK) { Join-Path $env:VITASDK 'bin' } else { 'C:\vitasdk\bin' }
if (-not (Test-Path -LiteralPath (Join-Path $toolbin 'arm-vita-eabi-gcc.exe'))) {
    throw "VitaSDK not found at $toolbin. Set VITASDK to your VitaSDK directory."
}
$source = Join-Path $PSScriptRoot 'rc1_music_fix.c'
$elf = Join-Path $PSScriptRoot 'rc1_music_fix.elf'
$velf = Join-Path $PSScriptRoot 'rc1_music_fix.velf'
$suprx = Join-Path $PSScriptRoot 'rc1_music_fix.suprx'
$exports = Join-Path $PSScriptRoot 'exports.yml'

& "$toolbin\arm-vita-eabi-gcc.exe" -O2 -Wall -Wextra -Werror -fno-strict-aliasing -nostartfiles '-Wl,-e,module_start' '-Wl,-q' -o $elf $source -ltaihen_stub -lSceKernelModulemgr_stub
if ($LASTEXITCODE -ne 0) { throw 'C compilation failed' }
& "$toolbin\vita-elf-create.exe" -e $exports $elf $velf
if ($LASTEXITCODE -ne 0) { throw 'Vita ELF conversion failed' }
& "$toolbin\vita-make-fself.exe" $velf $suprx
if ($LASTEXITCODE -ne 0) { throw 'suprx packaging failed' }
Get-FileHash $suprx -Algorithm SHA256
