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

## Exact game files required

The multiplayer maps are **not present in the Vita release**. They must be
extracted from the PS3 version of *Ratchet & Clank 3* and converted to Vita
formats. Renaming or directly copying the PS3 files is not sufficient: PS3
data is big-endian, PS3 keeps vertex data inside `engine.ps3`, and Vita
requires a separate `engine_vert.ps3` plus different engine, texture, and VRAM
layouts.

### Minimum files for the current Metropolis test

The current v87 path uses two converted PS3 levels:

| Level | Purpose | PS3 files to extract |
|---:|---|---|
| 39 | Multiplayer lobby/background loaded before Local Play | `engine.ps3`, `gameplay_ntsc`, `vram.ps3` |
| 44 | Metropolis multiplayer arena | `engine.ps3`, `gameplay_ntsc`, `sound.bnk`, `vram.ps3` |

Extract them from the PS3 archive with these relative paths:

```text
rc3/ps3data/level39/engine.ps3
rc3/ps3data/level39/gameplay_ntsc
rc3/ps3data/level39/vram.ps3

rc3/ps3data/level44/engine.ps3
rc3/ps3data/level44/gameplay_ntsc
rc3/ps3data/level44/sound.bnk
rc3/ps3data/level44/vram.ps3
```

The PS3 archive also contains `gameplay_pal` and `gameplay.dat` for level 39,
but the current USA/NTSC test path uses `gameplay_ntsc`. Level 39 has no PS3
`mobyload*.ps3` bundles.

### Full multiplayer level set

For all currently identified multiplayer content, extract PS3 levels
**39 through 55 inclusive**:

- Level 39 is the multiplayer lobby/background level.
- Levels 40 through 55 are the multiplayer arena set.
- For every level from 40 through 55, extract exactly:
  `engine.ps3`, `gameplay_ntsc`, `sound.bnk`, and `vram.ps3`.

The expected PS3 source layout is:

```text
rc3/ps3data/
  level39/
    engine.ps3
    gameplay_ntsc
    vram.ps3
  level40/
    engine.ps3
    gameplay_ntsc
    sound.bnk
    vram.ps3
  ...
  level55/
    engine.ps3
    gameplay_ntsc
    sound.bnk
    vram.ps3
```

Do not look for `engine_vert.ps3` in the PS3 archive. That file does not exist
there; it is generated by the modified Replanetizer Vita serializer.

### Vita reference and donor files

The current conversion work also uses files from the Vita copy's level 1:

```text
rc3/psp2data/level1/gameplay_ntsc
rc3/psp2data/level1/engine.ps3
rc3/psp2data/level1/engine_vert.ps3
rc3/psp2data/level1/vram.ps3
rc3/psp2data/level1/sound.bnk
rc3/psp2data/level1/mobyload0.ps3
rc3/psp2data/level1/mobyload1.ps3
rc3/psp2data/level1/mobyload2.ps3
rc3/psp2data/level1/mobyload3.ps3
rc3/psp2data/level1/mobyload4.ps3
```

`engine.ps3`, `engine_vert.ps3`, `gameplay_ntsc`, and `vram.ps3` are reference
fixtures used to validate the Vita layouts. The current hardware-test set uses
the Vita level-1 `sound.bnk` and all five level-1 `mobyload` bundles as donor
files for levels 40–55. This is a temporary bootstrap: map-specific PS3 sound
conversion and native multiplayer Moby bundle generation are not finished.

The donor file hashes used by the current test set are:

| File | Size | SHA-256 |
|---|---:|---|
| `sound.bnk` | 1,699,696 | `61D25F92983E9E3CC59D7330ADCA44E02F92D9FEC340436977B0CBF53AC13098` |
| `mobyload0.ps3` | 286,656 | `2C11A31C90FBCDE148585F582B87EAF4FEAEE564840F959DD3F212C98D8B7E3F` |
| `mobyload1.ps3` | 299,456 | `FC9D61941763F84D0585792D0552CB12249FC9091F6BA21EB31629F6DCE57D9A` |
| `mobyload2.ps3` | 170,112 | `A86BB8F21673D8ACC0F3E6BA53407A0035996FFFBDC973D2DD98825352F2C96F` |
| `mobyload3.ps3` | 486,080 | `C6BFCAE60C95234EF86E38412913CCC1F815A62BFB10C796376261DBDDBA8706` |
| `mobyload4.ps3` | 486,080 | `C6BFCAE60C95234EF86E38412913CCC1F815A62BFB10C796376261DBDDBA8706` |

### Gameplay conversion training pairs

For the mixed-endian gameplay word model used during this research, extract
`gameplay_ntsc` from **both** the PS3 and Vita versions for these 34 shared
campaign levels:

```text
1-14, 16-24, 26-36
```

In full, that is:

```text
1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
16, 17, 18, 19, 20, 21, 22, 23, 24,
26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36
```

Place the paired files under matching `levelN` directory names and pass their
roots through `--training-ps3-root` and `--training-vita-root`. At minimum,
levels 1, 2, 3, and 30 were used as direct structural validation fixtures;
the full list above produced the mixed-endian model used for the multiplayer
conversion.

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
  --training-ps3-root D:\legal-inputs\training\ps3 `
  --training-vita-root D:\legal-inputs\training\vita `
  --reference-vita D:\legal-inputs\vita\level1\gameplay_ntsc `
  --report D:\work\level44\gameplay-report.json
```

The converter and validators are research tools, not a general-purpose
one-click level port. Their reports must be reviewed before hardware testing.

### Expected converted output

Converted level 39 must contain:

```text
level39/
  engine.ps3
  engine_vert.ps3
  gameplay_ntsc
  vram.ps3
```

Each converted arena directory from level 40 through level 55 currently
contains:

```text
levelN/
  engine.ps3
  engine_vert.ps3
  gameplay_ntsc
  sound.bnk
  vram.ps3
  mobyload0.ps3
  mobyload1.ps3
  mobyload2.ps3
  mobyload3.ps3
  mobyload4.ps3
```

Only `engine.ps3`, `engine_vert.ps3`, `gameplay_ntsc`, and `vram.ps3` are
converted map data in the current set. As noted above, `sound.bnk` and the five
`mobyload` files are temporary Vita level-1 donors.

## Vita deployment

### The combined loose-file plugin is required

Do **not** replace or rebuild the installed `rc3.psarc`. The converted levels
are loaded through the collection-wide loose-file overlay in this repository's
other combined plugin, [`rc1_audio_fixes.suprx`](../README.md#installation).
Despite its historical filename, that plugin includes
`rc_loose_overrides.c` and supports loose replacements for RC1, RC2, and RC3.

Build it from the repository root:

```powershell
.\build_audio_fixes.ps1
```

Install both plugins:

```text
ur0:tai/rc1_audio_fixes.suprx
ur0:tai/rc3_restored_menu_test.suprx
```

The combined plugin also requires `ioplus.skprx` under `*KERNEL`. The relevant
taiHEN configuration is:

```text
*KERNEL
ur0:tai/ioplus.skprx

*PCSA00133
ur0:tai/rc1_audio_fixes.suprx
ur0:tai/rc3_restored_menu_test.suprx
```

Without `rc1_audio_fixes.suprx`, files placed under `ux0:data/rc_override`
will not be overlaid and RC3 will continue reading only from its original
PSARC.

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

For the minimum level-39 plus Metropolis test, the Vita directory tree must be:

```text
ux0:data/rc_override/rc3/psp2data/
  level39/
    engine.ps3
    engine_vert.ps3
    gameplay_ntsc
    vram.ps3
  level44/
    engine.ps3
    engine_vert.ps3
    gameplay_ntsc
    sound.bnk
    vram.ps3
    mobyload0.ps3
    mobyload1.ps3
    mobyload2.ps3
    mobyload3.ps3
    mobyload4.ps3
```

For the full arena set, repeat the ten-file arena layout under `level40`
through `level55`.

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
