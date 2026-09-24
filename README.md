# Ratchet & Clank Collection Vita patches

A taiHEN plugin containing hardware-tested fixes and replacement-file support
for the USA PS Vita release of *Ratchet & Clank Collection* (`PCSA00133`).

## Features

- Corrects Ratchet & Clank 1 music transitions while preserving crossfades.
- Reduces the long silence between streamed-audio loop restarts in RC1.
- Stops Tesla Claw and Morph-o-Ray audio that can continue after firing ends.
- Prevents duplicate starting-state voices for looping sound objects throughout
  RC1. The Veldin elevator and Kerwan helicopter are confirmed examples, but
  the fix operates on the game's shared looping-audio path rather than on
  those two objects specifically.
- Reduces RC1's pending display queue from two frames to one, substantially
  reducing the port's display-queue latency. Hardware measurements found a
  median reduction from 123.06 ms to 51.02 ms between frame submission and
  the display callback. This is not a complete button-to-screen measurement.
- Displays pre-rendered movies fullscreen and centered in RC1, RC2, and RC3.
- Loads loose replacement files for all three games without rebuilding a
  PSARC archive.

The plugin identifies each supported executable by its segment sizes and
verifies original instructions before applying game-code patches. Audio fixes
are RC1-specific. Loose-file overlays remain registered while the collection
switches between its three executables.

## Installation

1. Install taiHEN-compatible custom firmware and back up the active
   `ur0:tai/config.txt`.
2. Install [`ioplus.skprx`](https://github.com/TeamFAPS/PSVita-RE-tools/blob/master/ioPlus/ioPlus-0.2/release/ioplus.skprx)
   under `*KERNEL` and reboot. `ioplus` is required for the loose-file
   replacement layer to access files under `ux0:data` from the retail game
   process:

   ```text
   *KERNEL
   ur0:tai/ioplus.skprx
   ```

3. Copy `rc1_audio_fixes.suprx` to:

   ```text
   ur0:tai/rc1_audio_fixes.suprx
   ```

4. Add the plugin beneath the existing title section, or create it if needed:

   ```text
   *PCSA00133
   ur0:tai/rc1_audio_fixes.suprx
   ```

5. Remove entries for superseded versions of this plugin, refresh taiHEN, and
   reboot the Vita.

The release binary is 29,254 bytes and has SHA-256:

```text
D910690FBCDAEB19AC1AB99FC87837FC1ABF6A14ACE43DA496832F51A1F9471A
```

## Loose-file replacements

Place a file below the appropriate directory using the same relative path and
filename it has inside that game's PSARC:

```text
ux0:data/rc_override/rc1/<path inside rc1.psarc>
ux0:data/rc_override/rc2/<path inside rc2.psarc>
ux0:data/rc_override/rc3/<path inside rc3.psarc>
```

For example, RC1's logo movie can be replaced with:

```text
ux0:data/rc_override/rc1/psp2data/movies/logo_movie.bik
```

If a loose file is absent, the game reads the original PSARC normally. The
plugin does not modify the installed archives. Overlay registration results
are written to `ux0:data/rc_loose_override.log`.

## Higher-quality PS3 movie replacements

The PS3 collection's RC1 movies can be converted to a Vita-friendly Bink 1
format and loaded through the loose-file system. The movies are not distributed
with this project. You must provide files extracted from your own PS3 copy.

### Requirements

- A Windows PC.
- Movie files from your own PS3 copy. RC1 stores them under
  `PS3_GAME/USRDIR/rc1/ps3data/movies`.
- The Bink 1-era RAD Video Tools 1.99i installer,
  [`RADTools_1994i.exe`](https://www.videohelp.com/software?d=RADTools_1994i.exe).
  This is the exact version tested by the conversion script. Newer packages
  may expose different command-line tools or produce an incompatible format.
- Enough free storage for the source files, converted files, and FTP transfer.

The output must be Bink 1 with the `BIKi` signature. Bink 2/`.bk2` files are
not supported by the Vita games.

### Tested output settings

- 720 × 408
- 30 frames per second
- 48 kHz, 16-bit stereo audio
- 300,000 bytes/second video data rate
- Largest compressed frame below 65,536 bytes

The frame-size limit matters: larger frame spikes caused reproducible crashes
on Vita hardware even when the file copied correctly. The included conversion
script validates the Bink header, dimensions, frame rate, audio-track count,
file length, and largest compressed frame.

### Convert RC1's movies

After installing RAD Video Tools 1.99i, run:

```powershell
.\convert_rc1_ps3_fmvs.ps1 `
  -SourceRoot 'E:\PS3_GAME\USRDIR\rc1\ps3data\movies' `
  -OutputRoot '.\converted\rc1\psp2data\movies'
```

Change the source path to match the mounted or extracted PS3 game. The script
converts the 42 non-Japanese movie files referenced by the Vita RC1 archive and
skips the unused `_j` variants.

Copy the resulting directory to the Vita so the final layout is:

```text
ux0:data/rc_override/rc1/psp2data/movies/<original filename>.bik
```

Keep every filename unchanged. Restart the game after installing the plugin;
individual movie replacements can then be added or updated without repacking
`rc1.psarc`.

## Building

Install VitaSDK with its taiHEN and system-library stubs, then run:

```powershell
.\build_audio_fixes.ps1
```

Set `VITASDK` if the SDK is not installed at `C:\vitasdk`. The build combines:

- `rc1_audio_fixes.c`
- `rc1_input_latency_fix.c`
- `rc_loose_overrides.c`
- `rc_fmv_widescreen.c`
- `rc1_combined_fixes.c`

It produces `rc1_audio_fixes.elf`, `rc1_audio_fixes.velf`, and the installable
`rc1_audio_fixes.suprx`.

## Compatibility

The patches were developed and tested with the USA `PCSA00133` release. Other
regions or executable revisions are not currently supported. The repository
and releases do not include decrypted executables, PSARC contents, or movie
assets.
