// DR. ROBOTNIK'S RING RACERS
//-----------------------------------------------------------------------------
// Copyright (C) 2025 by OneUp
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  stereo_composite.hpp
/// \brief HWR2 pass that runs a per-mode stereoscopic composite shader on the
///        main backbuffer in place of the BlitRectPass scale step. Used by
///        the shader-composite display modes (Anaglyph, Row/Column-Interlaced,
///        Checkerboard) — the SbS/TaB / Off / LeiaSR paths keep using the
///        existing BlitRectPass family.

#ifndef __SRB2_HWR2_STEREO_COMPOSITE_HPP__
#define __SRB2_HWR2_STEREO_COMPOSITE_HPP__

#include "../rhi/rhi.hpp"

namespace srb2::hwr2
{

class StereoCompositePass
{
public:
	StereoCompositePass();
	~StereoCompositePass();
	StereoCompositePass(const StereoCompositePass&) = delete;
	StereoCompositePass& operator=(const StereoCompositePass&) = delete;

	void draw(rhi::Rhi& rhi);

	void set_texture(rhi::Handle<rhi::Texture> texture, uint32_t width, uint32_t height) noexcept
	{
		texture_ = texture;
		texture_width_ = width;
		texture_height_ = height;
	}

	void set_output(int32_t x, int32_t y, uint32_t width, uint32_t height) noexcept
	{
		output_position_ = {x, y, width, height};
	}

	// Integer matches the stereomode_t enum in hw_stereo.h. Caller passes the
	// *raw* cv_stereomode value (not R_StereoMode()): the shader composite
	// only runs for the four shader-composite user modes; the internal SBS/TAB
	// substitution is consumed render-side, not at present time.
	void set_stereo_mode(int32_t mode) noexcept { stereo_mode_ = mode; }

private:
	void prepass(rhi::Rhi& rhi);
	void transfer(rhi::Rhi& rhi);
	void graphics(rhi::Rhi& rhi);

	rhi::Handle<rhi::Program> program_;
	rhi::Handle<rhi::Texture> texture_;
	uint32_t texture_width_ = 0;
	uint32_t texture_height_ = 0;
	rhi::Rect output_position_ {};
	rhi::Handle<rhi::Buffer> quad_vbo_;
	rhi::Handle<rhi::Buffer> quad_ibo_;
	bool quad_vbo_needs_upload_ = false;
	bool quad_ibo_needs_upload_ = false;
	int32_t stereo_mode_ = 0;
};

} // namespace srb2::hwr2

#endif // __SRB2_HWR2_STEREO_COMPOSITE_HPP__
