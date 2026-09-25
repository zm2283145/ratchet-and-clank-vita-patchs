$ErrorActionPreference = 'Stop'
$toolbin = if ($env:VITASDK) { Join-Path $env:VITASDK 'bin' } else { 'C:\vitasdk\bin' }
if (-not (Test-Path -LiteralPath (Join-Path $toolbin 'arm-vita-eabi-gcc.exe'))) {
    throw "VitaSDK not found at $toolbin. Set VITASDK to your VitaSDK directory."
}
Push-Location $PSScriptRoot
try {
    $buildRoot = Split-Path -Parent $PSScriptRoot
    $buildElf = Join-Path $buildRoot 'rc3_native_menu_lobby_test.build.elf'
    $buildVelf = Join-Path $buildRoot 'rc3_native_menu_lobby_test.build.velf'
    $buildSuprx = Join-Path $buildRoot 'rc3_native_menu_lobby_test.build.suprx'
    & "$toolbin\arm-vita-eabi-gcc.exe" -DRC3_LOBBY_TEST -O2 -Wall -Wextra -Werror -nostdlib '-Wl,-e,module_start' '-Wl,-q' -o $buildElf rc3_native_menu.c -ltaihen_stub -lSceLibc_stub_weak -lSceLibKernel_stub_weak -lSceKernelModulemgr_stub -lSceKernelThreadMgr_stub -lSceIofilemgr_stub -lSceCtrl_stub -lgcc
    if ($LASTEXITCODE -ne 0) { throw 'C compilation failed' }
    & "$toolbin\vita-elf-create.exe" -e exports_rc3_native_menu.yml $buildElf $buildVelf
    if ($LASTEXITCODE -ne 0) { throw 'Vita ELF conversion failed' }
    & "$toolbin\vita-make-fself.exe" $buildVelf $buildSuprx
    if ($LASTEXITCODE -ne 0) { throw 'suprx packaging failed' }
    $finalSuprx = Join-Path $buildRoot 'rc3_native_menu_lobby_test.suprx'
    Copy-Item -LiteralPath $buildSuprx -Destination $finalSuprx -Force
    Get-FileHash $finalSuprx -Algorithm SHA256
} finally {
    Pop-Location
}
