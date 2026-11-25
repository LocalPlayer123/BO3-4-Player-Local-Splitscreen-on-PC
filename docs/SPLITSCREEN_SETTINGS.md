# Splitscreen settings

Open **SPLITSCREEN SETTINGS** from the offline main menu or supported LAN game
lobby. Four permanent name slots are available, even when guests are not seated.

- Select **Player 1** through **Player 4** to edit that slot's name.
- **PRESET: PLAYER 1 - PLAYER 4** fills all four slots with numbered names.
- **RESET: ENGINE ORIGINAL NAMES** clears all four overrides.
- Accepting an empty name resets just that slot. Cancel leaves it unchanged.
- Names allow up to 31 UTF-8 bytes; quotes, slashes, semicolons and control
  characters are rejected. The stock keyboard may impose additional restrictions.

The native plugin automatically saves changed names to the game folder's
boiii_players/splitscreen_names.cfg using a temporary file and atomic replacement.
No Lua filesystem access or unsafe-Lua option is needed. A load error protects the
existing file from replacement; a save error is shown instead of claiming success.
Editing is disabled until the native bridge initializes. Offline name overrides do
not alter the global online profile, XUIDs, controller assignments or screen slots.

Closing Settings updates the existing global lobby name models immediately and
twice more at 250 ms intervals to cover the native name poll. It no longer depends
on capturing an earlier roster widget. Only valid, uniquely identified local rows'
gamertag and clan-prefixed display-name models are written; controller slots,
remote rows and membership are untouched. Reset uses fresh stock rows rather than
previously customized presentation rows; native/session propagation still needs
live verification. The diagnostic dvar splitscreen_names_refresh_status reports
updated row counts or why the refresh was skipped.

In-game scoreboard presentation has a separate adapter: stock GetClientName
(also used by GetClientNameAndClanTag) and GetScoreboardPlayerName now resolve
custom names for unique, active local controllers in an offline match. Match
client numbers are obtained from Engine.GetClientNum, not assumed to equal
controller slots or sorted scoreboard row indices. Clan tags remain stock.
Remote players, bots, malformed rows, ambiguous mappings, unavailable bridge
APIs and online play retain the original display functions. The adapter does
not rewrite server client records, identities, or arbitrary text. Other HUD
surfaces that bypass these functions are not covered by this fix.

The native overhead cache uses a different path again. For supported checksum
0x06531394, its cache updater at RVA 0x006A7AD0 calls the name formatter at
0x013E3160 with 32-byte plain and 40-byte clan-prefixed buffers. A separate
byte-guarded detour now replaces successful formatter output for uniquely mapped
active local players only. It reads the established seat records and CG client
numbers (CG stride 0x342720); it does not assume local slot equals match client.
The CG global is protected/encoded: reading it as a raw pointer was a bug in the
first overhead adapter. The corrected adapter wraps only the overhead updater
call at 0x0069D7D9. A checked near relay supplies the engine-decoded viewer CG
already held in RBX, without changing the original call's stack or return address.
A thread-local scope exists only for that stock updater invocation, restoring
the previous context on return or unwinding. Other formatter consumers retain
stock names, and no CG pointer is cached across map transitions. The complete
updater body is byte-checked except its existing playerDetails relocation operand;
that operand must agree with both the draw and reset LEAs to a readable array.
Stock clan prefixes are retained, UTF-8 truncation respects the supplied buffer
capacity, and invalid/unready/ambiguous cases retain stock output. No actor,
entity, session or identity record is written. The diagnostic dvar
splitscreen_names_match_status reports ready or hook_error. If byte checks fail,
only this adapter is disabled; the prior settings/lobby bridge remains available.

The settings body uses single-line UIText heights (which control font size even
for multiline paragraphs), an opaque background, and a flat safe-area title.
Guest joining owns the physical A-release gesture and preserves requests while
the lobby queue is briefly busy. The offline four-seat play hint takes precedence
over competing widget states; the old clickable PC activation toggle stays hidden.

## Validation boundary

Windows cross-build and portable regression coverage exercise the name codec,
four-slot settings UI, keyboard acceptance/cancellation, saved-name bridge status,
roster ordering, online guards and existing ezz join/leave behavior. The user has
confirmed that the settings menu opens and name editing appears to work. Corrected
layout, physical one-press joining/hints, close-time roster refresh, and disk
save/restart still require live testing. A two-player Zombies screenshot confirmed
the scoreboard displays the edited names after the Lua adapter, while both
overhead labels still retained stock names with the first native adapter.
New regressions cover match client mapping, sorted scoreboard rows, all viewing
controllers, clan tags, resets, late loading, and stock fallback behavior. Native
tests also cover shuffled/duplicate seat mappings and bounded UTF-8/clan output.
The corrected scoped native adapter has not yet been confirmed live. Run
tools/run_name_display_tests.py for an additional UBSan harness including the
unchanged production overhead module. It exercises all viewers, shuffled clients
and controllers, invalid/duplicate/unready mappings, stock formatter failures,
clan/reset behavior, nested calls, thread-local lifetime, every byte guard,
synchronized relocation operands, and checked relay installation failures.
These mocks do not execute the Windows relay or establish live game behavior.