#include "chunks.h"

#include "util.h"

#include "GL/gl3w.h"

#include <cstdlib>
#include <initializer_list>
#include <stdio.h>
#include <stdlib.h>
#include <vmath.h>

using namespace std;
using namespace vmath;

// generate us a nice lil chunk
Block* gen_chunk() {
	Block* result = (Block*)calloc(sizeof(uint8_t) * CHUNK_SIZE, sizeof(uint8_t));

	for (int y = 0; y < CHUNK_HEIGHT; y++) {
		for (int x = 0; x < CHUNK_WIDTH; x++) {
			for (int z = 0; z < CHUNK_DEPTH; z++) {
				// 62 or lower = stone
				if (y <= 62) {
					chunk_set(result, x, y, z, Block::Stone);
				}

				// 63 to 64 = grass
				else if (y <= 64) {
					chunk_set(result, x, y, z, Block::Grass);
				}

				// 65 = maybe grass
				else if (y <= 65) {
					if (rand() % 2) {
						chunk_set(result, x, y, z, Block::Grass);
					}
				}

				// add some floating blocks
				else if (y == 68) {
					if (rand() % 4 == 0) {
						chunk_set(result, x, y, z, Block::Grass);
					}
				}

				// else air
				else {
					chunk_set(result, x, y, z, Block::Air);
				}
			}
		}
	}

	return result;
};

initializer_list<ivec2> surrounding_chunks(ivec2 chunk_coord) {
	return {
		chunk_coord + ivec2(0,  0),
		chunk_coord + ivec2(0,  1),
		chunk_coord + ivec2(1,  0),
		chunk_coord + ivec2(1,  1),
		chunk_coord + ivec2(0,  0),
		chunk_coord + ivec2(0, -1),
		chunk_coord + ivec2(-1,  0),
		chunk_coord + ivec2(-1, -1),
	};
}

// given player's coordinate, find all surrounding blocks (for collision detection) (TODO: Switch to array and add player_coord to each element. Or do for loop.)
// TODO: Do velocity-based checking.
//       - E.g.: Moving north-east = check all blocks on that side
//       -> Removes half the checking, just makes logic a little messier maybe?
initializer_list<vec4> surrounding_blocks(vec4 player_coord) {
	return {
	// North
	player_coord + NORTH_0 + DOWN_0,
	player_coord + NORTH_0,
	player_coord + NORTH_0 + UP_0,
	player_coord + NORTH_0 + UP_0 * 2,

	// South
	player_coord + SOUTH_0 + DOWN_0,
	player_coord + SOUTH_0,
	player_coord + SOUTH_0 + UP_0,
	player_coord + SOUTH_0 + UP_0 * 2,

	// East
	player_coord + EAST_0 + DOWN_0,
	player_coord + EAST_0,
	player_coord + EAST_0 + UP_0,
	player_coord + EAST_0 + UP_0 * 2,

	// West
	player_coord + WEST_0 + DOWN_0,
	player_coord + WEST_0,
	player_coord + WEST_0 + UP_0,
	player_coord + WEST_0 + UP_0 * 2,

	// Up/Down
	player_coord + UP_0 * 2,
	player_coord + DOWN_0,

	// Corners
	player_coord + NORTH_0 + EAST_0 + DOWN_0,
	player_coord + NORTH_0 + EAST_0,
	player_coord + NORTH_0 + EAST_0 + UP_0,
	player_coord + NORTH_0 + EAST_0 + UP_0 * 2,

	player_coord + NORTH_0 + WEST_0 + DOWN_0,
	player_coord + NORTH_0 + WEST_0,
	player_coord + NORTH_0 + WEST_0 + UP_0,
	player_coord + NORTH_0 + WEST_0 + UP_0 * 2,

	player_coord + SOUTH_0 + EAST_0 + DOWN_0,
	player_coord + SOUTH_0 + EAST_0,
	player_coord + SOUTH_0 + EAST_0 + UP_0,
	player_coord + SOUTH_0 + EAST_0 + UP_0 * 2,

	player_coord + SOUTH_0 + WEST_0 + DOWN_0,
	player_coord + SOUTH_0 + WEST_0,
	player_coord + SOUTH_0 + WEST_0 + UP_0,
	player_coord + SOUTH_0 + WEST_0 + UP_0 * 2,
	};
}

// CURRENT (stupid-feeling) IDEA:
// - Have function to return north blocks, south blocks, east blocks, west blocks, up blocks, and down blocks.
// - Then for each corner:
// - If ((corner.north % 1) > (player_position.north % 1))) // if player's corner is north of current block
//   - For each north block:
//     - if (corner.north > ...
// - Check if ((position > north) || (position < (south + NORTH_0)))
// - Check if ((position > north) || (position < (south + NORTH_0)))

// BETTER IDEA:
// - Calculate all corners
// - Calculate all blocks that contain your corners (just calculate ((vec4<int>) corners))
// - Calculate all north, south, east, west, up, and down blocks
// - If that block is in list of north/south blocks:
//   - Kill north/south velocity
// - TODO: Return it as an array?
// - TODO 2:
//   - Return ALL BLOCKS surrounding you (north/south/etc) as a 3D array (including ones you're standing in)
//   - Index into that array using your 8 corners!
// - Then if inside hard block (like stone/grass), kill relevant velocities.

// ANOTHER IDEA:
// - Every corner only has 8 possible blocks it could be intersecting
// - If we do it like that, it cuts down checks from corners*blocks (8*34 = 272) to corners*8 (8*8 = 64)
initializer_list<vec4> north_blocks(vec4 player_coord) {
	return {};
}



// transform a chunk's coords to real-world coords
inline vec4 chunk_to_world(ivec2 chunk_coords) {
	return vec4(chunk_coords[0] * 16.0f, chunk_coords[1] * 16.0f, 0.0f, 1.0f);
}

// transform world coordinates to chunk coordinates
inline ivec2 world_to_chunk(vec4 world_coords) {
	return ivec2(world_coords[0], world_coords[1]);
}
