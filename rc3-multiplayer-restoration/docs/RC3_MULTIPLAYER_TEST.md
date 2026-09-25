# RC3 multiplayer menu probe

This diagnostic plugin is only for the USA `PCSA00133` version of Ratchet &
Clank 3. It replaces the dormant Multiplayer menu callback, which otherwise
tries to reboot into the PS2-only `host0:code/game/i5bootn.elf` path.

The probe does not load a map or start networking. Selecting Multiplayer
returns safely and appends a line to:

`ux0:data/rc3mp/menu_test.log`

## Install and test

1. Copy `rc3_multiplayer_probe.suprx` to
   `ur0:tai/rc3_multiplayer_probe.suprx`.
2. Add it under the existing title section in the active taiHEN config:

   ```text
   *PCSA00133
   ur0:tai/rc3_multiplayer_probe.suprx
   ```

3. Refresh taiHEN or reboot, launch Ratchet & Clank 3, and select Multiplayer.
4. Open `ux0:data/rc3mp/menu_test.log` with VitaShell.

A successful run contains both `RC3 multiplayer probe attached.` and
`Multiplayer menu callback reached safely.`. Remove the config line and reboot
to uninstall. The plugin does not modify the installed game executable.
