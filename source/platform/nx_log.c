/* nx_log.c -- log file on the SD card, and nxlink when connected.
 *
 * Copyright (C) 2026  HeXen2NX contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <switch.h>

#include "nx_log.h"
#include "nx_paths.h"

static FILE	*log_file;
static int	nxlink_fd = -1;

void NX_LogInit (void)
{
	char		path[256];
	time_t		now = time(NULL);
	const struct tm	*t = localtime(&now);

	mkdir(NX_GAME_DIR, 0777);
	mkdir(NX_LOG_DIR, 0777);

	/* Keep the previous run: it is the one that crashed. */
	snprintf(path, sizeof(path), "%s/%s", NX_LOG_DIR, NX_LOG_FILE);
	remove(NX_LOG_DIR "/" NX_LOG_FILE_OLD);
	rename(path, NX_LOG_DIR "/" NX_LOG_FILE_OLD);

	log_file = fopen(path, "w");
	if (log_file)
		setvbuf(log_file, NULL, _IOLBF, 4096);

	if (__nxlink_host.s_addr != 0)
		nxlink_fd = nxlinkStdio();

	/* Without nxlink, what libraries print goes to the log file too. */
	if (nxlink_fd < 0)
	{
		freopen(path, "a", stdout);
		freopen(path, "a", stderr);
		setvbuf(stdout, NULL, _IOLBF, 1024);
		setvbuf(stderr, NULL, _IONBF, 0);
	}

	NX_LogPrintf("HeXen2NX log, %04d-%02d-%02d %02d:%02d:%02d, nxlink %s\n",
			t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
			t->tm_hour, t->tm_min, t->tm_sec,
			(nxlink_fd >= 0) ? "on" : "off");
}

void NX_LogShutdown (void)
{
	if (log_file)
	{
		fclose(log_file);
		log_file = NULL;
	}
	if (nxlink_fd >= 0)
	{
		close(nxlink_fd);
		nxlink_fd = -1;
	}
}

void NX_LogWrite (const char *text)
{
	if (log_file)
		fputs(text, log_file);
	if (nxlink_fd >= 0)
		fputs(text, stdout);
}

void NX_LogPrintf (const char *fmt, ...)
{
	char	buf[1024];
	va_list	argptr;

	va_start(argptr, fmt);
	vsnprintf(buf, sizeof(buf), fmt, argptr);
	va_end(argptr);
	NX_LogWrite(buf);
}

void NX_LogFlush (void)
{
	if (log_file)
		fflush(log_file);
}
