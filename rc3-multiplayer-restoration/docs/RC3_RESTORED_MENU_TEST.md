# RC3 retained-menu test plugin

This experimental PCSA00133-only plugin reuses the four-entry boot menu that
survived from the PS3 build in the Vita executable. It replaces the obsolete
callbacks before displaying the menu, so the removed PS2 network boot path is
never executed.

Menu behavior:

- `Multiplayer` requests level 39 directly. Converted multiplayer assets are
  supplied by the loose-file overlay under `rc3/psp2data/level39`, avoiding
  level 1's cutscene and progression flow.
- `Single Player` reveals the unchanged retail `New Game / Load Game /
  Options` menu.
- `Load Game` selects Load Game through the retail menu.
- `Options` selects Options through the retail menu.

The plugin leaves the original `Press Start` screen alone. The restored menu
appears automatically shortly after Start or Cross enters the retail front end.
The delay intentionally allows the Insomniac logo sequence to finish first.
`SELECT+TRIANGLE` hides or reopens it. The existing
`L+R+SELECT+START` level-load hotkey remains available as a fallback.

The plugin logs to `ux0:data/rc3mp/restored_menu.log`.

Only install this plugin while testing RC3. Remove the older
`rc3_level_load_probe.suprx` and `rc3_multiplayer_probe.suprx` lines from the
PCSA00133 section so the probes do not compete for the same menu and controls.
