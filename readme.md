<p align="center"><img src="distribution/amiga/screenshots/title.png" width="640" alt="OpenRCT2 title screen on AmigaOS, 640x480, 8-bit RTG"></p>

<h1 align="center">OpenRCT2 for AmigaOS (68k)</h1>

<p align="center">RollerCoaster Tycoon 2, running natively on a big-endian 68k Amiga.<br>
A port of <a href="https://github.com/OpenRCT2/OpenRCT2">OpenRCT2</a> to AmigaOS 3.x — Picasso96/CyberGraphX display, Intuition input, AHI sound.</p>

<p align="center"><a href="https://github.com/thomas-luebker/openrct2-amiga/releases"><b>Download the tester build</b></a> · <a href="distribution/amiga/TESTER-GUIDE.md">Tester guide</a> · <a href="distribution/amiga/BUILDING.md">Building</a> · <a href="AMIGA_PORT.md">How the port works</a></p>

---

## What this is

OpenRCT2 is the open-source re-implementation of RollerCoaster Tycoon 2. This repository carries the
`amiga` branch: the same engine cross-compiled for m68k AmigaOS 3.x with bebbo's GCC 16, no SDL, no
threads, soft float, big-endian. It is a fork; upstream declined to merge it, so it lives here.

| | |
|---|---|
| ![RCT1 Forest Frontiers](distribution/amiga/screenshots/rct1-forest-frontiers.png) | ![RCT2 Electric Fields](distribution/amiga/screenshots/rct2-electric-fields.png) |
| RCT1 *Forest Frontiers* | RCT2 *Electric Fields* |

## Status

- **Plays.** RCT2 scenarios (`.sc6`, incl. Wacky Worlds / Time Twister) and RCT1 scenarios (`.sc4`,
  incl. Added Attractions / Loopy Landscapes) load natively and play; `.park` saves load.
- **Bit-exact.** The simulation produces the same entity checksums as the x86-64/arm64 build over
  5000 ticks, and scenario loads are identical at tick 0 — verified on a 68040 against a native build
  of the same tree.
- **Display and input** on an 8-bit RTG screen (640×480 recommended), mouse and keyboard through
  Intuition. About 25 fps at 640×480 on an emulated 68040; PiStorm and Vampire are faster.
- **Sound and music** through `ahi.device`: the RCT2 sound bank and `css*.dat` music. OpenRCT2's own
  Ogg music packs are not supported (no Vorbis decoder in this build).
- Not yet: mouse cursor shapes and clipboard, networking, scripting, hardware-FPU builds.

## Requirements

- AmigaOS 3.2 (tested on 3.2.3); an RTG card with a Picasso96 or CyberGraphX driver and an 8-bit
  640×480 mode (AGA/ECS-only machines are not supported).
- 68040/68060, PiStorm (Emu68) or Vampire/Apollo; about 300 MB of free Fast RAM.
- AHI for sound (optional; silent without it).
- The data files of the original RollerCoaster Tycoon 2 (GOG, Steam or CD). Not included.

See the [tester guide](distribution/amiga/TESTER-GUIDE.md) for installation.

## Building

Cross-compile on Linux or macOS: [distribution/amiga/BUILDING.md](distribution/amiga/BUILDING.md)
covers the compiler, the four cross-built dependencies (`build-deps.sh`), `configure.sh` and packaging.

## Credits and licence

OpenRCT2 is © the OpenRCT2 developers, GPLv3 — see [README.upstream.md](README.upstream.md),
[contributors.md](contributors.md) and [licence.txt](licence.txt). RollerCoaster Tycoon 2 is © Chris Sawyer
and Atari; you need your own copy. The AmigaOS port is by Thomas Lübker, developed with Claude Code;
it is licensed under the same GPLv3.
