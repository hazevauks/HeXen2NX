# Host check

Runs the port on a PC, on the game's real files, with no console and no emulator.

`shim/` stands in for libnx, so that `source/platform/` and the engine compile
unchanged for the PC. The game plays a scenario instead of a player, on a virtual
clock: a frame is one of a 60 Hz console, however fast the PC is, so a run gives the
same frames every time. What it draws goes to image files, what it plays to a raw
audio file, and the log to `SD_LOG` in the game's folder, as on the console.

It needs a POSIX environment with GCC (MSYS2's `gcc` package, or any Linux).

```bash
python tools/package_sd.py "/path/to/your/copy/of/HeXen II"
cp -r SD_CARD/switch/HeXen2NX /some/scratch/folder
tools/hostcheck/build.sh /some/scratch/folder/HeXen2NX
mkdir out
build_host/hexen2_host tools/hostcheck/scenarios/smoke.txt out -noudp
```

Use a copy of the game's folder: the game writes its settings and saves there.

| Scenario | What it does |
| --- | --- |
| `scenarios/smoke.txt` | Through the menus into a new game; walks, turns, attacks, opens the menu |
| `scenarios/all_maps.txt` | Loads all maps of the base game, then saves and loads |

For misaligned memory accesses, which the console can be less forgiving of in GPU
memory, build with traps. The program reports where one happened:

```bash
FORCE=1 tools/hostcheck/build.sh <game folder> -fsanitize=alignment -fsanitize-undefined-trap-on-error
```

It leaves out what has no meaning on a PC: the crash handler, and the Vorbis decoder
and network, for which there are no libraries of the console's kind. It tells nothing
about the Switch's speed, or about its GPU.
