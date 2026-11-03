/* nx_data.c -- puts the player's game files in the layout the engine expects.
 *
 * A copy of the game (GOG's, for one) comes with mixed-case file names and
 * keeps the CD music in a folder of its own, named by track. The SD card's
 * file system does not care for case, but an emulator's host may, so names
 * are made lowercase once, and the music moves to where the engine looks for
 * it, under the name the game asks for (see docs/README.music in uHexen2).
 *
 * Copyright (C) 2026  HeXen2NX contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "nx_data.h"
#include "nx_log.h"

#define PATH_SIZE	512
#define NAME_SIZE	64
#define MAX_RENAMES	64

typedef struct
{
	const char	*gamedir;
	const char	*midi_name[32];	/* index: CD track */
} music_table_t;

static const music_table_t music_tables[] =
{
	{ "data1", {
		[2] = "casa1", [3] = "casa2", [4] = "casa3", [5] = "casa4",
		[6] = "egyp1", [7] = "egyp2", [8] = "egyp3",
		[9] = "meso1", [10] = "meso2", [11] = "meso3",
		[12] = "roma1", [13] = "roma2", [14] = "roma3",
		[15] = "casb1", [16] = "casb2", [17] = "casb3" } },
	{ "portals", {
		[2] = "tulku7", [3] = "tulku1", [4] = "tulku4", [5] = "tulku2",
		[6] = "tulku9", [7] = "tulku10", [8] = "tulku6", [9] = "tulku5",
		[10] = "tulku8", [11] = "tulku3" } }
};

static bool IsDirectory (const char *path)
{
	struct stat	st;
	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* Lowercases the names of the files in a directory, not of what is below it. */
static void LowercaseNames (const char *dirpath)
{
	/* Renaming as the directory is read may skip entries: names first. */
	static char	names[MAX_RENAMES][NAME_SIZE];
	char		from[PATH_SIZE], to[PATH_SIZE];
	DIR		*dir = opendir(dirpath);
	struct dirent	*ent;
	int		count = 0, i;

	if (!dir)
		return;

	while (count < MAX_RENAMES && (ent = readdir(dir)) != NULL)
	{
		const char	*c;

		for (c = ent->d_name; *c; c++)
		{
			if (isupper((unsigned char) *c))
				break;
		}
		if (*c && strlen(ent->d_name) < NAME_SIZE)
			strcpy(names[count++], ent->d_name);
	}

	closedir(dir);

	for (i = 0; i < count; i++)
	{
		char	*c;

		snprintf(from, sizeof(from), "%s/%s", dirpath, names[i]);
		for (c = names[i]; *c; c++)
			*c = (char) tolower((unsigned char) *c);
		snprintf(to, sizeof(to), "%s/%s", dirpath, names[i]);
		/* A case-insensitive file system may refuse; nothing is lost then. */
		rename(from, to);
	}
}

/* Moves trackNN.ogg from srcdir to <gamedir>/music/<midi name>.ogg. */
static int MoveMusic (const char *basedir, const char *srcdir, const music_table_t *table)
{
	char	from[PATH_SIZE], to[PATH_SIZE];
	int	track, moved = 0;

	snprintf(to, sizeof(to), "%s/%s/music", basedir, table->gamedir);
	mkdir(to, 0777);

	for (track = 0; track < 32; track++)
	{
		if (!table->midi_name[track])
			continue;

		snprintf(from, sizeof(from), "%s/track%02d.ogg", srcdir, track);
		snprintf(to, sizeof(to), "%s/%s/music/%s.ogg", basedir, table->gamedir, table->midi_name[track]);
		if (rename(from, to) == 0)
			moved++;
	}
	return moved;
}

void NX_PrepareGameData (const char *basedir)
{
	char	path[PATH_SIZE];
	size_t	i;

	snprintf(path, sizeof(path), "%s/music", basedir);
	LowercaseNames(path);

	for (i = 0; i < sizeof(music_tables) / sizeof(music_tables[0]); i++)
	{
		const music_table_t	*table = &music_tables[i];
		int			moved = 0;

		snprintf(path, sizeof(path), "%s/%s", basedir, table->gamedir);
		if (!IsDirectory(path))
			continue;

		LowercaseNames(path);

		/* A PC configuration binds nothing we have and turns the music off. */
		{
			char	cfg[PATH_SIZE], pc_cfg[PATH_SIZE];

			snprintf(cfg, sizeof(cfg), "%s/config.cfg", path);
			snprintf(pc_cfg, sizeof(pc_cfg), "%s/config_pc.cfg", path);
			if (!NX_ConfigIsOurs(cfg))
				rename(cfg, pc_cfg);
		}

		/* The expansion's music is not in the base folder of any copy. */
		if (i == 0)
		{
			snprintf(path, sizeof(path), "%s/music", basedir);
			moved += MoveMusic(basedir, path, table);
		}
		snprintf(path, sizeof(path), "%s/%s/music", basedir, table->gamedir);
		LowercaseNames(path);
		moved += MoveMusic(basedir, path, table);

		if (moved)
			NX_LogPrintf("data: moved %d music tracks into %s/music\n", moved, table->gamedir);
	}
}

bool NX_ConfigIsOurs (const char *path)
{
	char	line[128];
	bool	ours = false;
	FILE	*f = fopen(path, "r");

	if (!f)
		return true;	/* nothing to move out of the way */

	/* Only what this port's bindings write has these. */
	while (!ours && fgets(line, sizeof(line), f))
		ours = (strstr(line, "BUTTON_") != NULL);

	fclose(f);
	return ours;
}
