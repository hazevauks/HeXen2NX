/* in_nx.c -- controller input: Joy-Con, Pro Controller and handheld mode.
 *
 * Buttons become key events, and so go through the engine's bindings. While a
 * menu or the console has the keys, they mean what the keyboard's keys mean
 * there. The sticks are read directly, for they are analog in the game.
 *
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

#include <switch.h>

#define STICK_MAX		32767.0f
#define PITCH_MAX		80.0f
#define PITCH_MIN		-70.0f

#define MENU_STICK_THRESHOLD	0.5f
#define MENU_REPEAT_FIRST	0.40
#define MENU_REPEAT_NEXT	0.12

static cvar_t	joy_deadzone = {"joy_deadzone", "0.15", CVAR_ARCHIVE};
static cvar_t	joy_exponent = {"joy_exponent", "1.8", CVAR_ARCHIVE};	/* look: slower near the center */
static cvar_t	joy_yawspeed = {"joy_yawspeed", "220", CVAR_ARCHIVE};	/* degrees per second */
static cvar_t	joy_pitchspeed = {"joy_pitchspeed", "150", CVAR_ARCHIVE};
static cvar_t	joy_invertpitch = {"joy_invertpitch", "0", CVAR_ARCHIVE};

typedef struct
{
	u64	mask;
	int	key;		/* in the game, and when binding a button */
	int	menu_key;	/* in the menus and the console, if anything */
	int	sent;		/* what a press sent, for its release to match */
} nx_button_t;

static nx_button_t buttons[] =
{
	{ HidNpadButton_Plus,		K_ESCAPE,	K_ESCAPE,	0 },
	{ HidNpadButton_A,		K_NX_A,		K_ENTER,	0 },
	{ HidNpadButton_B,		K_NX_B,		K_ESCAPE,	0 },
	{ HidNpadButton_X,		K_NX_X,		K_BACKSPACE,	0 },
	{ HidNpadButton_Y,		K_NX_Y,		0,		0 },
	{ HidNpadButton_L,		K_NX_L,		0,		0 },
	{ HidNpadButton_R,		K_NX_R,		0,		0 },
	{ HidNpadButton_ZL,		K_NX_ZL,	K_PGUP,		0 },
	{ HidNpadButton_ZR,		K_NX_ZR,	K_PGDN,		0 },
	{ HidNpadButton_Minus,		K_NX_MINUS,	0,		0 },
	{ HidNpadButton_StickL,		K_NX_L3,	0,		0 },
	{ HidNpadButton_StickR,		K_NX_R3,	0,		0 },
	{ HidNpadButton_Up,		K_NX_DUP,	K_UPARROW,	0 },
	{ HidNpadButton_Down,		K_NX_DDOWN,	K_DOWNARROW,	0 },
	{ HidNpadButton_Left,		K_NX_DLEFT,	K_LEFTARROW,	0 },
	{ HidNpadButton_Right,		K_NX_DRIGHT,	K_RIGHTARROW,	0 }
};

static PadState	pad;
static u64	buttons_held;

static int	menu_stick_key;
static double	menu_stick_next;


static void Force_CenterView_f (void)
{
	cl.viewangles[PITCH] = 0;
}

void IN_ShowMouse (void)
{
}

void IN_HideMouse (void)
{
}

void IN_ActivateMouse (void)
{
}

void IN_DeactivateMouse (void)
{
}

void IN_Commands (void)
{
}

void IN_ClearStates (void)
{
}

void IN_Init (void)
{
	Cvar_RegisterVariable (&joy_deadzone);
	Cvar_RegisterVariable (&joy_exponent);
	Cvar_RegisterVariable (&joy_yawspeed);
	Cvar_RegisterVariable (&joy_pitchspeed);
	Cvar_RegisterVariable (&joy_invertpitch);

	Cmd_AddCommand ("force_centerview", Force_CenterView_f);

	padConfigureInput(1, HidNpadStyleSet_NpadStandard);
	padInitializeDefault(&pad);
}

void IN_Shutdown (void)
{
}

void IN_ReInit (void)
{
}

/* Removes the dead zone and rescales the rest to the range of -1 to 1. */
static float StickAxis (s32 raw)
{
	const float	deadzone = joy_deadzone.value;
	float		value = (float) raw / STICK_MAX;
	float		sign = (value < 0) ? -1.0f : 1.0f;

	value = fabsf(value);
	if (value <= deadzone)
		return 0;
	value = (value - deadzone) / (1.0f - deadzone);
	if (value > 1.0f)
		value = 1.0f;
	return sign * value;
}

static float LookCurve (float value)
{
	const float	sign = (value < 0) ? -1.0f : 1.0f;

	return sign * powf(fabsf(value), joy_exponent.value);
}

void IN_Move (usercmd_t *cmd)
{
	HidAnalogStickState	lstick, rstick;
	float			speed, move, yaw, pitch;

	/* cl.v.cameramode: the view belongs to something else, like a puzzle's camera. */
	if (Key_GetDest() != key_game || cl.v.cameramode)
		return;

	lstick = padGetStickPos(&pad, 0);
	rstick = padGetStickPos(&pad, 1);

	/* The same run and haste rules as for the keyboard, in CL_BaseMove. */
	speed = 200.0f;
	if (((cl_forwardspeed.value > 200) ^ (in_speed.state & 1)) && (cl.v.hasted <= 1))
		speed *= cl_movespeedkey.value;
	if (cl.v.hasted)
		speed *= cl.v.hasted;

	move = StickAxis(lstick.y);
	cmd->forwardmove += move * speed;
	move = StickAxis(lstick.x);
	cmd->sidemove += move * speed * (225.0f / 200.0f);

	yaw = LookCurve(StickAxis(rstick.x));
	pitch = LookCurve(StickAxis(rstick.y));
	if (joy_invertpitch.integer)
		pitch = -pitch;

	cl.viewangles[YAW] -= yaw * joy_yawspeed.value * host_frametime;

	if (pitch != 0)
	{
		cl.viewangles[PITCH] -= pitch * joy_pitchspeed.value * host_frametime;
		V_StopPitchDrift ();
	}
	if (cl.viewangles[PITCH] > PITCH_MAX)
		cl.viewangles[PITCH] = PITCH_MAX;
	if (cl.viewangles[PITCH] < PITCH_MIN)
		cl.viewangles[PITCH] = PITCH_MIN;
}

/* The left stick steps through the menus like the D-pad, with key repeat. */
static void MenuStickEvents (qboolean in_menu)
{
	HidAnalogStickState	stick;
	const double		now = Sys_DoubleTime();
	float			x, y;
	int			key = 0;

	if (in_menu)
	{
		stick = padGetStickPos(&pad, 0);
		x = (float) stick.x / STICK_MAX;
		y = (float) stick.y / STICK_MAX;

		if (y > MENU_STICK_THRESHOLD)
			key = K_UPARROW;
		else if (y < -MENU_STICK_THRESHOLD)
			key = K_DOWNARROW;
		else if (x < -MENU_STICK_THRESHOLD)
			key = K_LEFTARROW;
		else if (x > MENU_STICK_THRESHOLD)
			key = K_RIGHTARROW;
	}

	if (key != menu_stick_key)
	{
		if (menu_stick_key)
			Key_Event (menu_stick_key, false);
		if (key)
			Key_Event (key, true);
		menu_stick_key = key;
		menu_stick_next = now + MENU_REPEAT_FIRST;
	}
	else if (key && now >= menu_stick_next)
	{
		Key_Event (key, true);
		menu_stick_next = now + MENU_REPEAT_NEXT;
	}
}

void IN_SendKeyEvents (void)
{
	const keydest_t	dest = Key_GetDest();
	const qboolean	raw = (dest == key_game || dest == key_menubind);
	size_t		i;
	u64		held, changed;

	padUpdate(&pad);

	held = padGetButtons(&pad);
	changed = held ^ buttons_held;
	buttons_held = held;

	for (i = 0; i < Q_COUNTOF(buttons); i++)
	{
		nx_button_t	*b = &buttons[i];

		if (!(changed & b->mask))
			continue;

		if (held & b->mask)
		{
			b->sent = raw ? b->key : b->menu_key;
			if (b->sent)
				Key_Event (b->sent, true);
		}
		else if (b->sent)
		{
			Key_Event (b->sent, false);
			b->sent = 0;
		}
	}

	MenuStickEvents (!raw);
}
