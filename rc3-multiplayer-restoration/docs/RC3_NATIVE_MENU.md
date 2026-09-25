# RC3 native five-entry menu

This experimental PCSA00133 plugin restores the two retail front-end records
removed from the Vita build. It redirects the game's original three-record
menu table to a five-record table containing:

1. Load Game
2. New Game
3. Multiplayer
4. Multiplayer Tutorial
5. Options

The original renderer, localized strings, highlight, and controller handling
remain in use. The first test build deliberately gives the two restored rows
safe logging callbacks. Selecting either row writes to
`ux0:data/rc3mp/native_menu.log` and does not start a level.

`rc3_native_menu_lobby_test.suprx` is the separate experimental build. Its
Multiplayer callback keeps the game in the title front end and rebuilds the
native renderer with the four PS3 `LobbyGUIScreenMain` strings: Online Play,
Local Play, Edit Profiles, and Exit Multiplayer. Exit Multiplayer rebuilds the
five-row title table. The remaining lobby callbacks are deliberately inert.
Keep the normal build as the rollback.

## State 39 and the removed lobby controller

State 39 is not the missing multiplayer menu. PS3 and Vita contain identical
68-record front-end transition maps, including states 39 through 55. State 39
selects callback record zero on both platforms, and the corresponding Vita
functions are ports of the PS3 loading-screen initialize, update, and draw
functions. Replacing these callbacks with the title initializer crashes because
the title widget context has already been destroyed.

After the PS3 loader completes, function `0x009035C0` performs a separate
`LobbyGUI` bootstrap that is absent from Vita. It resets the lobby subsystem,
reserves a dedicated memory arena, initializes its allocator, and calls
`0x00908848`. That function allocates a `0x294`-byte manager and constructs 43
screen slots with the factory at `0x00907CB8`. The initial screen is slot
`0x18`, `LobbyGUIScreenMain`.

The Vita ELF contains none of the `LobbyGUI` diagnostic strings or lobby-screen
constructors. Restoring the path therefore requires a replacement controller;
patching state 39 alone cannot restore it. The planned minimum controller is:

1. Main (`0x18`)
2. Local Profiles (`0x16`)
3. Create/Match Setup (`0x0C`)
4. Staging

The first controller milestone remains in the still-valid title front end. This
lets screen selection and back-navigation be proven independently of the
state-39 lifetime problem. State 39 should remain the original Vita loader and
hand off to the replacement controller only after loading has completed.

Static tracing now identifies the PS3 main-screen lifecycle precisely:
`LobbyGUIScreenMain::Enter` is `0x00943CD0`, update is `0x009441A8`, and draw
is `0x00944970`. The manager's active-screen field is at `+0x288`; the default
bootstrap writes `0x18`. The first three choices change this field instead of
requesting another front-end load. Local Play first selects `0x15` after
preparing local state; the factory's Local Profiles screen is selector `0x16`
with constructor `0x0093F2D8`. Exit is the choice that tears down the lobby and
returns to the title front end.

The Vita global locale still contains the removed multiplayer text, including
Online Play (`0x0F56`), Local Play (`0x0F58`), Team (`0x0F12`), Skin
(`0x0F13`), Player 1 through Player 4 (`0x0FF5`-`0x0FF8`), Start (`0x0FF9`),
Exit Multiplayer (`0x15CE`), and Edit Profiles (`0x18CC`). A replacement can
therefore use the game's own localized text instead of embedded English.
`tools/rc3_locale_dump.py` parses both little-endian Vita and big-endian PS3
locale files and supports ID lookup or text search.

The Local Profiles constructor also confirms that Vita retains the complete
PS3 choice ranges: team colors are `0x0F1A`-`0x0F21`, skins are
`0x0F22`-`0x0F28`, Ready is `0x1102`, and Launching is `0x1330`. The test
controller now exposes one native local slot with live team and skin values;
selecting either value cycles through the same ordered range as PS3.

The PS3 Local Profiles update at `0x0093FAD0` exposes the arena handoff timing.
After its setup calls complete, it checks that the current front-end state is
39 and requests the selected arena with `0x0015C7DC` before returning from the
update. Waiting until after the state-39 callback has completed allows the
retail Vita front end to return to the title scene. The experimental Vita
handoff therefore issues its arena request on the state-39 completion edge;
its first controlled target is level 40.

`tools/rc3_compare_frontend_states.py` verifies the transition tables and
prints the resolved PS3/Vita callback pairs.

The Vita-specific three-row frame scale is also restored from `0.47` to the
PS3 five-row value of `1.0`, allowing the original orange panel to contain all
five entries.

The animated side ornaments use their PS3 five-row vertical anchors (`0.225`
and `0.7875`) instead of the Vita compact-menu anchors (`0.370` and `0.6325`).
The native divider-widget loop is restored from two separators to four, so a
line is drawn between every adjacent pair of menu choices.

The compact Vita text padding and its first three selector coordinates are
replaced with the PS3 five-row values. The first two divider coordinates are
also restored; the other two were already present but unused in the Vita data.

The plugin validates the RC3 segment sizes, the original three records, and
every patched instruction before changing code. It therefore fails closed on
the collection launcher, RC1, RC2, or another RC3 executable revision.
