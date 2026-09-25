# Ratchet & Clank 3 Vita multiplayer restoration

This directory preserves the in-progress effort to restore the PS3 local
multiplayer path in the USA PS Vita release of *Ratchet & Clank 3*
(`PCSA00133`) with a taiHEN user plugin.

The immediate target is a one-player local Siege match on Metropolis (level
44), with no time limit and all weapons, launched from the restored native
**Multiplayer -> Local Play** menu. This is a research snapshot, not a finished
mod or release build.

## Current status

The current plugin source is the v87 hardware-test build from September 25,
2026. Its source SHA-256 is:

```text
573ADF8E4A2582AE5FE876ECF1917F47F32872BFE9E7E268E0EB0694CDDC4492
```

The corresponding deployed `.suprx` had SHA-256:

```text
78409C801194A7DD6FDCBA8CF76CB3563F42B935EE884163E36F2076B957FAE6
```

What works:

- The native five-entry title menu is restored: Load Game, New Game,
  Multiplayer, Multiplayer Tutorial, and Options.
- Multiplayer and Local Play are presented through retained native UI code.
- The state-39 multiplayer background/loading path is usable.
- Converted multiplayer levels 39 through 55 can be loaded through the
  loose-file overlay. Those converted game files are not distributed here.
- Metropolis level 44 reaches its retained gameplay scheduler instead of
  remaining in the stripped multiplayer staging state.
- One local player is registered through the retained Vita session helpers.
- The plugin spawns a loaded multiplayer skin Moby, binds it to the retained
  Vita player state, seeds the Metropolis spawn transform, and advances player
  and camera data from controller updates.
- Crash guards and allocator repairs cover the stripped ownership paths
  encountered so far.
- The current build synchronously traces the retained Moby draw-entry builder
  at Vita address `0x8117E73A`, including its PVS, frustum, distance, and
  emitted-entry decisions.

What remains:

- The local Hero Moby is bound and moving but is not visible in the steady
  render pass. The current diagnostic is isolating the exact pass-dependent
  rejection.
- Hero action/animation ownership is incomplete, and a metal-footstep sound
  loops while the placeholder state runs.
- The temporary camera bridge must be replaced with proper retained
  third-person camera ownership.
- Siege rules, no-time-limit state, inventory/all-weapons setup, HUD,
  gameplay music, teams, and bots are not complete.

## Important technical findings

- The Vita executable is Thumb-2 with text at `0x81000000` and data at
  `0x817FA000`; the tested retail revision has text file size `0x7F93C4`.
- The PS3 PPU reference is big-endian PPC64 code with 32-bit game pointers.
- The Vita player slot begins at `0x819D4B80`, stride `0x460`.
  `slot+0x240` is the Hero Moby.
- The retained Vita player state begins at `0x819DE900`, stride `0x780`.
  `player_state+0x140` aliases the same Hero Moby.
- The retained Vita Moby spawner is `0x811789C4`.
- PS3 multiplayer uses skin classes rather than oClass 0. Class `0x171C` is
  the current test skin and is present in the converted Metropolis data.
- The retained draw-entry builder is `0x8117E73A`. Accepted entries are
  `0x2C` bytes and contain the Moby pointer at `entry+0x28`.
- The plugin intentionally keeps Vita's `0x780` player state as canonical
  local-player ownership. It does not attempt to port the stripped PS3
  networking stack or C++ Hero/session architecture.

See [`docs/`](docs/) for earlier menu and loader probes. Those documents are
historical and may describe superseded test builds; this README and
`plugin/rc3_native_menu.c` represent the current state.

## Directory layout

```text
rc3-multiplayer-restoration/
  plugin/                  Current taiHEN plugin source and build metadata
  tools/                   Static analysis and level-conversion scripts
    Replanetizer/          Complete locally modified GPL Replanetizer snapshot
  docs/                    Earlier RC3 menu/loader investigation notes
```

## Requirements

### Plugin development and hardware testing

- A Windows PC with PowerShell.
- [VitaSDK](https://vitasdk.org/) with taiHEN and the Vita system-library
  stubs used by the build script.
- A taiHEN-capable PS Vita and VitaShell.
- `ioplus.skprx` for the loose-file override layer.
- `curl.exe` for FTP upload/download verification.
- A legally obtained USA `PCSA00133` installation.

### Static analysis

- Python 3.11 or 3.12.
- The packages in [`tools/requirements.txt`](tools/requirements.txt).
- A legally obtained/decrypted Vita `rc3.self.elf`.
- A legally obtained PS3 `RC3_PPU.elf` from the PS3 collection.
- [vita-parse-core](https://github.com/xyzz/vita-parse-core) at commit
  `644b5f081c5f3c9b205180793ab8f4209dfd9d97` for Vita crash-dump parsing.
  See [`tools/VITA_PARSE_CORE_NOTES.md`](tools/VITA_PARSE_CORE_NOTES.md) for
  the local Python 3 compatibility changes used during this research.

Useful clean reference repositories:

- [Metroynome/librac1](https://github.com/Metroynome/librac1), tested at
  `6c5477d7778ea494ba774a1e2ae676b4a6efedb9`.
- [VELD-Dev/Pyrocitor](https://github.com/VELD-Dev/Pyrocitor), tested at
  `3892379cb46e0598c77a3541c7eb36dc04c809b2`.

### Level conversion

- .NET 8 SDK.
- The modified Replanetizer source under `tools/Replanetizer`.
- Python 3 for the RC3 conversion/validation scripts.
- Level files extracted from legally owned PS3 and Vita copies.

The Replanetizer snapshot is based on upstream commit
`c0533f25186466d7f29d04bd64a81d641b38fdf6`. Its local changes add the
mixed/little-endian and split-engine serialization required by the RC3 Vita
experiments. See
[`tools/Replanetizer/LOCAL_CHANGES.md`](tools/Replanetizer/LOCAL_CHANGES.md).

## Build the plugin

From this directory:

```powershell
Set-Location .\plugin
.\build_rc3_native_menu_lobby_test.ps1
```

Set `VITASDK` if the SDK is not installed at `C:\vitasdk`. The build uses
`-DRC3_LOBBY_TEST -O2 -Wall -Wextra -Werror` and writes
`rc3_native_menu_lobby_test.suprx` one directory above `plugin`.

This experimental plugin must not be installed alongside older RC3 menu or
level-load probes that hook the same routines.

## Static-analysis examples

Install the Python dependencies:

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r .\tools\requirements.txt
```

Disassemble a Vita Thumb range:

```powershell
.\.venv\Scripts\python.exe .\tools\vita_disasm.py `
  D:\legal-inputs\rc3.self.elf 0x8117E73A 0x8117E950
```

Disassemble a PS3 PPU range:

```powershell
.\.venv\Scripts\python.exe .\tools\ps3_disasm.py `
  D:\legal-inputs\RC3_PPU.elf 0x3474C8 0x347B80
```

Find Vita references to an address:

```powershell
.\.venv\Scripts\python.exe .\tools\vita_address_xrefs.py `
  D:\legal-inputs\rc3.self.elf 0x81B8F2A4
```

Trace PS3 PPU references:

```powershell
.\.venv\Scripts\python.exe .\tools\ps3_ppu_xrefs.py `
  D:\legal-inputs\RC3_PPU.elf --address 0x00AFA004
```

The dump helpers expect `vita-parse-core` at `tools\vita-parse-core`:

```powershell
python .\tools\dump_vita_core_memory.py `
  D:\legal-inputs\crash.psp2dmp 0x819D4B80 0x460 player-slot.bin
```

## Level-conversion workflow

Build the local Replanetizer snapshot:

```powershell
Set-Location .\tools\Replanetizer
dotnet build .\Replanetizer.sln
```

The local serializer recognizes:

```powershell
$env:RC3_VITA_LITTLE_ENDIAN = '1'
$env:RC3_VITA_SPLIT_ENGINE = '1'
dotnet run --project .\Replanetizer\Replanetizer.csproj
```

`RC3_VITA_LITTLE_ENDIAN` enables little-endian numeric serialization.
`RC3_VITA_SPLIT_ENGINE` emits the Vita-style `engine.ps3` plus
`engine_vert.ps3` split for RC3. Replanetizer output is then passed through the
scripts in `tools/` to preserve mixed-endian and platform-specific sections.

For example:

```powershell
python .\tools\rc3_vita_gameplay_converter.py `
  D:\legal-inputs\ps3\level44\gameplay_ntsc `
  D:\work\level44\gameplay_ntsc `
  --typed-gameplay D:\work\replanetizer\level44\gameplay_ntsc `
  --reference-vita D:\legal-inputs\vita\level1\gameplay_ntsc `
  --report D:\work\level44\gameplay-report.json
```

The converter and validators are research tools, not a general-purpose
one-click level port. Their reports must be reviewed before hardware testing.

## Vita deployment

Copy the built plugin to:

```text
ur0:tai/rc3_restored_menu_test.suprx
```

Add it under the title section:

```text
*PCSA00133
ur0:tai/rc3_restored_menu_test.suprx
```

Converted files are loaded from:

```text
ux0:data/rc_override/rc3/<path inside rc3.psarc>
```

The test log is:

```text
ux0:data/rc3mp/native_menu.log
```

RC3 must be fully closed and relaunched after replacing the plugin. Hardware
tests used passive FTP at `10.1.1.217:1337` with
`curl.exe --disable-epsv`; always download the uploaded file and compare its
SHA-256 before testing.

## Files intentionally not included

This repository does **not** contain:

- Vita or PS3 executable files.
- PSARC archives or extracted game files.
- Converted multiplayer levels.
- Textures, models, audio, movies, or other game assets.
- Vita crash dumps or raw memory captures.
- Hardware logs, screenshots, FTP configuration, or built `.suprx` files.
- Pickled disassembly generated from copyrighted executables.

You must supply inputs from copies you legally own. Do not open an issue asking
for game files or converted multiplayer assets.

## Licensing and attribution

Replanetizer is distributed in its own subtree under GPL-3.0-or-later; its
original `LICENSE.md`, attribution, and history reference are preserved.
Other third-party projects are linked rather than copied. No license is
granted for proprietary Ratchet & Clank game content, and none is included.
