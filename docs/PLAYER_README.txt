BO3 Local Splitscreen 2.0  -  build {VERSION}
==============================================

Up to FOUR local split-screen players for Call of Duty: Black Ops III on PC -
Zombies and Multiplayer (offline custom games, bots work too). Works with the
ezz BOIII client. Local (offline) play only.

This is a BETA. Please report what works and what does not.


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

Coming from 1.0: switch to ezz BOIII, extract this zip over the old files
and change the Steam launch option as shown under START (-allowproxydlls
is not needed with ezz BOIII).


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

    XINPUT9_1_0.dll                       (next to boiii.exe)
    boiii\ui_scripts\zz_table_insert\
    boiii\ui_scripts\zz_splitscreen\
    boiii\ui_scripts\zz_mplan\            (MULTIPLAYER in PLAY OFFLINE)

No game files are replaced by this mod.


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
Delete XINPUT9_1_0.dll and the three folders listed under INSTALL.


KNOWN ISSUES
------------
* The 4th controller's first A press in the lobby is sometimes ignored -
  press A again.
* In the lobby, players 3 and 4 can show the wrong number after their name
  ("(2)" and "(3)" instead of "(3)" and "(4)"). Only the name text is
  affected.
* Multiplayer: the screens of players 3 and 4 can look too bright in some
  areas. Only the picture is affected.
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
* If another mod also ships an XINPUT9_1_0.dll, only one of the two can be
  installed.
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
