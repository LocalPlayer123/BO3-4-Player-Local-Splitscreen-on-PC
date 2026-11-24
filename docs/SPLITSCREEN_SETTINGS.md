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
layout, physical one-press joining/hints, close-time roster refresh, disk
save/restart, and in-game name propagation still require live testing. Offline
tests do not establish those game behaviors.