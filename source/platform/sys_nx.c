/* sys_nx.c -- Nintendo Switch system interface code, from uHexen2's sys_unix.c
 *
 * Copyright (C) 1996-1997  Id Software, Inc.
 * Copyright (C) 2001 contributors of the Anvil of Thyrion project
 * Copyright (C) 2004-2005  Steven Atkinson <stevenaaus@yahoo.com>
 * Copyright (C) 2005-2012  O.Sezer <sezero@users.sourceforge.net>
 * Copyright (C) 2026  HeXen2NX contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "quakedef.h"
#include "debuglog.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <setjmp.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <switch.h>

#include "nx_data.h"
#include "nx_log.h"
#include "nx_paths.h"

#define ENGINE_STACK_SIZE	0x800000	/* the main thread's is the loader's choice */
#define ENGINE_THREAD_PRIO	0x2C
#define ENGINE_MEM_ALLOC	0x8000000	/* 128 MB: engine hunk */
#define MAX_ARGS		64
#define PERF_LOG_SECONDS	10.0

cvar_t		sys_nostdout = {"sys_nostdout", "0", CVAR_NONE};
/* The frame's wait for the screen does the pacing: this only keeps a loop that
 * got ahead of it from spinning, and stays out of a frame's 16 ms otherwise. */
cvar_t		sys_throttle = {"sys_throttle", "0.005", CVAR_NONE};

qboolean	isDedicated;

static u64	start_tick;
static jmp_buf	exit_jump;
static int	exit_code;


/*
===============================================================================

FILE IO

===============================================================================
*/

int Sys_mkdir (const char *path, qboolean crash)
{
	int rc = mkdir (path, 0777);
	if (rc != 0 && errno == EEXIST)
	{
		struct stat st;
		if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
			rc = 0;
	}
	if (rc != 0 && crash)
	{
		rc = errno;
		Sys_Error("Unable to create directory %s: %s", path, strerror(rc));
	}
	return rc;
}

int Sys_rmdir (const char *path)
{
	return rmdir(path);
}

int Sys_unlink (const char *path)
{
	return unlink(path);
}

int Sys_rename (const char *oldp, const char *newp)
{
	return rename(oldp, newp);
}

long Sys_filesize (const char *path)
{
	struct stat	st;

	if (stat(path, &st) != 0)
		return -1;
	if (! S_ISREG(st.st_mode))
		return -1;

	return (long) st.st_size;
}

int Sys_FileType (const char *path)
{
	struct stat	st;

	if (stat(path, &st) != 0)
		return FS_ENT_NONE;
	if (S_ISDIR(st.st_mode))
		return FS_ENT_DIRECTORY;
	if (S_ISREG(st.st_mode))
		return FS_ENT_FILE;

	return FS_ENT_NONE;
}

#define	COPY_READ_BUFSIZE		0x10000	/* game saves copy a lot of small files */
int Sys_CopyFile (const char *frompath, const char *topath)
{
	static char	buf[COPY_READ_BUFSIZE];
	FILE	*in, *out;
	size_t	count;
	int	failed = 0;

	in = fopen (frompath, "rb");
	if (!in)
	{
		Con_Printf ("%s: unable to open %s\n", __thisfunc__, frompath);
		return 1;
	}
	out = fopen (topath, "wb");
	if (!out)
	{
		Con_Printf ("%s: unable to create %s\n", __thisfunc__, topath);
		fclose (in);
		return 1;
	}

	while ((count = fread(buf, 1, sizeof(buf), in)) > 0)
	{
		if (fwrite(buf, 1, count, out) != count)
		{
			failed = 1;
			break;
		}
	}
	if (ferror(in))
		failed = 1;

	fclose (in);
	fclose (out);

	return failed;
}

/*
=================================================
simplified findfirst/findnext implementation:
Sys_FindFirstFile and Sys_FindNextFile return
filenames only, not a dirent struct. this is
what we presently need in this engine.
=================================================
*/
static DIR		*finddir;
static struct dirent	*finddata;
static char		*findpath, *findpattern;

/* Newlib declares fnmatch() without having it. Names on the SD card keep
 * whatever case they came with, so matching here does not care for case. */
static qboolean NameMatches (const char *pattern, const char *name)
{
	for (; *pattern; pattern++, name++)
	{
		if (*pattern == '*')
		{
			while (*pattern == '*')
				pattern++;
			if (!*pattern)
				return true;
			for (; *name; name++)
			{
				if (NameMatches(pattern, name))
					return true;
			}
			return false;
		}
		if (!*name)
			return false;
		if (*pattern != '?' &&
		    tolower((unsigned char) *pattern) != tolower((unsigned char) *name))
			return false;
	}
	return (*name == 0);
}

const char *Sys_FindFirstFile (const char *path, const char *pattern)
{
	if (finddir)
		Sys_Error ("Sys_FindFirst without FindClose");

	finddir = opendir (path);
	if (!finddir)
		return NULL;

	findpattern = Z_Strdup (pattern);
	findpath = Z_Strdup (path);

	return Sys_FindNextFile();
}

const char *Sys_FindNextFile (void)
{
	struct stat	test;

	if (!finddir)
		return NULL;

	while ((finddata = readdir(finddir)) != NULL)
	{
		if (NameMatches(findpattern, finddata->d_name))
		{
			if ( (stat(va("%s/%s", findpath, finddata->d_name), &test) == 0)
						&& S_ISREG(test.st_mode))
				return finddata->d_name;
		}
	}

	return NULL;
}

void Sys_FindClose (void)
{
	if (finddir != NULL)
	{
		closedir(finddir);
		finddir = NULL;
	}
	if (findpath != NULL)
	{
		Z_Free (findpath);
		findpath = NULL;
	}
	if (findpattern != NULL)
	{
		Z_Free (findpattern);
		findpattern = NULL;
	}
}


/*
===============================================================================

SYSTEM IO

===============================================================================
*/

static FUNC_NORETURN void EngineExit (int code)
{
	exit_code = code;
	longjmp (exit_jump, 1);
}

/* Shown over whatever is on the screen, since nothing of ours is by now. */
static void ShowErrorApplet (const char *text)
{
	ErrorApplicationConfig	config;

	if (R_SUCCEEDED(errorApplicationCreate(&config, text, NULL)))
		errorApplicationShow(&config);
}

#define ERROR_PREFIX	"\nFATAL ERROR: "
void Sys_Error (const char *error, ...)
{
	va_list		argptr;
	char		text[MAX_PRINTMSG];

	host_parms->errstate++;

	va_start (argptr, error);
	q_vsnprintf (text, sizeof(text), error, argptr);
	va_end (argptr);

	if (con_debuglog)
	{
		LOG_Print (ERROR_PREFIX);
		LOG_Print (text);
		LOG_Print ("\n\n");
	}

	NX_LogPrintf ("%s%s\n\n", ERROR_PREFIX, text);
	NX_LogFlush ();

	Host_Shutdown ();

	ShowErrorApplet (text);

	EngineExit (1);
}

void Sys_PrintTerm (const char *msgtxt)
{
	if (sys_nostdout.integer)
		return;

	NX_LogWrite (msgtxt);
}

void Sys_Quit (void)
{
	Host_Shutdown();

	EngineExit (0);
}


/*
================
Sys_DoubleTime
================
*/
double Sys_DoubleTime (void)
{
	return (double) armTicksToNs(armGetSystemTick() - start_tick) / 1e9;
}

char *Sys_DateTimeString (char *buf)
{
	static char strbuf[24];
	time_t t;
	struct tm *l;

	if (!buf) buf = strbuf;

	t = time(NULL);
	l = localtime(&t);
	strftime (buf, 20, "%m/%d/%Y %H:%M:%S", l);

	return buf;
}

const char *Sys_ConsoleInput (void)
{
	return NULL;
}

void Sys_Sleep (unsigned long msecs)
{
	svcSleepThread ((s64) msecs * 1000000);
}

void Sys_SendKeyEvents (void)
{
	IN_SendKeyEvents();
}

char *Sys_GetClipboardData (void)
{
	return NULL;
}


/*
===============================================================================

MAIN

===============================================================================
*/

typedef struct
{
	int	argc;
	char	*argv[MAX_ARGS];
	char	text[1024];		/* what args.txt said */
} nx_args_t;

static quakeparms_t	parms;
static nx_args_t	engine_args;
static char		basedir[MAX_OSPATH];

/* An NRO only gets its own path as an argument: args.txt is the command line. */
static void BuildArguments (int argc, char **argv)
{
	nx_args_t	*a = &engine_args;
	FILE		*f;
	char		*tok;
	int		i;

	for (i = 0; i < argc && a->argc < MAX_ARGS; i++)
		a->argv[a->argc++] = argv[i];

	f = fopen (NX_ARGS_FILE, "r");
	if (f)
	{
		size_t	len = fread (a->text, 1, sizeof(a->text) - 1, f);

		fclose (f);
		a->text[len] = 0;
		for (tok = strtok(a->text, " \t\r\n"); tok && a->argc < MAX_ARGS; tok = strtok(NULL, " \t\r\n"))
			a->argv[a->argc++] = tok;
	}
}

static qboolean HasArgument (const char *name)
{
	int	i;

	for (i = 1; i < engine_args.argc; i++)
	{
		if (!strcmp(engine_args.argv[i], name))
			return true;
	}
	return false;
}

static void AddImpliedArguments (qboolean have_network)
{
	static char	portals[] = "-portals";
	static char	noudp[] = "-noudp";
	char		path[MAX_OSPATH];

	/* The mission pack has its own folder: having it means wanting it. */
	q_snprintf (path, sizeof(path), "%s/portals/pak3.pak", basedir);
	if (engine_args.argc < MAX_ARGS && !HasArgument("-noportals") &&
	    Sys_FileType(path) == FS_ENT_FILE)
	{
		engine_args.argv[engine_args.argc++] = portals;
	}

	if (engine_args.argc < MAX_ARGS && !have_network)
		engine_args.argv[engine_args.argc++] = noudp;
}

/* The game's folder is the NRO's, when the loader tells us, as long as the
 * game's files are next to it: a forwarder's NRO may be anywhere. */
static void FindBaseDirectory (int argc, char **argv)
{
	char	path[MAX_OSPATH];
	char	*slash;

	q_strlcpy (basedir, NX_GAME_DIR, sizeof(basedir));

	if (argc < 1 || !argv[0] || strncmp(argv[0], "sdmc:/", 6) != 0)
		return;
	q_strlcpy (path, argv[0], sizeof(path));
	slash = strrchr (path, '/');
	if (!slash)
		return;
	*slash = 0;
	if (Sys_FileType(va("%s/data1", path)) == FS_ENT_DIRECTORY)
		q_strlcpy (basedir, path, sizeof(basedir));
}

static void LogMemory (void)
{
	u64	total = 0, used = 0;

	svcGetInfo (&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
	svcGetInfo (&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
	NX_LogPrintf ("memory: %lu MB total, %lu MB used\n", total >> 20, used >> 20);
}

/* How it runs goes to the log, for a report to say more than "it felt slow". */
static void LogPerformance (double now, double frame_time)
{
	static double	window_start, slowest;
	static int	frames_at_start;

	if (frame_time > slowest)
		slowest = frame_time;

	if (now - window_start >= PERF_LOG_SECONDS)
	{
		NX_LogPrintf ("perf: %.1f fps, slowest frame %.1f ms, map %s\n",
				(host_framecount - frames_at_start) / (now - window_start),
				slowest * 1000.0, sv.active ? sv.name : "-");
		window_start = now;
		frames_at_start = host_framecount;
		slowest = 0;
	}
}

static void EngineMain (void)
{
	double	time, oldtime, newtime;
	int	i;

	memset (&parms, 0, sizeof(parms));
	parms.basedir = basedir;
	parms.userdir = basedir;
	parms.argc = engine_args.argc;
	parms.argv = engine_args.argv;
	parms.errstate = 0;
	host_parms = &parms;

	Sys_Printf ("Hammer of Thyrion, release %s (%s)\n", HOT_VERSION_STR, HOT_VERSION_REL_DATE);
	Sys_Printf ("running on %s engine %4.2f (%s)\n", ENGINE_NAME, ENGINE_VERSION, PLATFORM_STRING);

	LOG_Init (&parms);

	COM_ValidateByteorder ();

	isDedicated = false;

	if (Sys_FileType(va("%s/data1/pak0.pak", basedir)) != FS_ENT_FILE)
		Sys_Error ("Game files not found.\nCopy the data1 folder of your copy of HeXen II to %s/", basedir);

	parms.memsize = ENGINE_MEM_ALLOC;
	i = COM_CheckParm ("-heapsize");
	if (i && i < com_argc-1)
		parms.memsize = atoi (com_argv[i+1]) * 1024;

	parms.membase = malloc (parms.memsize);
	if (!parms.membase)
		Sys_Error ("Insufficient memory.");

	Host_Init();

	NX_LogPrintf ("engine ready\n");
	LogMemory ();

	oldtime = Sys_DoubleTime ();

	while (appletMainLoop())
	{
		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;

		Host_Frame (time);
		LogPerformance (newtime, time);

		if (time < sys_throttle.value)
			svcSleepThread (1000000);

		oldtime = newtime;
	}

	Sys_Quit ();
}

static void EngineThread (void *unused)
{
	(void) unused;

	/* Sys_Quit and Sys_Error land here: a thread may not call exit() in an NRO,
	 * for that returns to the loader on the wrong stack. */
	if (setjmp(exit_jump) == 0)
		EngineMain ();
}

#ifdef NX_HOSTCHECK	/* tools/hostcheck has its own, which calls this one */
#define main NX_Main
#endif

int main (int argc, char **argv)
{
	Thread	thread;
	Result	rc, net_rc;

	start_tick = armGetSystemTick();

	net_rc = socketInitializeDefault();
	NX_LogInit();
	if (R_FAILED(net_rc))
		NX_LogPrintf ("Sockets not available: 0x%x\n", net_rc);

	/* Closing from the HOME menu comes through the main loop, to save settings. */
	appletLockExit();

	FindBaseDirectory (argc, argv);
	NX_PrepareGameData (basedir);
	BuildArguments (argc, argv);
	AddImpliedArguments (R_SUCCEEDED(net_rc));
	NX_LogPrintf ("applet type: %d, basedir: %s, arguments: %d\n",
			(int) appletGetAppletType(), basedir, engine_args.argc - 1);
	LogMemory ();

	rc = threadCreate(&thread, EngineThread, NULL, NULL, ENGINE_STACK_SIZE, ENGINE_THREAD_PRIO, -2);
	if (R_SUCCEEDED(rc))
		rc = threadStart(&thread);
	if (R_FAILED(rc))
	{
		NX_LogPrintf ("Failed starting the engine thread: 0x%x\n", rc);
		exit_code = 1;
	}
	else
	{
		threadWaitForExit(&thread);
		threadClose(&thread);
	}

	NX_LogPrintf ("exit code %d\n", exit_code);
	NX_LogShutdown();
	appletUnlockExit();
	if (R_SUCCEEDED(net_rc))
		socketExit();

	return exit_code;
}
