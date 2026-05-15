// DR. ROBOTNIK'S RING RACERS
//-----------------------------------------------------------------------------
// Copyright (C) 2025 by OneUp
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  hw_stereo.h
/// \brief Stereoscopic 3D state, CVARs, and per-eye geometry helpers.
///
/// Phase 0: scaffold only — CVARs + R_StereoMode() substitutions exist, but
/// no rendering code consumes them yet.

#ifndef __HWR_STEREO_H__
#define __HWR_STEREO_H__

#include "../doomtype.h"
#include "../command.h"

#ifdef __cplusplus
extern "C" {
#endif

// Stereo display mode. Integer values match the menu display order. See
// R_StereoMode() for the internal render-format substitutions used by
// render-side code.
typedef enum
{
	STEREO_OFF               = 0,
	STEREO_SBS               = 1,
	STEREO_TAB               = 2,
	STEREO_ROW_INTERLACED    = 3,
	STEREO_COLUMN_INTERLACED = 4,
	STEREO_CHECKERBOARD      = 5,
	STEREO_ANAGLYPH          = 6, // Dubois matrix, shader-composited
	STEREO_LEIASR            = 7,
} stereomode_t;

typedef enum
{
	STEREO_EYE_LEFT  = -1,
	STEREO_EYE_MONO  =  0,
	STEREO_EYE_RIGHT = +1,
} stereoeye_t;

extern CV_PossibleValue_t stereo_cons_t[];

extern consvar_t cv_stereomode;
extern consvar_t cv_stereoipd;
extern consvar_t cv_stereofocallength;
extern consvar_t cv_stereoswap;
extern consvar_t cv_stereohuddepth;
// (cv_stereocrosshairdepth omitted — RingRacers has no game-rendered crosshair.
//  hud_crosshair is a Lua-side HUD hook for mods; AM_drawCrosshair is the
//  automap center marker. Neither benefits from per-eye stereo placement.)

/// Pixel rect describing where to render or composite. Y is **top-down**
/// (matching `gl_viewwindowy` / GClipRect callers in hw_main.cpp — GClipRect
/// itself does the `screen_height - maxy` inversion to bottom-up at the
/// `pglViewport` call). Returned by R_StereoComputePlayerEyeRect.
typedef struct
{
	INT32 x, y, w, h;
} stereorect_t;

/// Return the *internal* render format, applying substitutions so render-side
/// code only ever sees STEREO_SBS / STEREO_TAB / STEREO_LEIASR. Mapping:
///   Anaglyph          -> SBS  (Dubois matrix mixes both halves)
///   RowInterlaced     -> TAB  (top/bottom split feeds row-parity composite)
///   ColumnInterlaced  -> SBS  (column-parity composite)
///   Checkerboard      -> SBS  (same source; shader picks by (col+row) parity)
///   LeiaSR+splitscreen-> SBS  (weaver can't process a 2x2 grid)
/// Returns STEREO_OFF when stereo is disabled or the renderer is not OpenGL.
///
/// Only the *present-time composite dispatch* in I_FinishUpdate reads the raw
/// cv_stereomode.value — everywhere else should go through R_StereoMode().
INT32 R_StereoMode(void);

/// True iff R_StereoMode() != STEREO_OFF.
boolean R_StereoActive(void);

/// 2 when stereo is active, else 1. Drives the eye loop in D_Display.
INT32 R_StereoNumEyes(void);

/// Eye-loop bracket. pass_idx is 0 or 1; placement_eye is the raw physical
/// region (-1 left, +1 right) regardless of swap; current_eye is the
/// perspective eye and flips when cv_stereoswap is on.
///
/// Why two: placement decides viewport geometry (which physical region we
/// render into); perspective decides iod sign + HUD shift. Inverting just the
/// perspective is what makes Swap Eyes actually flip depth instead of being a
/// no-op (see skill pitfall 6 / step 10).
void R_BeginStereoEye(INT32 pass_idx);
void R_EndStereoEye(void);

/// Raw physical eye (-1 left, +1 right). Used by SetStereoMode-style geometry.
INT8 R_GetCurrentPlacementEye(void);

/// Perspective eye (-1 left, +1 right). Used for iod sign + HUD parallax.
INT8 R_GetCurrentEye(void);

/// Signed IOD in world units, sign = perspective eye. Left eye = +ipd,
/// right eye = -ipd. GLPerspectiveStereo translates by -iod*0.5 along X, so
/// +ipd produces a leftward camera shift (the left-eye view).
FLOAT R_GetStereoIOD(void);

/// Focal / convergence-plane distance in world units (unsigned).
FLOAT R_GetStereoFocal(void);

/// Eye-region rect in legacy-GL bottom-up Y, for SetStereoMode + the matching
/// composite target. mode is the *internal* render format (SBS / TAB / LEIASR
/// — i.e. R_StereoMode()). player_idx == -1 means "single-player layout"
/// (used by Phase 1 transient overlays and the post-loop HUD pass too); 0..n
/// is splitscreen-aware (Phase 4 onward). For Phase 1, callers always pass -1.
stereorect_t R_StereoComputePlayerEyeRect(INT32 mode, INT32 eye, INT32 player_idx);

/// HUD parallax in screen pixels for an eye pass (0 = first/left, 1 = right).
/// Honors cv_stereoswap, cv_stereoipd, cv_stereofocallength, cv_stereohuddepth.
/// Returns 0 when stereo is inactive. Used by TwodeeRenderer to NDC-shift the
/// HUD's full-screen ortho projection. (Same shift applies to crosshair-like
/// content drawn by Lua HUD scripts — no separate crosshair-depth cvar; see
/// the comment above for why.)
FLOAT R_GetStereoHUDShiftPixelsForEyePass(INT32 pass_idx);

/// onchange callback for cv_stereomode. Warns when the renderer is not OpenGL
/// (without snapping the value back — cv_stereomode is CV_SAVE and we want it
/// to round-trip through config load before cv_renderer is applied).
void StereoMode_OnChange(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __HWR_STEREO_H__
