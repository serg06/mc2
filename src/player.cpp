#include "player.h"

#include "shapes.h"
#include "util.h"

vmath::vec4 Player::staring_direction() {
	return rotate_pitch_yaw(pitch, yaw) * NORTH_0;
}

vmath::vec4 Player::up_direction() {
	return rotate_pitch_yaw(pitch, yaw) * UP_0;
}

vmath::vec4 Player::right_direction() {
	return rotate_pitch_yaw(pitch, yaw) * EAST_0;
}
