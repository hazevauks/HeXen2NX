/* vid_nx.c -- video driver for the software renderer (the fallback renderer).
 *
 * The engine draws 8-bit pixels; they are expanded through the palette into a
 * libnx framebuffer here, once per frame. The system scales the window up to
 * the screen, handheld or docked. See docs/renderer.md for the deko3D path.
 *
 * Copyright (C) 1996-1997  Id Software, Inc.
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
#include "d_local.h"
#include "cfgfile.h"

#include <switch.h>

#define NX_SW_WIDTH	640
#define NX_SW_HEIGHT	360
#define NX_SW_BUFFERS	2

unsigned short	d_8to16table[256];
unsigned int	d_8to24table[256];

byte		globalcolormap[VID_GRADES*256], lastglobalcolor = 0;
byte		*lastsourcecolormap = NULL;

viddef_t	vid;
modestate_t	modestate = MS_UNINIT;
qboolean	in_mode_set;

cvar_t		_enable_mouse = {"_enable_mouse", "0", CVAR_NONE};

static cvar_t	vid_showload = {"vid_showload", "1", CVAR_NONE};

static Framebuffer	framebuffer;
static qboolean		vid_initialized;

static u32		palette_rgba[256];
static byte		*vid_pixels;
static byte		*vid_surfcache;
static int		vid_surfcachesize;
static int		VID_highhunkmark;


static void VID_AllocBuffers (int width, int height)
{
	int		tsize, tbuffersize;

	tbuffersize = width * height * sizeof (*d_pzbuffer);
	tsize = D_SurfaceCacheForRes (width, height);
	tbuffersize += tsize;

	if (host_parms->memsize < tbuffersize + 0x180000 + 0xC00000)
		Sys_Error ("Not enough memory for video mode");

	vid_surfcachesize = tsize;

	if (d_pzbuffer)
	{
		D_FlushCaches ();
		Hunk_FreeToHighMark (VID_highhunkmark);
		d_pzbuffer = NULL;
	}

	VID_highhunkmark = Hunk_HighMark ();

	d_pzbuffer = (short *) Hunk_HighAllocName (tbuffersize, "video");

	vid_surfcache = (byte *)d_pzbuffer + width * height * sizeof (*d_pzbuffer);
}

void VID_LockBuffer (void)
{
	if (r_dowarp)
		d_viewbuffer = r_warpbuffer;
	else
		d_viewbuffer = vid.buffer;

	if (r_dowarp)
		screenwidth = WARP_WIDTH;
	else
		screenwidth = vid.rowbytes;
}

void VID_UnlockBuffer (void)
{
}

void VID_SetPalette (const unsigned char *palette)
{
	int	i;

	for (i = 0; i < 256; i++, palette += 3)
		palette_rgba[i] = RGBA8_MAXALPHA(palette[0], palette[1], palette[2]);
}

void VID_ShiftPalette (const unsigned char *palette)
{
	VID_SetPalette (palette);
}

static void VID_MenuDraw (void)
{
	ScrollTitle("gfx/menu/title7.lmp");

	M_Print (76, 92, "Renderer  : ");
	M_PrintWhite (76+12*8, 92, "software");
	M_Print (76, 100, "Resolution: ");
	M_PrintWhite (76+12*8, 100, va("%d x %d", vid.width, vid.height));
}

static void VID_MenuKey (int key)
{
	if (key == K_ESCAPE)
		M_Menu_Options_f ();
}

void VID_Init (const unsigned char *palette)
{
	Result	rc;
	int	temp;

	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	Cvar_RegisterVariable (&_enable_mouse);
	Cvar_RegisterVariable (&vid_showload);

	rc = framebufferCreate(&framebuffer, nwindowGetDefault(), NX_SW_WIDTH, NX_SW_HEIGHT,
				PIXEL_FORMAT_RGBA_8888, NX_SW_BUFFERS);
	if (R_SUCCEEDED(rc))
		rc = framebufferMakeLinear(&framebuffer);
	if (R_FAILED(rc))
		Sys_Error ("Couldn't create the framebuffer: 0x%x", rc);

	vid_pixels = (byte *) Hunk_AllocName (NX_SW_WIDTH * NX_SW_HEIGHT, "vidpixels");

	vid.width = vid.conwidth = NX_SW_WIDTH;
	vid.height = vid.conheight = NX_SW_HEIGHT;
	vid.buffer = vid.conbuffer = vid.direct = vid_pixels;
	vid.rowbytes = vid.conrowbytes = NX_SW_WIDTH;
	vid.numpages = 1;
	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);
	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));
	vid.recalc_refdef = 1;
	modestate = MS_FULLDIB;

	VID_AllocBuffers (vid.width, vid.height);
	D_InitCaches (vid_surfcache, vid_surfcachesize);

	VID_SetPalette (palette);

	Con_SafePrintf ("Video Mode: %ux%ux8 (software)\n", vid.width, vid.height);

	scr_disabled_for_loading = temp;
	vid_initialized = true;

	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;
}

void VID_Shutdown (void)
{
	if (vid_initialized)
	{
		vid_initialized = false;
		framebufferClose(&framebuffer);
	}
}

void VID_Update (vrect_t *rects)
{
	const byte	*src = vid_pixels;
	u32		stride;
	u8		*dst;
	int		x, y;

	(void) rects;	/* the whole frame goes out, for the swap chain has two */

	dst = (u8 *) framebufferBegin(&framebuffer, &stride);

	for (y = 0; y < vid.height; y++)
	{
		u32	*row = (u32 *) dst;

		for (x = 0; x < vid.width; x++)
			row[x] = palette_rgba[src[x]];

		src += vid.rowbytes;
		dst += stride;
	}

	framebufferEnd(&framebuffer);
}

void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
	byte	*offset;

	if (!vid_initialized)
		return;
	if (x < 0)
		x = vid.width + x - 1;
	offset = vid_pixels + y * vid.rowbytes + x;
	while (height--)
	{
		memcpy (offset, pbitmap, width);
		offset += vid.rowbytes;
		pbitmap += width;
	}
}

void D_EndDirectRect (int x, int y, int width, int height)
{
	(void) x; (void) y; (void) width; (void) height;

	if (vid_initialized)
		VID_Update (NULL);
}

#ifndef H2W
void D_ShowLoadingSize (void)
{
#ifdef DRAW_PROGRESSBARS
	static int	prev_perc;
	int		cur_perc;
	viddef_t	save_vid;

	if (!vid_showload.integer || !vid_initialized)
		return;

	cur_perc = loading_stage * 100;
	if (total_loading_size)
		cur_perc += current_loading_size * 100 / total_loading_size;
	if (cur_perc == prev_perc)
		return;
	prev_perc = cur_perc;

	save_vid = vid;
	vid.buffer = vid.direct;

	SCR_DrawLoading();
	VID_Update (NULL);

	vid = save_vid;
#endif
}
#endif

void VID_HandlePause (qboolean paused)
{
	(void) paused;
}

void VID_ToggleFullscreen (void)
{
}
