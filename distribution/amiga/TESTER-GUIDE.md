# OpenRCT2 on AmigaOS 3.2 (68k) — tester guide

*Build: test14. This is an early, unfinished port. You are testing it — thank you.*

OpenRCT2 is the open-source re-implementation of RollerCoaster Tycoon 2. This build is a
big-endian port of the upstream C++20 engine to 68k AmigaOS, with an Intuition/RTG display
backend instead of SDL. The game logic is bit-exact with the PC version (verified over 5000
simulated ticks); the display and input layer is new.

## 0. Where to get builds

Tester builds are published on GitHub only: https://github.com/thomas-luebker/openrct2-amiga/releases
Each release has the full archive, a small update archive and this guide. Every build is checked on an
emulated 68040 before release: sound (AHI opens and the mixed output is audible), frame rate in a fresh
scenario and while scrolling, and the simulation cost of a park with 1,500 guests.

## 1. What you need

| | Requirement |
|---|---|
| OS | AmigaOS 3.2 (tested on 3.2.3). Other 3.x may work, untested. |
| Graphics | An RTG card with a **Picasso96 or CyberGraphX** driver offering an **8-bit 640×480** mode. AGA/ECS-only machines are not supported. |
| CPU | 68040/68060, **PiStorm** (Emu68) or **Vampire/Apollo**. Tested on an emulated 68040. A 68020/030 runs it, slowly. No FPU needed. |
| RAM | About **320 MB free Fast RAM** for a normal park; a very large park with 1,500 guests reaches ~240 MB and its autosave temporarily needs ~75 MB more, so 512 MB is recommended. If memory is tight, set `autosave = 5` (never) in `user/config.ini`. |
| Disk | ~80 MB for the game, plus your RCT2 data (~150 MB, or ~630 MB with all the ride music). |
| RCT2 data | The **original RollerCoaster Tycoon 2** files (GOG, Steam or CD). Not included, not redistributable. |
| Sound | **AHI** installed, with unit 0 configured (optional: the game is silent without it). |

## 2. Get your RCT2 data onto the Amiga

From your RCT2 installation copy these folders to the Amiga, keeping their names:

```
Data/      ObjData/      Scenarios/      Tracks/
```

Sound effects come from `Data/css1.dat` (5 MB) and the title music from `Data/css17.dat`. The
other `css*.dat` files are the ride music (~480 MB in total): copy the ones you want, or all. Copy over the network, a USB stick, or a CF card.
Example target: `Work:Games/RCT2/`.

## 2b. What to expect at the title screen

The title screen tours the empty RCT2 beginner scenarios (Crazy Castle, Electric Fields, ...)
instead of the large community parks the PC version shows: those need minutes to load and run at
one or two frames per second on a 68k. Reaching the title screen still takes a while (about half a
minute on a fast machine) because an RCT2 scenario references some 600 objects that all have to be
read from disk. Starting a scenario afterwards is much quicker, since most objects are already loaded.

## 2c. Scrolling

Scrolling with the mouse at the screen edge or with the arrow keys now moves the view by the same
amount per second whatever the frame rate. Earlier builds moved one fixed step per drawn frame, which
crawled at 15 fps and jumped at the map edge.

## 2d. Solid windows

Windows and the two bars are drawn opaque on the Amiga (`solid_windows = true` in `user/config.ini`).
The PC version treats every window as transparent, so the landscape underneath every open window and
both bars was painted again on every scrolled frame; that cost more than half of the scrolling frame
rate. Set `solid_windows = false` if you prefer the transparent look.

## 2e. The second program file

`bin/openrct2-o2` is the same program compiled for smaller code (-O2 instead of -O3). On real
68060 and 68080 CPUs with their small caches it may run faster; on a PiStorm or an emulator it
should make no difference. To try it, rename `bin/openrct2` to `bin/openrct2-o3` and
`bin/openrct2-o2` to `bin/openrct2`, then compare the fps counter in the same spot of the same park.

## 2f. Big parks and guest path finding

With more than a few hundred guests the simulation itself becomes the limit: guests that cannot
find a route to their goal within the game's junction limit run a search of up to 15,000 tiles on
the PC, and on a 68k two such searches per tick were 80% of the whole simulation. This build limits
those searches to 4,000 tiles (`pathfind_tile_budget` in `user/config.ini`); a search that reaches
its goal is unaffected, only the fallback direction of a failed search can differ. Set it to 15000
for exact PC behaviour, or lower (2000) if a big park still runs in slow motion.

## 3. Install the game

1. Extract the archive where there is room, e.g. `Work:Games/`. In a Shell:
   ```
   LhA x OpenRCT2-0.5.5-test7-*-amiga68k.lha Work:Games/
   ```
   You get a drawer `OpenRCT2` with an icon.

   **Updating from an earlier test build:** use the small `...-update.lha` instead. It contains only
   the program, the launcher and the docs. Extract it over your existing `OpenRCT2` drawer; your
   `user/config.ini` and the caches in `user/` stay, so no settings are lost and the slow first
   start is not repeated.
2. Tell the game where your RCT2 data is with an **assign** (this is how the launcher finds it):
   ```
   Assign RCT2: Work:Games/RCT2
   ```
   Put that line into `S:User-Startup` so it survives a reboot.
3. Start it: double-click the **OpenRCT2** icon in the drawer, or in a Shell:
   ```
   CD Work:Games/OpenRCT2
   Execute OpenRCT2
   ```
   (The launcher also accepts the folder as an argument: `Execute OpenRCT2 Work:Games/RCT2/`.)

## 4. First start — be patient

- The first start **builds an index** of all game objects and scenarios into the `user` drawer.
  This takes 1–2 minutes on a 68040 and is much faster on every later start.
- Loading the title screen takes 40–60 s on an emulated 68040.
- Then the title sequence plays (parks fly by) and the main menu appears in the middle:
  **New Game**, **Load Game**, **Toolbox**. Top-right: **Options**.
- Expect **~25–30 fps at 640×480** on a fast machine.

## 5. Playing

- **New Game** lists the RollerCoaster Tycoon 1 and 2 scenarios found in your Scenarios folder
  (and Wacky Worlds / Time Twister if you own them), in tabs on the left. Click a scenario to start it; loading takes about a minute.
- Standard OpenRCT2 controls: left-click to select/build, right-drag to scroll the map, mouse
  wheel or the magnifier icons to zoom, `Space`/the top-left icon to pause.
- The disk icon (third toolbar button) has **Save**, **Load**, and **Quit**. Saving works but has
  had little testing — save often and expect surprises.
- The `user/config.ini` file holds settings. `window_width`/`window_height` change the screen
  size; frame rate falls with pixel count (1280×720 runs at ~9 fps on an emulated 68040), so
  640×480 is the recommended default. Set `show_fps = true` to see the frame counter.

## 6. Known limitations of this build

- **Sound and music play through AHI** (`ahi.device` unit 0). Install AHI (Aminet `mus/misc/ahiusr_4.18.lha`
  works on 3.2) and configure unit 0 in `Prefs/AHI` — Paula on a plain Amiga, or the Vampire/PiStorm
  driver where one exists. Without AHI the game runs silently. Sound was verified by capturing the mixed stream on an emulator; real-hardware AHI playback is what this build needs testers for -- please report whether you hear the title music and clicks. Music needs the `css*.dat` files from
  your RCT2 `Data` folder; the optional OpenRCT2 music packs (OGG) are not supported yet.
- **RollerCoaster Tycoon 1 scenarios (`.SC4`) play** and their simulation is bit-exact with the
  PC build. Rides that only exist in RCT1 use fallback RCT2 graphics unless you also own RCT1 and
  set `rct1_path` in `user/config.ini` to its folder; the log line *"Park has objects which require
  RCT1 linked"* means exactly that and is not an error.
- Only the standard mouse pointer (no custom cursors); no clipboard.
- Multiplayer, plugins and the scenario editor are untested.
- The park load is CPU-bound (object loading, tile import); a real 68060 or PiStorm will be
  faster than the emulated 68040 the numbers above come from.

## 7. If it is slow: send us a performance trace

If the frame rate is low, please run it once with the trace on and send us the file, it tells us where
the time goes (and, if the game hangs during loading, where it stopped):

```
SetEnv OPENRCT2_TRACE T:openrct2-trace.txt
Execute OpenRCT2
```

Let the title screen run for a minute, quit, and send `T:openrct2-trace.txt` together with your
hardware (CPU/accelerator, graphics card and driver, screen mode). The lines that matter look like
`gfx: 100 frames in 4123 ms = 24.25 fps; rasterise 2100 ms, blit 900 ms for 30000 kpx (chunky)` and
`frame: per 20 draws: events 40 ms, sleep 0 ms (0 times), ticks 800 ms (80 ticks), input+windows 20 ms, draw 700 ms`
(where the frame time goes: simulation ticks, drawing, input), plus `tick: per 40 ticks: ...` with the
cost of each simulation subsystem. The title screen park is the heaviest thing in the game; please also
report the frame rate inside a freshly started scenario, that is what matters for playing.
If the game hangs while loading (one PiStorm tester saw it stop at "Loading title sequence (30%)" and
the machine's disk access died with it), please try once with sound disabled to isolate the audio path:
`SetEnv OPENRCT2_NO_AUDIO 1` before starting. If that loads, the hang is in the audio code; send the
trace of the hanging run either way.

An experimental direct framebuffer write can be tried with `SetEnv OPENRCT2_BLIT direct` before starting
(it hung one PiStorm during loading, so it is off by default; tell us what it does on your card). A smaller
screen mode (`window_width`/`window_height` in `user/config.ini`) scales the cost down almost linearly.

## 8. Reporting problems

Send: your machine (CPU, RAM, graphics card and driver, OS version), what you did, what you
expected, what happened, and a screenshot if you can.

For crashes or hangs, switch on the trace log **before** starting the game:

```
SetEnv OPENRCT2_TRACE T:openrct2-trace.txt
```

Reproduce the problem, then send `T:openrct2-trace.txt`. Switch it off again afterwards
(it slows loading down noticeably):

```
UnSetEnv OPENRCT2_TRACE
```

## 9. Uninstall

Delete the `OpenRCT2` drawer and remove the `Assign RCT2:` line from `S:User-Startup`.
Nothing is written outside the drawer.

---
OpenRCT2 is GPLv3. This port lives on the `amiga` branch of the project repository.
