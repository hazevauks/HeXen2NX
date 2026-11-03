/* nx_paths.h -- where the port keeps its files on the SD card. */

#ifndef NX_PATHS_H
#define NX_PATHS_H

#ifndef NX_GAME_DIR	/* tools/hostcheck points this at a folder on the PC */
#define NX_GAME_DIR	"sdmc:/switch/HeXen2NX"
#endif
#define NX_LOG_DIR	NX_GAME_DIR "/SD_LOG"
#define NX_LOG_FILE	"hexen2_log.txt"
#define NX_LOG_FILE_OLD	"hexen2_log_previous.txt"
#define NX_CRASH_FILE	"hexen2_crash.txt"
#define NX_ARGS_FILE	NX_GAME_DIR "/args.txt"

#endif	/* NX_PATHS_H */
