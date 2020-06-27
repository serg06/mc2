#include "shapes.h"

#include "vmath.h"

#include <cmath>
#include <vector>

// given a player's position, what blocks does he intersect with?
std::vector<vmath::ivec4> get_player_intersecting_blocks(const vmath::vec4& player_position) {
	// get x/y/z min/max
	const vmath::ivec3 xyzMin = { (int)floorf(player_position[0] - PLAYER_RADIUS), (int)floorf(player_position[1]), (int)floorf(player_position[2] - PLAYER_RADIUS) };
	const vmath::ivec3 xyzMax = { (int)floorf(player_position[0] + PLAYER_RADIUS), (int)floorf(player_position[1] + PLAYER_HEIGHT), (int)floorf(player_position[2] + PLAYER_RADIUS) };

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
