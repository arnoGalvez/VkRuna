// Copyright (c) 2021 Arno Galvez

#include "Camera.h"

#include "external/glm/gtc/matrix_transform.hpp"
#include "platform/Window.h"

namespace vkRuna
{
static const float	   pi_half = 3.1415926535898f / 2.0f;
static const glm::vec3 camUp( 0.0f, 0.0f, 1.0f );

void Camera::UpdateProjView()
{
	const auto &winProps = Window::GetInstance().GetProps();

	float aspect = static_cast< float >( winProps.width ) / static_cast< float >( winProps.height );

	m_proj = glm::perspective( glm::radians( m_frame.fov ), aspect, m_frame.nearPlane, m_frame.farPlane );
	m_proj[ 1 ][ 1 ] *= -1.0f; // In Vulkan viewport Y-axis points down

	float i_rad = glm::radians( m_frame.i );
	float a_rad = glm::radians( m_frame.a );

	glm::vec3 camFront( glm::sin( i_rad ) * glm::cos( a_rad ),
						glm::sin( i_rad ) * glm::sin( a_rad ),
						glm::cos( i_rad ) );

	m_view = glm::lookAt( m_frame.p, m_frame.p + camFront, camUp );
}

void Camera::Reset()
{
	m_frame = cameraFrame_t();
	UpdateProjView();
}

glm::vec3 Camera::GetFront()
{
	float i_rad = glm::radians( m_frame.i );
	float a_rad = glm::radians( m_frame.a );

	glm::vec3 camFront( glm::sin( i_rad ) * glm::cos( a_rad ),
						glm::sin( i_rad ) * glm::sin( a_rad ),
						glm::cos( i_rad ) );

	return camFront;
}

glm::vec3 Camera::GetRight()
{
	float i_rad = glm::radians( m_frame.i );
	float a_rad = glm::radians( m_frame.a );

	glm::vec3 camFront( glm::sin( i_rad ) * glm::cos( a_rad ),
						glm::sin( i_rad ) * glm::sin( a_rad ),
						glm::cos( i_rad ) );
	glm::vec3 camRight = glm::normalize( glm::cross( camUp, camFront ) );
	return camRight;
}

} // namespace vkRuna
