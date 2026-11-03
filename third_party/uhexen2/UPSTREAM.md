# uHexen2

The part of [uHexen2](https://github.com/sezero/uhexen2) (Hammer of Thyrion) this
port is built on, at commit `3d7ff45752edb8d1c8e0e02982f42bb54b1160a4` (2026-10-29).
It is GPL-2.0, like this project: see `docs/COPYING` and `docs/AUTHORS`.

Left out, for a Switch build has no use for them: drivers of other platforms, x86
assembly, the HexenWorld client and server, the game's QuakeC code and the tools.

The port's own code is in `source/`. What changed here is kept small, marked with
`PLATFORM_SWITCH`, and comes with this list:

| File | Change |
| --- | --- |
| `common/arch_def.h` | Detects the platform (`__SWITCH__`) |
| `common/net_sys.h` | libnx's `struct sockaddr` is BSD's, with `sa_len` |
| `engine/h2shared/sys.h` | No per-user directories |
| `engine/h2shared/snd_sys.c`, `snd_sys.h` | Registers the `snd_nx` driver |
| `engine/h2shared/keys.h`, `engine/hexen2/keys.c` | Names the Switch's buttons |
| `engine/h2shared/cmd.c` | Runs the console defaults after `default.cfg` |
