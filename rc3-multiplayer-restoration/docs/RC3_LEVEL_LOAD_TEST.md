# RC3 native level-load probe

This diagnostic asks RC3's native transition routine to load existing Vita
level 1 after `L+R+SELECT+START` is held for one second. It does not yet
install or load PS3 multiplayer assets. The hotkey is used because the retail
front end does not display the dormant developer Multiplayer entry.

The exact RC3 segment-size checks make the plugin a no-op in the collection
launcher, RC1, and RC2.

## Install

1. Disable or remove the `rc3_multiplayer_probe.suprx` config entry.
2. Copy `rc3_level_load_probe.suprx` to
   `ur0:tai/rc3_level_load_probe.suprx`.
3. Add this line under `*PCSA00133`:

   ```text
   ur0:tai/rc3_level_load_probe.suprx
   ```

4. Refresh taiHEN or reboot, launch RC3, then hold
   `L+R+SELECT+START` together for one second at the title menu.
5. Check `ux0:data/rc3mp/level_test.log` with VitaShell.

Expected behavior is a normal loading transition into level 1. The hotkey
fires only once per RC3 launch. If the game stays on the menu or crashes,
preserve the log and describe the last visible screen. Remove the config line
and reboot to uninstall.
