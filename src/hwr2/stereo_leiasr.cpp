// DR. ROBOTNIK'S RING RACERS
//-----------------------------------------------------------------------------
// Copyright (C) 2025 by OneUp
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  stereo_leiasr.cpp
/// \brief LeiaSR weaver bridge. The C entry points have a no-op fallback so
///        files that include the header still link when HAVE_LEIASR is off;
///        the SR-dependent code lives behind the #ifdef.

#include "stereo_leiasr.hpp"

#ifdef HAVE_LEIASR

#include <exception>
#include <mutex>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// The SR headers pull in <windows.h>. WIN32_LEAN_AND_MEAN is already defined
// by the SR header guards. Include order matters — SR types before our own.
#include <sr/management/srcontext.h>
#include <sr/weaver/glweaver.h>

#include "../console.h" // CONS_Alert

namespace
{

// Preflight check: probe whether the SR runtime DLLs are installed on this
// machine *before* calling any SR function. The SR libs are delay-loaded
// (see CMake) — that lets the exe start without SR present, but the first
// call into a missing delay-loaded DLL would raise an SEH exception, which
// can't be caught with C++ try/catch. By LoadLibrary-checking up front, we
// only proceed into the SR API when we know the DLLs resolve cleanly.
bool sr_runtime_available()
{
	// One probe is enough — the SR DLLs ship and install as a set. If
	// DimencoWeaving is present, the rest are too.
	HMODULE h = LoadLibraryW(L"DimencoWeaving.dll");
	if (!h)
		return false;
	FreeLibrary(h);
	return true;
}

SR::SRContext*    g_sr_context = nullptr;
SR::IGLWeaver1*   g_sr_weaver  = nullptr;
bool              g_init_attempted = false;
bool              g_init_succeeded = false;
std::mutex        g_init_mutex;

// Cached input texture state. Setting setInputViewTexture every frame works
// but reuploads weaver-side state; only push when something actually changed.
uint32_t          g_last_tex_id = 0;
int               g_last_w      = 0;
int               g_last_h      = 0;
uint32_t          g_last_fmt    = 0;

void destroy_weaver_locked()
{
	if (g_sr_weaver)
	{
		try { g_sr_weaver->destroy(); }
		catch (...) {} // weaver is a Leia object — swallow on teardown
		g_sr_weaver = nullptr;
	}
	if (g_sr_context)
	{
		try { SR::SRContext::deleteSRContext(g_sr_context); }
		catch (...) {}
		g_sr_context = nullptr;
	}
	g_last_tex_id = 0;
	g_last_w = g_last_h = 0;
	g_last_fmt = 0;
}

} // namespace

extern "C" boolean R_LeiaSR_Init(void *hwnd)
{
	std::lock_guard<std::mutex> lock(g_init_mutex);
	if (g_init_succeeded)
		return true;
	if (g_init_attempted)
		return false; // already tried and failed this session
	g_init_attempted = true;

	// Preflight: if the SR runtime DLLs aren't installed on this machine,
	// don't even touch the SR API — would raise an SEH on delay-load.
	if (!sr_runtime_available())
	{
		CONS_Alert(CONS_NOTICE,
			"LeiaSR runtime not installed on this machine. Falling back to Side-by-Side.\n");
		return false;
	}

	// SR runtime / hardware may be missing — anything thrown by the SR API
	// downgrades us to a clean false return so the caller can fall back to
	// SbS without crashing.
	try
	{
		g_sr_context = SR::SRContext::create();
		if (!g_sr_context)
			throw std::runtime_error("SRContext::create returned null");

		HWND hwnd_native = reinterpret_cast<HWND>(hwnd);
		WeaverErrorCode rc = SR::CreateGLWeaver(*g_sr_context, hwnd_native, &g_sr_weaver);
		if (rc != WeaverErrorCode::WeaverSuccess || g_sr_weaver == nullptr)
			throw std::runtime_error("SR::CreateGLWeaver failed");

		// Per the OpenGL weaving example (opengl_weaving/main.cpp line 703),
		// the SR context must be initialize()d *after* the weaver is created
		// and configured. Without this, the weaver hands back to the runtime
		// but the runtime never finishes wiring it up — weave() silently
		// produces an un-woven (raw SbS) image. setInputViewTexture itself
		// can be called either before or after initialize(); we defer it to
		// the first weave call where we actually know the texture id.
		g_sr_context->initialize();
	}
	catch (const std::exception& e)
	{
		CONS_Alert(CONS_WARNING, "LeiaSR init failed: %s\nFalling back to Side-by-Side.\n", e.what());
		destroy_weaver_locked();
		return false;
	}
	catch (...)
	{
		CONS_Alert(CONS_WARNING, "LeiaSR init failed (unknown exception). Falling back to Side-by-Side.\n");
		destroy_weaver_locked();
		return false;
	}

	g_init_succeeded = true;
	return true;
}

extern "C" boolean R_LeiaSR_Available(void)
{
	return g_init_succeeded && g_sr_weaver != nullptr;
}

extern "C" void R_LeiaSR_Shutdown(void)
{
	std::lock_guard<std::mutex> lock(g_init_mutex);
	destroy_weaver_locked();
	g_init_attempted = false;
	g_init_succeeded = false;
}

extern "C" void R_LeiaSR_Weave(uint32_t tex_id, int width, int height, uint32_t format)
{
	if (!g_init_succeeded || g_sr_weaver == nullptr)
		return;

	try
	{
		if (tex_id != g_last_tex_id || width != g_last_w || height != g_last_h || format != g_last_fmt)
		{
			g_sr_weaver->setInputViewTexture(static_cast<GLuint>(tex_id), width, height, static_cast<GLenum>(format));
			g_last_tex_id = tex_id;
			g_last_w = width;
			g_last_h = height;
			g_last_fmt = format;
		}
		g_sr_weaver->weave();
	}
	catch (const std::exception& e)
	{
		CONS_Alert(CONS_WARNING, "LeiaSR weave failed: %s\nDisabling for this session.\n", e.what());
		std::lock_guard<std::mutex> lock(g_init_mutex);
		destroy_weaver_locked();
		g_init_succeeded = false;
	}
	catch (...)
	{
		CONS_Alert(CONS_WARNING, "LeiaSR weave failed (unknown exception). Disabling for this session.\n");
		std::lock_guard<std::mutex> lock(g_init_mutex);
		destroy_weaver_locked();
		g_init_succeeded = false;
	}
}

#else // !HAVE_LEIASR

// Fallback no-op implementations so callers compile cleanly when SR support
// isn't built in. R_LeiaSR_Available() returning false flows naturally into
// the SbS fallback path.

extern "C" boolean R_LeiaSR_Init(void *hwnd)            { (void)hwnd; return false; }
extern "C" boolean R_LeiaSR_Available(void)             { return false; }
extern "C" void    R_LeiaSR_Shutdown(void)              {}
extern "C" void    R_LeiaSR_Weave(uint32_t, int, int, uint32_t) {}

#endif // HAVE_LEIASR
