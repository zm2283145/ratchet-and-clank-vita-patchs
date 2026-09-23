# Ratchet & Clank Vita patches

Community patches for the PS Vita *Ratchet & Clank Collection*. The current `rc1_audio_fixes.suprx` combines the RC1 music crossfade, music-loop startup, and looping sound-effect fixes for Ratchet & Clank 1 (USA, `PCSA00133`). It also retains the Tesla Claw/Morph-o-Ray release cleanup. Future fixes for the collection may be added after separate testing.

## RC1 music transition fix

### Original bug and diagnosis

On the unpatched Vita port, RC1 can keep repeating the music from the first area loaded on a planet instead of changing tracks when Ratchet crosses into another music area. Crossing additional boundaries can briefly double the loop, then leave the game silent. A notable clue from hardware testing: after crossing two music-change thresholds, playing a cutscene caused the *correct track for the current area* to start when the cutscene ended. This suggests the cutscene return path reinitializes or resynchronizes music state; it does not establish exactly which internal function does so.

We first tested a simpler runtime workaround that reset the music manager and started the destination track immediately. It stopped the stuck-loop behavior, but made every transition a hard cut. The final patch keeps the game's original crossfade path. Analysis showed that a transition request carries both the destination area's long-running track and a short bridge cue. The port failed to promote the destination track into the primary music slot. A separate transition latch could then remain set after the secondary stream completed, blocking the next boundary crossing. The plugin promotes the destination track while preserving the bridge cue, and clears that latch only after the secondary stream has finished. It does not restart the track or throw away a queued transition.

The crossfade fix was confirmed on hardware with VitaCheat first: music changed in both directions and continued looping. The same patch bytes were then loaded with an earlier standalone plugin; runtime address `0x81124226` was observed patched with VitaCheat's music code disabled, and gameplay testing confirmed the music issue was fixed. The combined plugin contains those same patch bytes.

The original stream constructor also opens, checks, and closes each audio file before requesting its actual asynchronous playback stream. Skipping that redundant preflight removed almost all of the roughly one-second silence between loops in hardware tests with VitaCheat. Area changes continued working and a second area's loop had no noticeable delay. A brief restart seam remains on some tracks. The combined plugin includes the same four-byte change. The skipped check applies to all streamed audio in RC1, not just music; the actual asynchronous open and its error handling remain in place.

The plugin is configured under the collection's title ID, but checks the loaded executable's segment size and original bytes before writing. It does nothing when the launcher, RC2, RC3, or an unrecognized RC1 revision is running. It is specific to the USA `PCSA00133` RC1 executable whose decrypted ELF has SHA-256 `DA03DDFE0B20A0C61CFF471772D14916FA6DA5C64597D7E4C3B9141A60485636`.

### Install

1. Back up your active taiHEN `config.txt`.
2. Copy [`rc1_audio_fixes.suprx`](rc1_audio_fixes.suprx) to `ur0:tai/rc1_audio_fixes.suprx`.
3. Add the following to the active taiHEN config (typically `ur0:tai/config.txt`), using the existing `*PCSA00133` section if one is already present:

   ```text
   *PCSA00133
   ur0:tai/rc1_audio_fixes.suprx
   ```

4. Remove entries for older standalone audio plugins, `rc1_state7_audio_test.suprx`, and RC1 audio diagnostic probes. Disable overlapping VitaCheat RC1 audio codes, then fully close and relaunch the game (or reboot).

To uninstall, remove only the `ur0:tai/rc1_audio_fixes.suprx` line from the config, refresh taiHEN or reboot, then remove the plugin file. Do not overwrite the game's installed SELF.

The current combined plugin binary has SHA-256 `32DB33CEE2F71AFD006A850AF6F1E4B50721C7334346C73E7939020DC974D722`.

### Build from source

Install VitaSDK with taiHEN headers/stubs, then run `./build_audio_fixes.ps1` in PowerShell. Set `VITASDK` if the SDK is not in `C:\vitasdk`. The build script produces `rc1_audio_fixes.elf`, `rc1_audio_fixes.velf`, and `rc1_audio_fixes.suprx` from [`rc1_audio_fixes.c`](rc1_audio_fixes.c) and [`exports_audio_fixes.yml`](exports_audio_fixes.yml).

## RC1 Tesla Claw and Morph-o-Ray release fix

Hardware traces confirmed that the Tesla Claw's looping sound (`ID 4`) and the
Morph-o-Ray's looping sound (`ID 0`) can remain in the playing state after the
fire button is released. The game eventually stops each stale voice during a
weapon switch. The Pyrocitor follows its normal stop path and is not affected.

The weapon portion of `rc1_audio_fixes.suprx` watches only looping `ID 0` and `ID 4` SFX slots
created while CIRCLE is held. After release it gives the game 40 ms to perform
its normal cleanup. If the exact same slot, voice, owner, flags, and sound ID
are still active, the plugin requests the same state transition used by RC1's
native stop routine. Slots that shut down normally or are reused during the
grace period are left untouched.

The combined plugin checks the recognized RC1 executable segment sizes and
original music-code bytes before acting. It does nothing in the launcher,
RC2, or RC3. These checks are for this USA executable revision and do not
establish compatibility with other revisions.

Hardware testing of the earlier plugin setup confirmed that it stops the
Tesla Claw and Morph-o-Ray sounds after release. The combined binary contains
the same cleanup logic.

## RC1 elevator and other repeating sound effects

The first Veldin elevator and Kerwan's first helicopter exposed a second,
more general sound bug. A sound slot in state `7` is *starting*, but RC1's
"already playing?" check only recognizes states `1` and `2`. If the same
object starts its sound again during state `7`, RC1 can allocate a second
looping voice and replace the object's remembered slot. Its later stop then
reaches only the remembered voice; the earlier one keeps playing. This
explains why the effect can persist until a reload or explicit cleanup.

The plugin hooks that existing check and treats state `7` as active **only
when the slot belongs to the same object**. It otherwise leaves the original
check unchanged. No sound definitions are muted, no continuous diagnostic
logging is included, and the fix applies through RC1's shared sound path
rather than a Veldin-specific address. In hardware tests with the same hook
running as a separate quiet plugin, the Kerwan helicopter gun and the
previously reproducible elevator loop stopped normally, including after
leaving and returning to the area. Those tests support the shared fix, but
do not prove every sound effect in every level is corrected.

## Scope and verification

The music crossfade and weapon fixes were confirmed on hardware in earlier
plugin setups. The loop-startup improvement was confirmed with VitaCheat;
a brief seam can still be audible on some tracks. The state-`7` hook was
confirmed with a separate quiet plugin. This newly unified binary builds
successfully, but should receive its own hardware check before being called
fully verified. The low-quality, 4:3 FMVs are a separate issue.

The plugin checks the recognized RC1 executable's segment sizes and original
code bytes before acting. It does nothing in the launcher, RC2, RC3, or an
unrecognized RC1 revision. The weapon cleanup polls RC1 sound state while
playing; the state-`7` hook itself does not run a polling thread.

This repository contains no decrypted game executable, diagnostic logs,
Vita configuration, or copyrighted game assets. You need your own copy of
the game.
