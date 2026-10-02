BO3 Local Splitscreen 2.6.9  -  build {VERSION}
================================================

Up to FOUR local split-screen players for Call of Duty: Black Ops III on PC -
Zombies and Multiplayer (offline custom games, bots work too). An ezz BOIII
plugin. Local (offline) play only.

This is a BETA. Please report what works and what does not.


NEW IN 2.6.9
------------
* New: choose which controller each player uses - Options > Controls >
  Split Screen now lists "Player 1 controller" to "Player 4 controller"
  (players 3/4 while they are in the game). Picking a controller that
  another player has swaps the two. The game had this for player 2 only.
* Changed: the sprint fix for players 3 and 4 from 2.6.7 is replaced by a
  smaller one by rdevathu (GitHub), who found the same bug on their own
  and tested it with four controllers: only the stick-click limit the game
  reads for players 3/4 is corrected; their settings are no longer
  replaced by a copy of player 1's.
* The download from Nexus Mods contains no update check (Nexus rules do
  not allow mods that connect to the internet); the GitHub download keeps
  the update notice.

Updating from 2.2 - 2.6.8: extract this zip over the old files.


NEW IN 2.6.8
------------
* Fixed: after the lobby had 3 or 4 players, going back to fewer players
  and starting a match crashed at the end of the loading screen
  (BlackOps3.exe+0x140A11EC0). The game was still set up for the larger
  number of players. Now every match is set up for the players it starts
  with, as on console - fewer or more than the last one.
* Changed: the lobby button now reads DEACTIVATE SPLITSCREEN as soon as
  extra players are in and no further controller is waiting to join, so you
  can always go back to one player (then ACTIVATE adds player 2 again).
  Before, it kept saying ACTIVATE with 2 or 3 players. Players can also
  leave on their own with B.
* Changed: a player whose controller switches off in the lobby keeps their
  place, as on console; the next controller you connect takes it. Before,
  that player was removed after half a second.
* New: update notice. Once per start the mod asks GitHub whether a newer
  version exists and, if so, shows a message in the menu. Nothing about you
  or your PC is sent. To switch it off, add -splitscreen_noupdate to the
  launch options.

Updating from 2.2 - 2.6.7: extract this zip over the old files.


NEW IN 2.6.7
------------
* Fixed: players 3 and 4 could not sprint with a controller (click the
  left stick), in Zombies and Multiplayer, although the click worked in
  menus. On PC the game ignores a stick click while that stick is pushed
  further than a limit stored in the player's settings. Players 3 and 4
  started with empty settings, so the limit was 0 and pushing forward -
  which you do to sprint - cancelled the click. They now start from
  player 1's settings. If you switched to the lefty layout or a Steam
  remap to work around it, you can switch back.
* Fixed: switching on a controller during a match could freeze the game
  (error EXCEPTION_BREAKPOINT) when the same session had 3 or 4 players
  before. The menu system kept the earlier player's screen marked as in
  use and sent the new controller to a menu that no longer existed.

Updating from 2.2 - 2.6.6: extract this zip over the old files.


NEW IN 2.6.6
------------
* Fixed: with 3 or 4 players some models were missing for everyone, most
  visibly the cars on Nuketown. The game loads detailed models on demand,
  based on where the players look from, and kept those positions for only
  two players. Player 3's position overwrote the game's own count, after
  which nothing new was loaded any more. It now has room for all players.
* Fixed: players 2-4 could not open the scoreboard (Back / View button) or
  the menu (Start / Menu button) with a controller. On PC only player 1 got
  these two buttons; now every player has them, as on console.
* Changed: with 2 or more players, Start (or ESC) opens the menu only for
  the player who pressed it, and the match keeps running - as in console
  Multiplayer. Before, one player's menu paused the match and opened the
  menu on every screen.
* Fixed: the background blur behind in-game menus was missing, and with 3
  or 4 players it wrote into memory it does not own.

Updating from 2.2 - 2.6.5: extract this zip over the old files.


NEW IN 2.6.5
------------
* Fixed: a round could stay on the loading screen forever (bar full,
  nothing happens) - every time, once your stats had grown large enough
  through playing. This is a bug in the PC game itself, not in the mod:
  before a round each player's stats are sent to the game's server in
  small pieces; the server reports the missing pieces as a number and the
  game read that number back wrongly, so one piece (number 63) was never
  sent again. The mod now reads it correctly. It affected solo games too.

Updating from 2.2 - 2.6.4: extract this zip over the old files.


NEW IN 2.6.4
------------
* Fixed: with 3 or 4 players the game could close without an error
  message when many destroyed pieces were lying around (seen on Nuketown
  in Multiplayer). The game tidies up old pieces by their distance to the
  players, and its list of player positions only had room for two.
  Players 3 and 4 overwrote the game's own stack check, and Windows
  closed the game. The tidy-up now uses players 1 and 2 only; pieces
  near players 3 and 4 may disappear a little sooner.

Updating from 2.2 - 2.6.3: extract this zip over the old files.


NEW IN 2.6.3
------------
* Fixed: Multiplayer with 2 or more players could crash at the end of the
  loading screen or during the match ("EXCEPTION_ACCESS_VIOLATION",
  BlackOps3.exe+0x142206280), seen with keyboard and mouse for player 1
  and a controller for player 2. The game marked a local player's
  stats as changed and then read that player's server-side stats, which
  local players do not have on PC. Those changes are now skipped instead.

Updating from 2.2 - 2.6.2: extract this zip over the old files.


NEW IN 2.6.2
------------
* Fixed: a controller plugged in while you played alone could become a
  second player instead of yours, and a button press on it (even the D-pad)
  made that second player join. The cause: the game saved the number of
  split-screen players in boiii_players\user\config.cfg, so the first start
  after a game with 2 or more players began with that number although you
  were alone, and the game handed the next controller to player 2. The
  number is no longer saved (as on console), and a newly connected
  controller goes to player 2 only when a player 2 has really joined.
* Changed: in the offline lobby an extra controller joins with A only, as
  described below. Other buttons no longer make it join.

Updating from 2.2 - 2.6.1: extract this zip over the old files.


NEW IN 2.6.1
------------
* Fixed: Multiplayer with 3 or 4 players could crash now and then while
  the game drew player names ("EXCEPTION_ACCESS_VIOLATION"). The name drawing kept six of its lists for two players
  only, so player 3 and 4 wrote into the neighbouring lists - one of them
  the list of names to draw. All six now have room for four players. The
  crash was in every earlier version.

Updating from 2.2 - 2.6: extract this zip over the old files.


NEW IN 2.6
----------
* Lens flares for players 3 and 4: the sun and bright lamps now flare on
  their screens too, and the flares fade behind cover as on the screens
  of players 1 and 2. Until now the game only kept lens flares for two
  players, so the mod had switched them off for players 3 and 4.

Updating from 2.2 - 2.5.1: extract this zip over the old files.


NEW IN 2.5.1
------------
* Nothing changes in the game: 2.5.1 plays like 2.5.
* Internal cleanup for the ezz BOIII developers: the game collects each
  player's stat changes in a small list before it writes them to that
  player's stats. For players 3 and 4 that list is now kept by a small C++
  function of the mod instead of patched game code. Before the switch,
  both versions ran side by side and gave identical results (440 of 440).

Updating from 2.2 - 2.5: extract this zip over the old files.


NEW IN 2.5
----------
* Nothing changes in the game: 2.5 plays like 2.4.1.
* Internal cleanup for the ezz BOIII developers: the Zombies barricade
  records (window boards, mystery box) of players 3 and 4 are handled by
  two small C++ functions of the mod instead of patched game code. Before
  the switch, both versions ran side by side and gave identical results
  (220 of 220).

Updating from 2.2 - 2.4.1: extract this zip over the old files.


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
   ("Activate Splitscreen" also adds the next waiting controller;
   "Deactivate Splitscreen" removes every extra player.)
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
* Connect all controllers before players join. Steam numbers the
  controllers (connected controllers first), and a controller connected
  later can take another player's place. If that happens, restart the game.
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
