# SD_LOG

The port writes its logs to `sd:/switch/HeXen2NX/SD_LOG/` on the console. Copy
them here to look at them. Nothing here but this file goes into the repository.

| File | What it is |
| --- | --- |
| `hexen2_log.txt` | Everything the engine printed during the last run |
| `hexen2_log_previous.txt` | The same, for the run before it |
| `hexen2_crash.txt` | Registers and a backtrace, written when the process faults |

A crash report's offsets map to source lines with the `.elf` of the same build:

```bash
aarch64-none-elf-addr2line -f -e build/HeXen2NX_sw.elf 0x12345
```
