Nuvie for Nintendo 3DS — test build
===================================

Nuvie plays Ultima VI: The False Prophet (and, in principle, Martian
Dreams and The Savage Empire) from the original game files.

Made for the NEW 3DS / NEW 2DS XL. It may start on an original 3DS,
but that has not been tested.

SD card layout
--------------
  /3ds/nuvie/nuvie.3dsx          the program (launch from the Homebrew Launcher)
  /3ds/nuvie/ultima6/            the Ultima VI game files - see below
  /3ds/nuvie/u6_save/            saved games (created for you)
  /3ds/nuvie/nuvie.cfg           settings (created on first run)
  /3ds/nuvie/nuvie_log.txt       log files (send these if something goes wrong)
  /3ds/nuvie/nuvie_err.txt

The game files
--------------
The ultima6 folder must contain the DOS game files themselves, directly:
MAP, CHUNKS, U6.SET, U6PAL, *.M, the SAVEGAME folder, and so on (about
170 files). Not the installer, and not a folder inside a folder.

  GOG on Windows:  copy everything from the game's install folder.
  GOG on Mac:      the files are inside the app - right-click the app,
                   "Show Package Contents", then Contents/Resources/game.
                   Copy what is inside "game". Skip Extras (manual,
                   DOSBox) and the rest of Contents.

If the folder is wrong, the program says so in nuvie_log.txt
("no U6.SET here"). Music and sound effects come from these files;
nothing else is needed.

Screens
-------
  Normal:   game on the TOP screen, a touch keyboard on the BOTTOM screen.
  Swapped:  press SELECT — the game moves to the bottom screen so you can
            tap it directly with the stylus. The top screen shows a live
            copy of the game. Press SELECT again to move it back up.
            The bottom screen is narrower, so the game is shrunk a little
            to fit while it is down there.

Layout
------
The game uses the whole top screen (400x240): the original side panels,
with a bigger map window. Two other layouts are available in
Start -> Video Options -> "Game style" (takes effect after a restart):
  original+   the default described above ("classic wide")
  new         the map fills the screen; inventory, text and the command
              bar pop up over it (Nuvie's modern look)
  original    the plain 320x200 DOS screen with black borders

Controls
--------
  Circle pad / D-pad   walk; after a command, pick the direction
  C-stick              move the mouse pointer on the game screen
  L                    left mouse button: tap = look, double-tap = use,
                       hold + move = drag an item
  R                    right mouse button: hold = walk toward the pointer
  A                    Enter (confirm the target / do it)
  B                    Space (cancel a command / pass)
  X                    your inventory; press again for the party list
  Y                    Look
  ZL                   Talk
  ZR                   Use
  Start                Esc: close a window, or the game menu (save, options)
  Select               swap the game between the two screens
  Touch keyboard       the Ultima VI commands along the top (Attack, Cast,
                       Talk, Look, Get, Drop, Move, Use, Rest, Combat),
                       letters, numbers, Esc, Shift (^), Space,
                       Backspace (<), Enter, MAP
  Home                 quit

Tapping a command on the keyboard works like tapping its icon on the
game's own command bar: the bottom screen switches to a view of the
game, you tap the target, and the keyboard comes back when the command
is done (or when a conversation starts). CANCEL under that view backs
out of the command. MAP opens the same view whenever you want it - tap
to walk or look, double-tap to use, KEYBOARD to return.

Without the touch screen, the original way works too: press a command
(Y for Look, ZL for Talk...), a crosshair appears next to you - move it
with the D-pad and press A, or move the pointer with the C-stick and
press L on the target. Conversations: type the keyword on the touch
keyboard and press A (or tap ENTER). Shift (^) applies to the next
letter only. Numbers 1-8 pick a party member for solo mode, 0 goes
back to party mode.

Known gaps in this first build: the title menu does not show which line
is highlighted (the choice still works - four D-pad downs then A is
Journey Onward), and the game runs at 20 frames a second like the
original Nuvie, so the pointer is a little less smooth than in Exult.
