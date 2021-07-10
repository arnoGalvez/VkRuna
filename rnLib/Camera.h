// Copyright (c) 2021 Arno Galvez

#pragma once

#include "external/glm/gtc/quaternion.hpp"
#include "external/glm/mat4x4.hpp"
#include "external/glm/vec3.hpp"
#include "platform/defines.h"

namespace vkRuna
{
// Angles in degrees
struct cameraFrame_t
{
	float	  i			= 90.0f;
	float	  a			= 0.0f;
	float	  fov		= 90.0f;
	float	  nearPlane = 0.1f;
	float	  farPlane	= 200.0f;
	glm::vec3 p { 0.0f, 0.0f, 0.0f };
};

// Conventions:
// - Cartesian coordinate system is right handed
// - Up is +Z
// - Spherical coordinate system follows ISO standard 80000-2:2019, hence a point is denoted (radial distance,
// inclination (or polar angle), azimuth)
// - Clip space depth range is [0, 1]

class Camera
{
   public:
	void UpdateProjView();
	void Reset();

	glm::vec3		 GetFront();
	glm::vec3		 GetRight();
	cameraFrame_t &	 GetFrame() { return m_frame; }
	const glm::mat4 &GetProj() const { return m_proj; }
	const float *	 GetProjPtr() const { return &m_proj[ 0 ][ 0 ]; }
	const glm::mat4 &GetView() const { return m_view; }
	const float *	 GetViewPtr() const { return &m_view[ 0 ][ 0 ]; }

   private:
	// changing attributes order will at least fuck up GameLocal::P_Ticker()
	cameraFrame_t m_frame;
	glm::mat4	  m_proj;
	glm::mat4	  m_view;
};

} // namespace vkRuna
