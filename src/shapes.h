#ifndef __SHAPES_H__
#define __SHAPES_H__

#include "util.h"

#include "cmake_pch.hxx"

// player dimensions
// NOTE: May cause extra t-junctions.
constexpr float PLAYER_RADIUS = 0.3f;
constexpr float PLAYER_WIDTH = PLAYER_RADIUS * 2;
constexpr float PLAYER_DEPTH = PLAYER_RADIUS * 2;
constexpr float PLAYER_HEIGHT = 1.8f;
constexpr float CAMERA_HEIGHT = 1.6f;

// coordinates of player's bounding box w.r.t. player
#define PLAYER_NORTH_0 (NORTH_0 * PLAYER_DEPTH / 2.0f)
#define PLAYER_SOUTH_0 (SOUTH_0 * PLAYER_DEPTH / 2.0f)

#define PLAYER_EAST_0 (EAST_0 * PLAYER_WIDTH / 2.0f)
#define PLAYER_WEST_0 (WEST_0 * PLAYER_WIDTH / 2.0f)

#define PLAYER_UP_0 (UP_0 * PLAYER_HEIGHT)
constexpr vmath::vec4 PLAYER_DOWN_0 = vmath::vec4(0.0f);

namespace shapes {
	// Cube at origin
	static const GLfloat cube_centered_half[108] =
	{
		-0.25f, 0.25f, -0.25f,
		-0.25f, -0.25f, -0.25f,
		0.25f, -0.25f, -0.25f,

		0.25f, -0.25f, -0.25f,
		0.25f, 0.25f, -0.25f,
		-0.25f, 0.25f, -0.25f,

		0.25f, -0.25f, -0.25f,
		0.25f, -0.25f, 0.25f,
		0.25f, 0.25f, -0.25f,

		0.25f, -0.25f, 0.25f,
		0.25f, 0.25f, 0.25f,
		0.25f, 0.25f, -0.25f,

		0.25f, -0.25f, 0.25f,
		-0.25f, -0.25f, 0.25f,
		0.25f, 0.25f, 0.25f,

		-0.25f, -0.25f, 0.25f,
		-0.25f, 0.25f, 0.25f,
		0.25f, 0.25f, 0.25f,

		-0.25f, -0.25f, 0.25f,
		-0.25f, -0.25f, -0.25f,
		-0.25f, 0.25f, 0.25f,

		-0.25f, -0.25f, -0.25f,
		-0.25f, 0.25f, -0.25f,
		-0.25f, 0.25f, 0.25f,

		-0.25f, -0.25f, 0.25f,
		0.25f, -0.25f, 0.25f,
		0.25f, -0.25f, -0.25f,

		0.25f, -0.25f, -0.25f,
		-0.25f, -0.25f, -0.25f,
		-0.25f, -0.25f, 0.25f,

		-0.25f, 0.25f, -0.25f,
		0.25f, 0.25f, -0.25f,
		0.25f, 0.25f, 0.25f,

		0.25f, 0.25f, 0.25f,
		-0.25f, 0.25f, 0.25f,
		-0.25f, 0.25f, -0.25f,
	};

	static const GLfloat cube_small[108] =
	{
		0.0f, 0.5f, 0.0f,
		0.0f, 0.0f, 0.0f,
		0.5f, 0.0f, 0.0f,

		0.5f, 0.0f, 0.0f,
		0.5f, 0.5f, 0.0f,
		0.0f, 0.5f, 0.0f,

		0.5f, 0.0f, 0.0f,
		0.5f, 0.0f, 0.5f,
		0.5f, 0.5f, 0.0f,

		0.5f, 0.0f, 0.5f,
		0.5f, 0.5f, 0.5f,
		0.5f, 0.5f, 0.0f,

		0.5f, 0.0f, 0.5f,
		0.0f, 0.0f, 0.5f,
		0.5f, 0.5f, 0.5f,

		0.0f, 0.0f, 0.5f,
		0.0f, 0.5f, 0.5f,
		0.5f, 0.5f, 0.5f,

		0.0f, 0.0f, 0.5f,
		0.0f, 0.0f, 0.0f,
		0.0f, 0.5f, 0.5f,

		0.0f, 0.0f, 0.0f,
		0.0f, 0.5f, 0.0f,
		0.0f, 0.5f, 0.5f,

		0.0f, 0.0f, 0.5f,
		0.5f, 0.0f, 0.5f,
		0.5f, 0.0f, 0.0f,

		0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.5f,

		0.0f, 0.5f, 0.0f,
		0.5f, 0.5f, 0.0f,
		0.5f, 0.5f, 0.5f,

		0.5f, 0.5f, 0.5f,
		0.0f, 0.5f, 0.5f,
		0.0f, 0.5f, 0.0f,
	};

	static const GLfloat cube_full[108] =
	{
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,

		1.0f, 0.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f,

		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 1.0f,
		1.0f, 1.0f, 0.0f,

		1.0f, 0.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 0.0f,

		1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 1.0f,

		0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 1.0f,

		0.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 0.0f,

		1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f,

		0.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		0.0f, 1.0f, 1.0f,
		0.0f, 1.0f, 0.0f,
	};

	static const GLfloat player[108] = {
		-0.3125f, 0.90625f, -0.3125f,
		-0.3125f, 0.0f, -0.3125f,
		0.3125f, 0.0f, -0.3125f,

		0.3125f, 0.0f, -0.3125f,
		0.3125f, 0.90625f, -0.3125f,
		-0.3125f, 0.90625f, -0.3125f,

		0.3125f, 0.0f, -0.3125f,
		0.3125f, 0.0f, 0.3125f,
		0.3125f, 0.90625f, -0.3125f,

		0.3125f, 0.0f, 0.3125f,
		0.3125f, 0.90625f, 0.3125f,
		0.3125f, 0.90625f, -0.3125f,

		0.3125f, 0.0f, 0.3125f,
		-0.3125f, 0.0f, 0.3125f,
		0.3125f, 0.90625f, 0.3125f,

		-0.3125f, 0.0f, 0.3125f,
		-0.3125f, 0.90625f, 0.3125f,
		0.3125f, 0.90625f, 0.3125f,

		-0.3125f, 0.0f, 0.3125f,
		-0.3125f, 0.0f, -0.3125f,
		-0.3125f, 0.90625f, 0.3125f,

		-0.3125f, 0.0f, -0.3125f,
		-0.3125f, 0.90625f, -0.3125f,
		-0.3125f, 0.90625f, 0.3125f,

		-0.3125f, 0.0f, 0.3125f,
		0.3125f, 0.0f, 0.3125f,
		0.3125f, 0.0f, -0.3125f,

		0.3125f, 0.0f, -0.3125f,
		-0.3125f, 0.0f, -0.3125f,
		-0.3125f, 0.0f, 0.3125f,

		-0.3125f, 0.90625f, -0.3125f,
		0.3125f, 0.90625f, -0.3125f,
		0.3125f, 0.90625f, 0.3125f,

		0.3125f, 0.90625f, 0.3125f,
		-0.3125f, 0.90625f, 0.3125f,
		-0.3125f, 0.90625f, -0.3125f,
	};

	static const GLfloat player_vertices[24] = {
		-0.3125f, 0.0f, -0.3125f,
		-0.3125f, 0.0f, 0.3125f,
		-0.3125f, 0.90625f, -0.3125f,
		-0.3125f, 0.90625f, 0.3125f,
		0.3125f, 0.0f, -0.3125f,
		0.3125f, 0.0f, 0.3125f,
		0.3125f, 0.90625f, -0.3125f,
		0.3125f, 0.90625f, 0.3125f,
	};
}

// given a player's position, what blocks does he intersect with?
static inline std::vector<vmath::ivec4> get_intersecting_blocks(const vec4& player_position) {
	// get x/y/z min/max
	const ivec3 xyzMin = { (int)floorf(player_position[0] - PLAYER_RADIUS), (int)floorf(player_position[1]), (int)floorf(player_position[2] - PLAYER_RADIUS) };
	const ivec3 xyzMax = { (int)floorf(player_position[0] + PLAYER_RADIUS), (int)floorf(player_position[1] + PLAYER_HEIGHT), (int)floorf(player_position[2] + PLAYER_RADIUS) };

	// get all blocks that our player intersects with
	std::vector<vmath::ivec4> blocks;
	for (int y = xyzMin[1]; y <= xyzMax[1]; y++) {
		for (int z = xyzMin[2]; z <= xyzMax[2]; z++) {
			for (int x = xyzMin[0]; x <= xyzMax[0]; x++) {
				blocks.push_back({ x, y, z, 0 });
			}
		}
	}

	return blocks;
}


#endif /* __SHAPES_H__ */
