$ErrorActionPreference = 'Stop'
$toolbin = if ($env:VITASDK) { Join-Path $env:VITASDK 'bin' } else { 'C:\vitasdk\bin' }
if (-not (Test-Path -LiteralPath (Join-Path $toolbin 'arm-vita-eabi-gcc.exe'))) {
    throw "VitaSDK not found at $toolbin. Set VITASDK to your VitaSDK directory."
}
Push-Location $PSScriptRoot
try {
    & "$toolbin\arm-vita-eabi-gcc.exe" -std=gnu11 -O2 -Wall -Wextra -Werror -fno-strict-aliasing -nostdlib '-Wl,-e,module_start' '-Wl,-q' -o rc1_audio_fixes.elf rc1_audio_fixes.c -ltaihen_stub -lSceLibc_stub_weak -lSceLibKernel_stub_weak -lSceKernelModulemgr_stub -lSceKernelThreadMgr_stub -lSceCtrl_stub -lgcc
    if ($LASTEXITCODE -ne 0) { throw 'C compilation failed' }
    & "$toolbin\vita-elf-create.exe" -e exports_audio_fixes.yml rc1_audio_fixes.elf rc1_audio_fixes.velf
    if ($LASTEXITCODE -ne 0) { throw 'Vita ELF conversion failed' }
    & "$toolbin\vita-make-fself.exe" rc1_audio_fixes.velf rc1_audio_fixes.suprx
    if ($LASTEXITCODE -ne 0) { throw 'suprx packaging failed' }
    Get-FileHash .\rc1_audio_fixes.suprx -Algorithm SHA256
} finally {
    Pop-Location
}
