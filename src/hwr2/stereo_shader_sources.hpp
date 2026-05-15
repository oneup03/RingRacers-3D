// DR. ROBOTNIK'S RING RACERS
//-----------------------------------------------------------------------------
// Copyright (C) 2025 by OneUp
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  stereo_shader_sources.hpp
/// \brief Built-in (exe-embedded) GLSL sources for the stereoscopic composite
///        passes. The RHI shader loader (SdlGl2Platform::find_shader_sources)
///        consults this registry before falling through to the pk3 glsllist
///        path — so the stereo shaders ship with the executable rather than
///        as separate WAD lumps.

#ifndef __SRB2_HWR2_STEREO_SHADER_SOURCES_HPP__
#define __SRB2_HWR2_STEREO_SHADER_SOURCES_HPP__

#include <string_view>
#include <tuple>

#include "../core/string.h"
#include "../core/vector.hpp"

namespace srb2::hwr2
{

/// Return embedded vertex + fragment source vectors for a built-in stereo
/// shader program, or std::nullopt if `name` isn't one we provide. The
/// returned vectors are the same shape `SdlGl2Platform::find_shader_sources`
/// returns — each entry a source string concatenated by the GL2 compiler in
/// order, no `#version` line (the RHI prepends it).
bool find_builtin_stereo_shader_sources(
	const char* name,
	srb2::Vector<srb2::String>& vertex_sources_out,
	srb2::Vector<srb2::String>& fragment_sources_out);

} // namespace srb2::hwr2

#endif // __SRB2_HWR2_STEREO_SHADER_SOURCES_HPP__
