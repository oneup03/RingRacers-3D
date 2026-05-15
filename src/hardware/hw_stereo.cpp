// DR. ROBOTNIK'S RING RACERS
//-----------------------------------------------------------------------------
// Copyright (C) 2025 by OneUp
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  hw_stereo.cpp
/// \brief Stereoscopic 3D scaffold: CVARs, mode substitutions, eye accessors.

#include "hw_stereo.h"

#ifdef HWRENDER

#include <math.h>

#include "../console.h"
#include "../doomstat.h" // r_splitscreen
#include "../i_video.h"  // rendermode, render_opengl
#include "../m_fixed.h"  // FIXED_TO_FLOAT
#include "../r_main.h"   // cv_fov, R_FOV
#include "../screen.h"   // cv_renderer

// CVARs are defined alongside the other OpenGL cvars in cvars.cpp (so they
// auto-register via CV_RegisterList(cvlist_opengl) from HWR_AddCommands). We
// only own the table of possible values + the helpers below.

extern "C"
{

CV_PossibleValue_t stereo_cons_t[] = {
	{STEREO_OFF,                "Off"},
	{STEREO_SBS,                "Side-by-Side"},
	{STEREO_TAB,                "Top-and-Bottom"},
	{STEREO_ROW_INTERLACED,     "Row-Interlaced"},
	{STEREO_COLUMN_INTERLACED,  "Column-Interlaced"},
	{STEREO_CHECKERBOARD,       "Checkerboard"},
	{STEREO_ANAGLYPH,           "Anaglyph"},
	{STEREO_LEIASR,             "LeiaSR"},
	{0, NULL}
};

INT32 R_StereoMode(void)
{
	// Stereo is only meaningful on the OpenGL renderer. The cvar can hold any
	// value through software/dedicated; render-side code sees Off until OpenGL
	// is active.
	if (rendermode != render_opengl)
		return STEREO_OFF;

	INT32 mode = cv_stereomode.value;

	// Note: LeiaSR + splitscreen also works — the SbS+splitscreen quadrant
	// layout (P0L top-left, P1L bottom-left, P0R top-right, P1R bottom-right)
	// is just one wide SbS image to the weaver. It still resolves "left of
	// center = L eye, right of center = R eye", so both players see autostereo
	// depth on supported hardware. No need to fall back.

	// Shader-composite modes (anaglyph, row/column-interlaced, checkerboard)
	// render an internal SBS or TAB image; the per-mode composite shader runs
	// at present time. See D5 in the plan.
	switch (mode)
	{
		case STEREO_ANAGLYPH:           return STEREO_SBS;
		case STEREO_ROW_INTERLACED:     return STEREO_TAB;
		case STEREO_COLUMN_INTERLACED:  return STEREO_SBS;
		case STEREO_CHECKERBOARD:       return STEREO_SBS;
		default:                        return mode;
	}
}

boolean R_StereoActive(void)
{
	return R_StereoMode() != STEREO_OFF;
}

INT32 R_StereoNumEyes(void)
{
	return R_StereoActive() ? 2 : 1;
}

// Eye state. Reset to MONO outside the eye loop; R_BeginStereoEye/R_EndStereoEye
// own all transitions. See skill pitfall 6 — placement and perspective must
// stay independent so Swap Eyes flips depth instead of cancelling itself out.
static INT8 s_current_placement_eye = STEREO_EYE_MONO;
static INT8 s_current_eye           = STEREO_EYE_MONO;

void R_BeginStereoEye(INT32 pass_idx)
{
	// pass_idx 0 = left physical region, 1 = right.
	s_current_placement_eye = (pass_idx == 0) ? STEREO_EYE_LEFT : STEREO_EYE_RIGHT;
	s_current_eye = s_current_placement_eye;
	if (cv_stereoswap.value)
		s_current_eye = (INT8)(-s_current_eye);
}

void R_EndStereoEye(void)
{
	s_current_placement_eye = STEREO_EYE_MONO;
	s_current_eye = STEREO_EYE_MONO;
}

INT8 R_GetCurrentPlacementEye(void)
{
	return s_current_placement_eye;
}

INT8 R_GetCurrentEye(void)
{
	return s_current_eye;
}

FLOAT R_GetStereoIOD(void)
{
	// Slider is integer * 10; recover world units. Sign convention:
	// GLPerspectiveStereo post-multiplies the projection with
	// pglTranslatef(-iod*0.5, 0, 0), so the asymmetric frustum sees input
	// vertices shifted by -iod*0.5 in X. For the LEFT eye (camera at world
	// x = -ipd/2), the frustum should see world coords shifted by +ipd/2 →
	// -iod*0.5 = +ipd/2 → iod = -ipd. Hence iod = current_eye * ipd
	// (current_eye = -1 for LEFT, +1 for RIGHT).
	const FLOAT ipd = (FLOAT)cv_stereoipd.value * 0.1f;
	return ((FLOAT)s_current_eye) * ipd;
}

FLOAT R_GetStereoFocal(void)
{
	return (FLOAT)cv_stereofocallength.value;
}

FLOAT R_GetStereoHUDShiftPixelsForEyePass(INT32 pass_idx)
{
	if (!R_StereoActive())
		return 0.0f;

	// Placement eye = which physical region we're drawing in (pass 0 = left).
	// Perspective eye = swap-honoring sign for parallax. We want the HUD shift
	// to follow the perspective eye, so Swap Eyes actually flips depth.
	const INT8 placement = (pass_idx == 0) ? STEREO_EYE_LEFT : STEREO_EYE_RIGHT;
	INT8 perspective = placement;
	if (cv_stereoswap.value)
		perspective = (INT8)(-perspective);

	const FLOAT ipd = (FLOAT)cv_stereoipd.value * 0.1f;
	const FLOAT focal = (FLOAT)cv_stereofocallength.value;
	const FLOAT depth_frac = (FLOAT)cv_stereohuddepth.value * 0.01f;

	// FOV is per-player; Phase 1 only ships single-player so cv_fov[0] is fine.
	const FLOAT fov_deg = FIXED_TO_FLOAT(R_FOV(0));
	const FLOAT half_fov = (FLOAT)(fov_deg * 0.5 * M_PIl / 180.0);
	const FLOAT tan_half_fov = (FLOAT)tan((double)half_fov);
	if (tan_half_fov <= 0.0f || focal <= 0.0f)
		return 0.0f;

	// disparity_px is the half-IPD spread between the two eyes in pixels at
	// the convergence plane. shift_px = -eye * depth_frac * disparity_px:
	// depth_frac=0 puts HUD at the screen plane (no shift); positive recedes,
	// negative pops out.
	const FLOAT disparity_px = ipd * (FLOAT)vid.width / (4.0f * focal * tan_half_fov);
	const FLOAT shift_px = -((FLOAT)perspective) * depth_frac * disparity_px;
	return shift_px;
}

stereorect_t R_StereoComputePlayerEyeRect(INT32 mode, INT32 eye, INT32 player_idx)
{
	// Layout rule: stereo is OUTER, splitscreen is INNER. The eye half of the
	// full screen is computed first, then subdivided per splitscreen player.
	// For TaB this gives the plan's P0L/P1L/P0R/P1R stripe layout (skill step
	// 6 + pitfall 5 — both players' L views in the top half so a row-parity
	// composite groups L content for both players). For SbS it gives 2x2
	// quadrants (left col = L eye, right col = R eye, rows = players) and
	// scales naturally to 4-player as a 2x2 sub-grid within each eye half.
	stereorect_t r;
	const INT32 W = vid.width;
	const INT32 H = vid.height;

	// Eye half of the full screen.
	INT32 ex, ey, ew, eh;
	switch (mode)
	{
		case STEREO_SBS:
		case STEREO_LEIASR:
			ew = W / 2;
			eh = H;
			ex = (eye == 0) ? 0 : (W / 2);
			ey = 0;
			break;
		case STEREO_TAB:
			ew = W;
			eh = H / 2;
			ex = 0;
			ey = (eye == 0) ? 0 : (H / 2);
			break;
		default:
			ex = 0; ey = 0; ew = W; eh = H;
			break;
	}

	const INT32 splits = r_splitscreen;
	if (player_idx < 0 || splits == 0)
	{
		// Single-player layout (or top-of-loop overlay rect).
		r.x = ex; r.y = ey; r.w = ew; r.h = eh;
	}
	else if (splits == 1)
	{
		// 2-player: subdivide the eye half horizontally (top/bottom stripes)
		// — mirrors mono 2-player splitscreen. Combined with SbS-outer this
		// produces 2x2 quadrants; combined with TaB-outer it produces the
		// P0L/P1L/P0R/P1R 4-stripe layout.
		r.x = ex;
		r.w = ew;
		r.h = eh / 2;
		r.y = ey + ((player_idx == 0) ? 0 : (eh / 2));
	}
	else
	{
		// 3 or 4 player: 2x2 quadrants within the eye half. P0 top-left,
		// P1 top-right, P2 bottom-left, P3 bottom-right — matching the mono
		// HWR_ShiftViewPort layout. For 3-player (splits=2) the P3 region
		// is just left cleared, same as mono.
		r.w = ew / 2;
		r.h = eh / 2;
		r.x = ex + ((player_idx & 1) ? (ew / 2) : 0);
		r.y = ey + ((player_idx >= 2) ? (eh / 2) : 0);
	}

	return r;
}

void StereoMode_OnChange(void)
{
	// During config load, cv_stereomode is applied before cv_renderer. Check
	// both the live rendermode AND the pending cv_renderer value so the
	// warning doesn't fire on every startup.
	if (rendermode != render_opengl && cv_renderer.value != render_opengl)
	{
		CONS_Alert(CONS_WARNING,
			"Stereoscopic 3D requires the OpenGL renderer; "
			"this setting takes effect once OpenGL is selected.\n");
	}
}

} // extern "C"

#endif // HWRENDER
