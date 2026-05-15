// DR. ROBOTNIK'S RING RACERS
//-----------------------------------------------------------------------------
// Copyright (C) 2025 by OneUp
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  stereo_composite.cpp

#include "stereo_composite.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <tcb/span.hpp>

using namespace srb2;
using namespace srb2::hwr2;
using namespace srb2::rhi;

// Same vertex format + index buffer as BlitRectPass — keep them in sync so
// the embedded stereocomposite shader can use the standard attribute
// bindings (a_position vec3, a_texcoord0 vec2).
namespace
{
struct BlitVertex
{
	float x = 0.f;
	float y = 0.f;
	float z = 0.f;
	float u = 0.f;
	float v = 0.f;
};
} // namespace

static const BlitVertex kVerts[] =
	{{-.5f, -.5f, 0.f, 0.f, 0.f}, {.5f, -.5f, 0.f, 1.f, 0.f}, {-.5f, .5f, 0.f, 0.f, 1.f}, {.5f, .5f, 0.f, 1.f, 1.f}};

static const uint16_t kIndices[] = {0, 1, 2, 1, 3, 2};

StereoCompositePass::StereoCompositePass() = default;
StereoCompositePass::~StereoCompositePass() = default;

void StereoCompositePass::draw(Rhi& rhi)
{
	prepass(rhi);
	transfer(rhi);
	graphics(rhi);
}

void StereoCompositePass::prepass(Rhi& rhi)
{
	if (!program_)
	{
		// Reuse the "ENABLE_VA_TEXCOORD0" define convention so the embedded
		// vertex shader can compile cleanly even if downstream code expects
		// a-texcoord0-bound varyings. The shader source itself is delivered
		// by the built-in registry hook in SdlGl2Platform::find_shader_sources.
		ProgramDesc desc {};
		const char* defines[1] = {"ENABLE_VA_TEXCOORD0"};
		desc.name = "stereocomposite";
		desc.defines = tcb::make_span(defines);
		program_ = rhi.create_program(desc);
	}

	if (!quad_vbo_)
	{
		quad_vbo_ = rhi.create_buffer({sizeof(kVerts), BufferType::kVertexBuffer, BufferUsage::kImmutable});
		quad_vbo_needs_upload_ = true;
	}

	if (!quad_ibo_)
	{
		quad_ibo_ = rhi.create_buffer({sizeof(kIndices), BufferType::kIndexBuffer, BufferUsage::kImmutable});
		quad_ibo_needs_upload_ = true;
	}
}

void StereoCompositePass::transfer(Rhi& rhi)
{
	if (quad_vbo_needs_upload_ && quad_vbo_)
	{
		rhi.update_buffer(quad_vbo_, 0, tcb::as_bytes(tcb::span(kVerts)));
		quad_vbo_needs_upload_ = false;
	}

	if (quad_ibo_needs_upload_ && quad_ibo_)
	{
		rhi.update_buffer(quad_ibo_, 0, tcb::as_bytes(tcb::span(kIndices)));
		quad_ibo_needs_upload_ = false;
	}
}

void StereoCompositePass::graphics(Rhi& rhi)
{
	rhi.bind_program(program_);

	RasterizerStateDesc rs {};
	rs.cull = CullMode::kNone;
	rhi.set_rasterizer_state(rs);

	rhi.bind_vertex_attrib("a_position",  quad_vbo_, VertexAttributeFormat::kFloat3, offsetof(BlitVertex, x), sizeof(BlitVertex));
	rhi.bind_vertex_attrib("a_texcoord0", quad_vbo_, VertexAttributeFormat::kFloat2, offsetof(BlitVertex, u), sizeof(BlitVertex));

	// Identity NDC quad (matches BlitRectPass when scale=1, aspect-preserving
	// fits within the output viewport). Y is flipped via texcoord0_transform
	// so the source backbuffer (GL bottom-up) reads with top of content at
	// top of display — same flip BlitRectPass does for its output.
	glm::mat4 projection = glm::scale(glm::identity<glm::mat4>(), glm::vec3(1.f, -1.f, 1.f));
	glm::mat4 modelview  = glm::scale(glm::identity<glm::mat4>(), glm::vec3(2.f,  2.f, 1.f));
	glm::mat3 texcoord0_transform = glm::mat3(
		glm::vec3(1.f,  0.f, 0.f),
		glm::vec3(0.f, -1.f, 0.f),
		glm::vec3(0.f,  1.f, 1.f));

	rhi.set_uniform("u_projection", projection);
	rhi.set_uniform("u_modelview", modelview);
	rhi.set_uniform("u_texcoord0_transform", texcoord0_transform);
	rhi.set_uniform("u_stereo_mode", stereo_mode_);

	rhi.set_sampler("s_sampler0", 0, texture_);
	rhi.set_viewport(output_position_);
	rhi.bind_index_buffer(quad_ibo_);
	rhi.draw_indexed(6, 0);
}
