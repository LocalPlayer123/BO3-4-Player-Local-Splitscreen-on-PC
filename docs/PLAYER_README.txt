BO3 Local Splitscreen 2.4.1  -  build {VERSION}
================================================

Up to FOUR local split-screen players for Call of Duty: Black Ops III on PC -
Zombies and Multiplayer (offline custom games, bots work too). An ezz BOIII
plugin. Local (offline) play only.

This is a BETA. Please report what works and what does not.


NEW IN 2.4.1
------------
* Fixed: players 2-4 start with player 1's classes and stats again (as on
  console: a guest plays with the host's level, unlocks and classes and
  can change them for that session). Since 2.0 this copy silently did not
  happen under ezz BOIII, so players 2-4 kept old classes of their own.
* Internal cleanup for the ezz BOIII developers: the last five
  hand-written machine-code patches are C++ now too, through one shared,
  self-tested helper that lets C++ code run at any point inside a game
  function. No patch-specific machine code is left in the mod.

Updating from 2.2 - 2.4: extract this zip over the old files.


NEW IN 2.4
----------
* Nothing changes in the game: 2.4 plays like 2.3.1.
* Internal cleanup for the ezz BOIII developers: nine of the hand-written
  machine-code patches are now ordinary C++ functions (the player-active
  checks, the scene and lens-flare guards for players 3/4, the player
  count, the controller setup and the sun-shadow patches). Only five
  small ones remain, where the game offers no clean place to hook in.

Updating from 2.2 - 2.3.1: extract this zip over the old files.


NEW IN 2.3.1
------------
* Fixed: a 3- or 4-player game could crash while loading ("Ezz has tried
  to recover your game") or get stuck on the loading screen. Whether it
  happened depended on where Windows placed the game in memory when it
  started - on some PCs every time until the next reboot. Players 3 and 4
  now always get game memory of their own.

Updating from 2.2 - 2.3: extract this zip over the old files.


NEW IN 2.3
----------
* Nothing changes in the game: 2.3 plays exactly like 2.2.4.
* Internal cleanup for the ezz BOIII developers: every game address the
  mod uses is now in one file instead of spread over the code. When the
  game or ezz BOIII moves to a new version, supporting it means updating
  that one file.

Updating from 2.2 - 2.2.4: extract this zip over the old files.


NEW IN 2.2.4
------------
* Fixed: five game functions that map scripts can call for a player
  (perk checks and the perk list, stats, the helicopter check and one
  menu toggle) still refused players 3 and 4 with a script error. They
  now answer for all four players, as on console. We have not seen a
  map call them for players 3/4 in our tests; this closes the gap.
* Removed from the known issues: "players 3 and 4 look too bright in
  Multiplayer". Measured over many scenes, their screens are not brighter
  than those of players 1 and 2.

Updating from 2.2 - 2.2.3: extract this zip over the old files.


NEW IN 2.2.3
------------
* Fixed: the 4th player had to press A twice to join. When the 3rd
  player joined, the game opened a warning ("You will not be able to play
  networked games with 3 or more controllers") and the next A press only
  closed it. In the offline lobby, the only place for 3-4 players, this
  warning is no longer shown, so every player joins with one A press.

Updating from 2.2 - 2.2.2: extract this zip over the old files.


NEW IN 2.2.2
------------
* Fixed: in the lobby, players 3 and 4 showed the wrong number after their
  name ("(2)" and "(3)" instead of "(3)" and "(4)"). With player names
  that are not exactly 8 characters long, the mod changed the wrong
  character of the name instead. Players 2-4 now show as "<name>(2)",
  "<name>(3)" and "<name>(4)".

Updating from 2.2 or 2.2.1: extract this zip over the old files.


NEW IN 2.2.1
------------
* Fixed: a controller plugged in while you play alone became a SECOND
  player on its first button press (the party showed "<name>(2)"). The
  first controller is now always player 1, as in the unmodded game; any
  further controller joins by pressing A, in the order they press it.
* Fixed: the game could stall for about 25 seconds when that happened in
  the online main menu. Extra players now join in the offline lobby only
  (PLAY OFFLINE), which is where 3 and 4 players work anyway.

Updating from 2.2: extract this zip over the old files.


NEW IN 2.2
----------
* Cleanup release - the game plays exactly as with 2.1. All test and
  debug code was removed from the mod, so the plugin is smaller (about
  285 KB instead of 335 KB) and does less work while you play.
* Less work per frame: the mod no longer updates internal test counters
  (up to several hundred system calls a second before), and one lobby
  step with 3-4 players no longer repeats every frame.

Updating from 2.1: extract this zip over the old files (it replaces
boiii\plugins\bo3_local_splitscreen.dll). Nothing else changes.


NEW IN 2.1
----------
* The mod is now an ezz BOIII plugin: it lives in boiii\plugins\ instead of
  a XINPUT9_1_0.dll next to boiii.exe, so it no longer clashes with other
  mods that ship an XINPUT9_1_0.dll.
* Fixed: the game could freeze when a controller was plugged in while the
  game was running (for example at the main menu).
* Fixed: a rare crash in the Multiplayer lobby while a third player joined
  (it depended on the order in which the controllers joined).

Updating from 2.0: DELETE XINPUT9_1_0.dll from your Black Ops III folder,
then extract this zip. If the old file stays, it starts first and the
plugin stands aside - you would keep running 2.0.


NEW IN 2.0 (compared with 1.0)
------------------------------
* Now for ezz BOIII (tested with ezz BOIII 3.0.0). The BOIII client from
  CBServers is not supported by this version - the mod stays switched off
  there and the game runs normally. Version 1.0 stays available for it.
* Multiplayer: 4-player offline custom games, with or without bots
  (PLAY OFFLINE -> MULTIPLAYER).
* Separate sun shadows for every screen - no more shimmering shadows for
  players 2-4.
* Fixed: player 4's screen could turn grey and blurry during a round.
* Fixed: "Failed to host lobby" when 3-4 players were already in the party.
* Fixed: screen filter effects were missing for players 3 and 4.
* Works around a problem in ezz BOIII that froze about one game start in
  three as soon as the first player command reached the server
  ("Connection Interrupted"): a check inside the game clashes with ezz's
  own command handling, and the mod switches that check off.

Coming from 1.0: switch to ezz BOIII, delete XINPUT9_1_0.dll, extract this
zip over the old files and change the Steam launch option as shown under
START (-allowproxydlls is not needed with ezz BOIII).


REQUIREMENTS
------------
* Black Ops III on Steam with the ezz BOIII client, already installed and
  working (its boiii.exe in your Black Ops III folder). On its first start
  ezz BOIII installs the game version it needs - that is ezz's own setup,
  not this mod.
* One controller per player (Xbox or PlayStation)
* Steam Input switched ON for Black Ops III (this is Steam's default).
  With Steam Input off the game does not see any controller.


INSTALL
-------
Extract this zip into your Black Ops III folder - the folder that contains
ezz's boiii.exe. That is all.

The zip adds only these files:

    boiii\plugins\bo3_local_splitscreen.dll   (the mod; ezz loads it)
    boiii\ui_scripts\zz_table_insert\
    boiii\ui_scripts\zz_splitscreen\
    boiii\ui_scripts\zz_mplan\            (MULTIPLAYER in PLAY OFFLINE)

No game files are replaced by this mod. ezz BOIII lists the plugins it
loaded in boiii_players\plugins.log inside your Black Ops III folder
("Loaded: BO3 Local Splitscreen").


START
-----
Start ezz BOIII the way you always do - the mod loads with the game.

To start it with Steam's Play button: Steam library -> right-click
Call of Duty: Black Ops III -> Properties -> General -> Launch Options, and
enter (with the quotes):

    "<your Black Ops III folder>\boiii.exe" -launch %command%

Replace <your Black Ops III folder> with the full path of that folder - in
Steam: right-click the game -> Manage -> Browse local files shows it.
-launch skips ezz's launcher window; leave it out if you want to see it.


PLAY
----
1. Connect the controllers - before or after starting the game, both work.
2. Main menu: PLAY OFFLINE first, then ZOMBIES or MULTIPLAYER.
   Important: players 3 and 4 can only join the offline lobby. ezz BOIII's
   normal lobby is online, and online play allows two players per PC (the
   game's own rule).
3. In the lobby, every extra player presses A on their controller to join.
   B on that controller leaves again.
   ("Activate Splitscreen" also adds the next controller.)
4. Start the game. 3 players = 3 screens, 4 players = 2 x 2 screens.

Players 2-4 get a copy of player 1's weapon kits and stats every time they
join. They can change their kits for that session; player 1's own save is
never changed.


UNINSTALL
---------
Delete boiii\plugins\bo3_local_splitscreen.dll and the three ui_scripts
folders listed under INSTALL.


KNOWN ISSUES
------------
* Let players 3 and 4 join in the lobby (after choosing ZOMBIES or
  MULTIPLAYER), not in the main menu - a controller joining in the main
  menu can get a garbled name.
* 1.0 showed blocky, wrongly lit patches on the screens of players 3 and 4
  in some indoor areas (Der Eisendrache). Not re-tested since - reports
  welcome.
* Now and then the game closes by itself while it starts (before the main
  menu). Just start it again.
* Other maps than the tested ones (Der Eisendrache, Shadows of Evil and
  The Giant in Zombies; Safeguard and Splash in Multiplayer) should work
  but have not been tested yet - reports welcome.
* Starting ezz BOIII with -noplugins also switches this mod off.
* If the game or ezz BOIII changes to a version this mod does not know, the
  mod switches itself off and the game runs normally.


REPORTING A PROBLEM
-------------------
Say which map, how many players and what you did just before. If the game
crashed, ezz BOIII saves a crash file in the "minidumps" folder of its data
folder (%LOCALAPPDATA%\boiii\minidumps, ezz-crash-....zip) - attaching it
helps a lot.


THIRD-PARTY LICENSES
--------------------
This mod uses MinHook. Its license follows unchanged.

{MINHOOK_LICENSE}
