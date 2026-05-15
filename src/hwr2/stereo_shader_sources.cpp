// DR. ROBOTNIK'S RING RACERS
//-----------------------------------------------------------------------------
// Copyright (C) 2025 by OneUp
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  stereo_shader_sources.cpp

#include "stereo_shader_sources.hpp"

#include <cstring>

namespace srb2::hwr2
{

// ---------------------------------------------------------------------------
//  stereocomposite
// ---------------------------------------------------------------------------
//
// Vertex: passthrough — same attribute layout as BlitRectPass (a_position
// vec3, a_texcoord0 vec2) and same uniforms (u_projection, u_modelview,
// u_texcoord0_transform). The RHI compiler prepends `#version 120` and any
// `#define` lines from ProgramDesc::defines, so we stick to GLSL 1.20.
//
// Fragment: branches on the int uniform `u_stereo_mode` to select the
// per-mode composite. The integer values match the stereomode_t enum in
// hw_stereo.h so the dispatch from I_FinishUpdate can pass cv_stereomode
// directly.
//
//   mode 3 = STEREO_ROW_INTERLACED     - source TaB, row parity
//   mode 4 = STEREO_COLUMN_INTERLACED  - source SbS, column parity
//   mode 5 = STEREO_CHECKERBOARD       - source SbS, (col+row) parity
//   mode 6 = STEREO_ANAGLYPH (Dubois)  - source SbS, 3x6 matrix
//
// Integer-modulo emulation via `n - (n / 2) * 2` because GLSL 1.20 `mod()` on
// ints isn't portable.
//
// gl_FragCoord.{x,y} are window-pixel coordinates (bottom-up Y in GL), so
// parity maps directly to the final output framebuffer's pixel grid — which
// is what makes the row/column/checkerboard patterns line up with the
// physical display lines/columns even when the internal render resolution
// doesn't match the window.

static const char* kStereoCompositeVertex = R"glsl(
attribute vec3 a_position;
attribute vec2 a_texcoord0;
uniform mat4 u_projection;
uniform mat4 u_modelview;
uniform mat3 u_texcoord0_transform;
varying vec2 v_texcoord0;

void main()
{
	v_texcoord0 = (u_texcoord0_transform * vec3(a_texcoord0, 1.0)).xy;
	gl_Position = u_projection * u_modelview * vec4(a_position, 1.0);
}
)glsl";

static const char* kStereoCompositeFragment = R"glsl(
varying vec2 v_texcoord0;
uniform sampler2D s_sampler0;
uniform int u_stereo_mode;

void main()
{
	vec2 uv = v_texcoord0;
	vec3 outColor;

	if (u_stereo_mode == 6) // Anaglyph (Dubois)
	{
		vec2 uvL = vec2(uv.x * 0.5,       uv.y);
		vec2 uvR = vec2(uv.x * 0.5 + 0.5, uv.y);
		vec3 cA = texture2D(s_sampler0, uvL).rgb;
		vec3 cB = texture2D(s_sampler0, uvR).rgb;
		float r = clamp( 0.437*cA.r + 0.449*cA.g + 0.164*cA.b
		                -0.011*cB.r - 0.032*cB.g - 0.007*cB.b, 0.0, 1.0);
		float g = clamp(-0.062*cA.r - 0.062*cA.g - 0.024*cA.b
		                +0.377*cB.r + 0.761*cB.g + 0.009*cB.b, 0.0, 1.0);
		float b = clamp(-0.048*cA.r - 0.050*cA.g - 0.017*cA.b
		                -0.026*cB.r - 0.093*cB.g + 1.234*cB.b, 0.0, 1.0);
		outColor = vec3(r, g, b);
	}
	else if (u_stereo_mode == 3) // Row-Interlaced - TaB source
	{
		int row = int(gl_FragCoord.y);
		vec2 uvOut = uv;
		if ((row - (row / 2) * 2) == 0)
			uvOut.y = uv.y * 0.5 + 0.5;
		else
			uvOut.y = uv.y * 0.5;
		outColor = texture2D(s_sampler0, uvOut).rgb;
	}
	else if (u_stereo_mode == 4) // Column-Interlaced - SbS source
	{
		int col = int(gl_FragCoord.x);
		vec2 uvOut = uv;
		if ((col - (col / 2) * 2) == 0)
			uvOut.x = uv.x * 0.5;
		else
			uvOut.x = uv.x * 0.5 + 0.5;
		outColor = texture2D(s_sampler0, uvOut).rgb;
	}
	else if (u_stereo_mode == 5) // Checkerboard - SbS source, (col+row) parity
	{
		int col = int(gl_FragCoord.x);
		int row = int(gl_FragCoord.y);
		int sum = col + row;
		vec2 uvOut = uv;
		if ((sum - (sum / 2) * 2) == 0)
			uvOut.x = uv.x * 0.5;
		else
			uvOut.x = uv.x * 0.5 + 0.5;
		outColor = texture2D(s_sampler0, uvOut).rgb;
	}
	else // Fallback passthrough (debug)
	{
		outColor = texture2D(s_sampler0, uv).rgb;
	}

	gl_FragColor = vec4(outColor, 1.0);
}
)glsl";

bool find_builtin_stereo_shader_sources(
	const char* name,
	srb2::Vector<srb2::String>& vertex_sources_out,
	srb2::Vector<srb2::String>& fragment_sources_out)
{
	if (name == nullptr)
		return false;

	if (std::strcmp(name, "stereocomposite") == 0)
	{
		vertex_sources_out.clear();
		fragment_sources_out.clear();
		vertex_sources_out.emplace_back(kStereoCompositeVertex);
		fragment_sources_out.emplace_back(kStereoCompositeFragment);
		return true;
	}

	return false;
}

} // namespace srb2::hwr2
