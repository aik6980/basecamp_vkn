#include "arcball.h"

#include "app.h"
//#include "input/input_manager.h"

void Arcball::init(const glm::vec3& pos, const glm::vec3& target)
{
	m_target	 = target;
	m_zoom		 = glm::length(target - pos);
	glm::mat4 view	 = glm::lookAtLH(pos, target, glm::vec3(0.0f, 1.0f, 0.0f)); // CreateLookAtLH(pos, target, Vector3::Up);
	auto&& world = glm::inverse(view);
	m_orient	 = glm::quat_cast(world);
}

#if 0
void Arcball::update()
{
	switch (m_state) {
	case State_idle: {
		if (App::input().mouse_down(OIS::MouseButtonID::MB_Right)) {
			m_state = State_rotate_arcball;

			m_begin_mouse_pos = App::input().mouse_npos();
			m_begin_orient	  = m_orient;
		}
	} break;
	case State_rotate_arcball: {
		if (App::input().mouse_down(OIS::MouseButtonID::MB_Right) == false) {
			m_state = State_idle;
		}

		// auto&& mouse_pos = App::input().mouse_npos();
		//
		// auto&& curr_arcball_pos = npos_to_vector(mouse_pos);
		// auto&& prev_arcball_pos = npos_to_vector(m_begin_mouse_pos);
		//// find rot quaternion from point a to point b on the unit sphere
		// Quaternion rot;
		//{
		//     // https://stackoverflow.com/questions/1171849/finding-quaternion-representing-the-rotation-from-one-vector-to-another
		//     // Quaternion q;
		//     // vector a = crossproduct(v1, v2);
		//     // q.xyz = a;
		//     // q.w = sqrt((v1.Length ^ 2) * (v2.Length ^ 2)) + dotproduct(v1, v2);
		//     auto&&  v_from = prev_arcball_pos;
		//     auto&&  v_to   = curr_arcball_pos;
		//     Vector3 cross  = v_from.Cross(v_to);
		//     float   dot    = v_from.Dot(v_to);
		//
		//     rot = Quaternion(cross.x, cross.y, cross.z, dot);
		// }
		//
		// m_orient = rot * m_begin_orient;

		auto&&	mouse_pos	= App::input().mouse_npos();
		Vector2 delta_angle = (mouse_pos - m_begin_mouse_pos);

		delta_angle.x *= XM_2PI;
		delta_angle.y *= -XM_PI;

		// find new camera orientation
		auto&& yaw_rot	 = Quaternion::CreateFromYawPitchRoll(delta_angle.x, 0.0f, 0.0f);
		auto&& pitch_rot = Quaternion::CreateFromYawPitchRoll(0.0f, delta_angle.y, 0.0f);
		m_orient		 = Quaternion::Concatenate(yaw_rot, m_orient);
		m_orient		 = Quaternion::Concatenate(m_orient, pitch_rot);

		m_begin_mouse_pos = mouse_pos;
	} break;
	}

	// update zoom
	auto&& zoom = App::input().mouse_scroll();
	m_zoom += zoom * -0.01f;
}
#endif

glm::vec3 Arcball::npos_to_vector(const glm::vec2& npos)
{
	// adjust the arcball size inscreen space
	glm::vec2 pos = npos * 1.0f / m_radius_screen_space;

	// calculate surface position of unit sphere from 2d pos
	float z	   = 0.0f;
	float l_sq = glm::dot(pos, pos);

	if (l_sq > 1.0f) {
		float scale = 1.0f / sqrtf(l_sq);
		pos *= scale;
	}
	else {
		z = sqrtf(1.0f - l_sq);
	}

	return glm::vec3(pos.x, pos.y, z);
}

glm::vec3 Arcball::pos()
{
	glm::mat4 world = glm::mat4_cast(m_orient);
	auto forward	 = glm::vec3(world[2]);
	return m_target + forward * m_zoom;
}

glm::mat4 Arcball::view()
{
	glm::mat4 world = glm::mat4_cast(m_orient);
	auto forward	 = glm::vec3(world[2]);
	auto&& pos	 = m_target + forward * m_zoom;
	world[3] = glm::vec4(pos, 1.0f);
	auto&& view = glm::inverse(world);
	return view;
}
