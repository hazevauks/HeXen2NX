# Status

The port goes one stage at a time, each checked before the next.

| Stage | State | Checked by |
| --- | --- | --- |
| 1. Source setup, asset layout, file system | Done | Host check, on the real game files |
| 2. Build with devkitA64, CMake and libnx | Done | Local build and GitHub Actions |
| 3. Boot with the fallback renderer | **Waiting for a console** | Host check; not yet on hardware |
| 4. Native deko3D renderer | Not started | |
| 5. Performance and Switch tuning | Not started | |

## What was checked on a PC

[tools/hostcheck](../tools/hostcheck/README.md) runs the port's own code, with a
stand-in for libnx, on the game's real files. With it:

- Both of the game's pak files mount and pass the engine's check for the retail
  version, and `progs.dat` loads.
- The main menu leads through class and difficulty selection into the first map.
- All 42 maps of the base game load and draw, without an error in the log.
- Saving and loading work, on the hub system's files.
- The sticks walk and turn the player, and bound buttons act.
- The mixer produces sound, and it stops and starts where the game says.
- A build that traps on any misaligned memory access runs all of this without one.

## What only a console can check

- That it starts at all: libnx's services, the framebuffer, audout, HID and the
  thread and exit handling are stand-ins on a PC.
- The music: the host check has no Vorbis decoder, so it only shows that the game
  asks for the right names.
- Speed on the Switch's CPU, and how much memory it really has to work with.
- The crash reports, and the error screen for missing game files.
- Multiplayer over the network, which the host check leaves out.

## What has no test data yet

The copy of the game used so far has no Portal of Praevus (`portals/`), so nothing
of the expansion has run. The code that starts it comes from uHexen2 as it is.
