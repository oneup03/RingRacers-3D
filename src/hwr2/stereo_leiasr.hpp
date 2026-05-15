// DR. ROBOTNIK'S RING RACERS
//-----------------------------------------------------------------------------
// Copyright (C) 2025 by OneUp
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  stereo_leiasr.hpp
/// \brief LeiaSR autostereoscopic weaver bridge. Wraps the Simulated Reality
///        SDK (`thirdparty/LeiaSR64`) so the rest of the engine can hand
///        a side-by-side GL texture + window handle to the SR weaver and
///        get a glasses-free 3D image out the other side.
///
///        Whole module is gated by HAVE_LEIASR + WIN32 in CMake. When the SR
///        runtime is missing on the target machine, R_LeiaSR_Init returns
///        false and the caller falls back to plain SbS.

#ifndef __SRB2_HWR2_STEREO_LEIASR_HPP__
#define __SRB2_HWR2_STEREO_LEIASR_HPP__

#include <cstdint>

#include "../doomtype.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize the SR context + GL weaver. Lazy / idempotent: subsequent calls
/// after the first successful init are no-ops. Returns true on success, false
/// if the SR runtime is unavailable or no Leia/SR display is connected.
boolean R_LeiaSR_Init(void *hwnd);

/// True after a successful R_LeiaSR_Init. Used by I_FinishUpdate to decide
/// whether to dispatch to the weaver or fall back to SbS pass-through.
boolean R_LeiaSR_Available(void);

/// Tear down the weaver + SR context. Safe to call when uninitialized.
void R_LeiaSR_Shutdown(void);

/// Run the weave for one frame. Caller has just bound the SDL default
/// framebuffer and set the viewport — the weaver writes into the current
/// render target.
///
///   tex_id     raw GL texture name of the SbS source (from
///              Rhi::get_native_texture). The source must be the full window
///              size — the SR weaver samples exactly (width * height) pixels;
///              lying about dimensions doesn't work.
///   width,height texture dimensions in pixels (must match the texture).
///   format     OpenGL pixel format token of the source texture (GL_RGB /
///              GL_RGBA / etc.). Pass 0x1907 = GL_RGB for the standard
///              backbuffer.
void R_LeiaSR_Weave(uint32_t tex_id, int width, int height, uint32_t format);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __SRB2_HWR2_STEREO_LEIASR_HPP__
