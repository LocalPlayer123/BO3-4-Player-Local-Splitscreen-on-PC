BO3 Local Splitscreen  -  build {VERSION}
==========================================

Up to FOUR local split-screen players for Call of Duty: Black Ops III
Zombies on PC. Works with the BOIII client. Local (offline) play only.

This is a BETA. Please report what works and what does not.


REQUIREMENTS
------------
* Black Ops III with the BOIII client, already installed and working
* One controller per player (Xbox or PlayStation)
* Steam Input switched ON for Black Ops III (this is Steam's default).
  With Steam Input off the game does not see any controller.


INSTALL
-------
Extract this zip into your Black Ops III folder - the folder that contains
boiii.exe. That is all.


START FROM STEAM
----------------
Once: Steam library -> right-click Call of Duty: Black Ops III ->
Properties -> General -> Launch Options, and enter (with the quotes):

    "<your Black Ops III folder>\boiii.exe" -allowproxydlls %command%

Replace <your Black Ops III folder> with the full path of that folder - in
Steam: right-click the game -> Manage -> Browse local files shows it.

From then on the normal Play button in Steam starts BOIII with the mod.

Why -allowproxydlls: since July 2026 BOIII ignores DLLs in the game folder
that have the same name as a Windows DLL, unless it is started with this
switch. Without it the mod simply does not load (the game runs normally).
Double-clicking boiii.exe therefore does not load the mod.

The zip adds only these files:

    XINPUT9_1_0.dll                       (next to boiii.exe)
    boiii\ui_scripts\zz_table_insert\
    boiii\ui_scripts\zz_splitscreen\

No game files are replaced, and BOIII updates do not remove the mod.


PLAY
----
1. Connect the controllers - before or after starting the game, both work.
2. Main menu: PLAY OFFLINE -> ZOMBIES.
3. In the lobby, every extra player presses A on their controller to join.
   B on that controller leaves again.
   ("Activate Splitscreen" also adds the next controller.)
4. Start the game. 3 players = 3 screens, 4 players = 2 x 2 screens.

Players 2-4 get a copy of player 1's weapon kits and stats every time they
join. They can change their kits for that session; player 1's own save is
never changed.

Online play stays at two players per PC - that is the game's own rule.


UNINSTALL
---------
Delete XINPUT9_1_0.dll and the two folders listed under INSTALL.


KNOWN ISSUES
------------
* Now and then the game closes by itself while it starts (before the main
  menu). Just start it again. We are still investigating this.
* With 3 or 4 players, the sun shadows of players 2-4 can shimmer slightly.
* With 4 players, the screens of players 3 and 4 can show blocky, wrongly lit
  patches on walls in some indoor areas (seen in Der Eisendrache). Only the
  picture is affected; we are working on it.
* Tested on Der Eisendrache, Shadows of Evil and The Giant. Other maps
  should work but have not been tested yet - reports welcome.
* If another mod also ships an XINPUT9_1_0.dll, only one of the two can be
  installed.
* If the game or BOIII changes to a version this mod does not know, the mod
  switches itself off and the game runs normally.


REPORTING A PROBLEM
-------------------
Say which map, how many players and what you did just before. If the game
crashed, BOIII saves a crash file in the "minidumps" folder inside your
Black Ops III folder (boiii-crash-....zip) - attaching it helps a lot.


THIRD-PARTY LICENSES
--------------------
This mod uses MinHook. Its license follows unchanged.

{MINHOOK_LICENSE}
