// DR. ROBOTNIK'S RING RACERS
//-----------------------------------------------------------------------------
// Copyright (C) 2025 by OneUp
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  menus/options-video-stereo.c
/// \brief Stereoscopic 3D options submenu.

#include "../k_menu.h"
#include "../hardware/hw_stereo.h" // stereo cvars
#include "../i_video.h"            // rendermode

menuitem_t OPTIONS_VideoStereo[] =
{
	{IT_HEADER, "Display Mode...", NULL,
		NULL, {NULL}, 0, 0},

	{IT_STRING | IT_CVAR, "Mode", "Off, Side-by-Side, Top-and-Bottom, Anaglyph, Row/Column-Interlaced, Checkerboard, or LeiaSR (autostereoscopic).",
		NULL, {.cvar = &cv_stereomode}, 0, 0},

	{IT_STRING | IT_CVAR, "Swap Eyes", "Invert which eye sees which view. Use if 3D depth appears inverted.",
		NULL, {.cvar = &cv_stereoswap}, 0, 0},

	{IT_SPACE | IT_NOTHING, NULL, NULL,
		NULL, {NULL}, 0, 0},

	{IT_HEADER, "Geometry...", NULL,
		NULL, {NULL}, 0, 0},

	{IT_STRING | IT_CVAR | IT_CV_SLIDER, "Eye Separation", "Inter-pupillary distance in world units * 10. Higher = stronger 3D effect.",
		NULL, {.cvar = &cv_stereoipd}, 0, 0},

	{IT_STRING | IT_CVAR | IT_CV_SLIDER, "Focal Distance", "Distance to the zero-parallax plane, in world units. Objects nearer pop out; farther recede.",
		NULL, {.cvar = &cv_stereofocallength}, 0, 0},

	{IT_SPACE | IT_NOTHING, NULL, NULL,
		NULL, {NULL}, 0, 0},

	{IT_HEADER, "HUD Depth...", NULL,
		NULL, {NULL}, 0, 0},

	{IT_STRING | IT_CVAR | IT_CV_SLIDER, "HUD Depth", "HUD parallax * 100. 0 = screen plane, negative = pops out, positive = recedes.",
		NULL, {.cvar = &cv_stereohuddepth}, 0, 0},
};

menu_t OPTIONS_VideoStereoDef = {
	sizeof (OPTIONS_VideoStereo) / sizeof (menuitem_t),
	&OPTIONS_VideoDef,
	0,
	OPTIONS_VideoStereo,
	48, 80,
	SKINCOLOR_PLAGUE, 0,
	MBF_DRAWBGWHILEPLAYING,
	NULL,
	2, 5,
	M_DrawGenericOptions,
	M_DrawOptionsCogs,
	M_OptionsTick,
	NULL,
	NULL,
	NULL,
};
