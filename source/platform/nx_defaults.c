/* nx_defaults.c -- the console's default settings and button bindings.
 *
 * They run right after the game's default.cfg and ahead of config.cfg, so what
 * the player changes in the menus stays changed. The README lists the buttons.
 *
 * Copyright (C) 2026  HeXen2NX contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */

#include "quakedef.h"

static const char nx_default_config[] =
	/* Movement: ZL, A and B stay in reach while the right stick looks around. */
	"bind BUTTON_ZR \"+attack\"\n"
	"bind BUTTON_ZL \"+jump\"\n"
	"bind BUTTON_A \"+jump\"\n"
	"bind BUTTON_B \"+crouch\"\n"
	/* Inventory: browse with the shoulders, use what is selected. */
	"bind BUTTON_L \"invleft\"\n"
	"bind BUTTON_R \"invright\"\n"
	"bind BUTTON_X \"invuse\"\n"
	"bind BUTTON_DDOWN \"impulse 44\"\n"
	/* Weapons. */
	"bind BUTTON_Y \"impulse 10\"\n"
	"bind BUTTON_DRIGHT \"impulse 10\"\n"
	"bind BUTTON_DLEFT \"impulse 12\"\n"
	/* The rest. */
	"bind BUTTON_MINUS \"+showinfo\"\n"
	"bind BUTTON_DUP \"+showdm\"\n"
	"bind BUTTON_L3 \"+speed\"\n"
	"bind BUTTON_R3 \"centerview\"\n"
	/* Settings. An analog stick has its own walking speed: running is the default. */
	"cl_forwardspeed 400\n"
	"cl_backspeed 400\n"
	"bgmtype midi\n"
	"bgm_extmusic 1\n"
	"bgmvolume 0.7\n";

const char *NX_GetDefaultConfig (void)
{
	return nx_default_config;
}
