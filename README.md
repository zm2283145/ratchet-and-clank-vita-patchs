# Ratchet & Clank Vita patches

Community patches for the PS Vita *Ratchet & Clank Collection*. This repository currently contains a music-transition fix for Ratchet & Clank 1 (USA, `PCSA00133`). The longer-term goal is to collect verified fixes for the trilogy and, where practical, combine them into one game-specific plugin.

## RC1 music transition fix

### Original bug and diagnosis

On the unpatched Vita port, RC1 can keep repeating the music from the first area loaded on a planet instead of changing tracks when Ratchet crosses into another music area. Crossing additional boundaries can briefly double the loop, then leave the game silent. A notable clue from hardware testing: after crossing two music-change thresholds, playing a cutscene caused the *correct track for the current area* to start when the cutscene ended. This suggests the cutscene return path reinitializes or resynchronizes music state; it does not establish exactly which internal function does so.

We first tested a simpler runtime workaround that reset the music manager and started the destination track immediately. It stopped the stuck-loop behavior, but made every transition a hard cut. The final patch keeps the game's original crossfade path. Analysis showed that a transition request carries both the destination area's long-running track and a short bridge cue. The port failed to promote the destination track into the primary music slot. A separate transition latch could then remain set after the secondary stream completed, blocking the next boundary crossing. The plugin promotes the destination track while preserving the bridge cue, and clears that latch only after the secondary stream has finished. It does not restart the track or throw away a queued transition.

`rc1_music_fix.suprx` applies those runtime changes. The crossfade fix was confirmed on hardware with VitaCheat first: music changed in both directions and continued looping. The same patch bytes were then loaded with this standalone plugin; runtime address `0x81124226` was observed patched with VitaCheat's music code disabled, and gameplay testing confirmed the music issue was fixed.

The plugin is configured under the collection's title ID, but checks the loaded executable's segment size and original bytes before writing. It does nothing when the launcher, RC2, RC3, or an unrecognized RC1 revision is running. It is specific to the USA `PCSA00133` RC1 executable whose decrypted ELF has SHA-256 `DA03DDFE0B20A0C61CFF471772D14916FA6DA5C64597D7E4C3B9141A60485636`.

### Install

1. Back up your active taiHEN `config.txt`.
2. Copy [`rc1_music_fix.suprx`](rc1_music_fix.suprx) to `ur0:tai/rc1_music_fix.suprx`.
3. Add the following to the active taiHEN config (typically `ur0:tai/config.txt`), using the existing `*PCSA00133` section if one is already present:

   ```text
   *PCSA00133
   ur0:tai/rc1_music_fix.suprx
   ```

4. Refresh taiHEN or reboot, and disable any overlapping VitaCheat RC1 music patch before testing.

To uninstall, remove only the `ur0:tai/rc1_music_fix.suprx` line from the config, refresh taiHEN or reboot, then remove the plugin file. Do not overwrite the game's installed SELF.

The included plugin binary has SHA-256 `903D0697DE366AA320C257CAAF95DE44B976A0483983539FC7593E7C46DAD28D`.

### Build from source

Install VitaSDK with taiHEN headers/stubs, then run `./build.ps1` in PowerShell. Set `VITASDK` if the SDK is not in `C:\vitasdk`. The build script produces `rc1_music_fix.elf`, `rc1_music_fix.velf`, and `rc1_music_fix.suprx` from [`rc1_music_fix.c`](rc1_music_fix.c) and [`exports.yml`](exports.yml).

## Scope and future fixes

Only the RC1 music-transition/looping behavior above is confirmed. The abrupt gap at the seam of some music loops and the low-quality, 4:3 FMVs are separate issues, not fixed by this plugin. Future patches should include a reproducible build and hardware test notes before being marked confirmed. We can consolidate them into a single plugin once the individual fixes are stable.

This repository contains no decrypted game executable or copyrighted game assets. You need your own copy of the game.
