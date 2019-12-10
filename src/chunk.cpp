#include "chunk.h"
#include "chunkdata.h"
#include "kenp_noise/kenp_noise.h"
#include "util.h"

#include "GL/gl3w.h"
#include "FastNoise/FastNoise.h"

#include <assert.h>
#include <cstdlib>
#include <initializer_list>
#include <stdio.h>
#include <stdlib.h>
#include <vmath.h>

#define WATER_HEIGHT 64

using namespace std;
using namespace vmath;

double noise22(float vec[2]) {
	double result = noise2(vec);
	result = (result + 1.0f) / 2.0f;
	assert(result >= 0);
	assert(result <= 1);
	return result;
}

inline float softmax(float v, float minv, float maxv) {
	assert(maxv > minv);
	assert(minv <= v && v <= maxv);

	float dist = maxv - minv;
	float dist_rad = dist / 2.0f;
	float orig = v;

	// center everything
	v -= minv;
	v -= dist_rad;

	// squeeze from -1 to 1
	v /= dist_rad;

	int strength = 3;
	for (int i = 0; i < strength; i++) {
		// apply softmax
		v *= abs(v);
	}

	// stretch back out
	v *= dist_rad;

	// recenter at original point
	v += dist_rad;
	v += minv;

	return v;
}

Chunk* gen_chunk_data(int chunkX, int chunkZ) {
	char buf[256];
	FastNoise fn;

	// create chunk
	Chunk* chunk = new Chunk();
	chunk->set_all_air();
	chunk->coords = { chunkX, chunkZ };

	// fill data
	for (int x = 0; x < CHUNK_WIDTH; x++) {
		for (int z = 0; z < CHUNK_DEPTH; z++) {
			// get height at this location
			double y = fn.GetSimplex((FN_DECIMAL)(x + chunkX * 16) / 2.0, (FN_DECIMAL)(z + chunkZ * 16) / 2.0);
			y += fn.GetPerlin((FN_DECIMAL)(x + chunkX * 16) / 2.0, (FN_DECIMAL)(z + chunkZ * 16) / 2.0);
			y += fn.GetCellular((FN_DECIMAL)(x + chunkX * 16) /2.0, (FN_DECIMAL)(z + chunkZ * 16) / 2.0) / 2.0;
			y /= 2.5;

			y = (y + 1.0) / 2.0; // normalize to [0.0, 1.0]
			y *= 64; // variation of around 32
			y += 38; // minimum height 40

			// fill everything under that height
			for (int i = 0; i < y; i++) {
				chunk->set_block(x, i, z, Block::Stone);
			}
			chunk->set_block(x, (int)floor(y), z, Block::Grass);

			// generate tree if we wanna
			if (y >= WATER_HEIGHT) {
				float w = fn.GetWhiteNoise((FN_DECIMAL)(x + chunkX * 16), (FN_DECIMAL)(z + chunkZ * 16));
				w = (w + 1.0) / 2.0; // normalize random value to [0.0, 1.0]
				// 1/256 chance to make tree
				if (w <= (1.0f / 256.0f)) {
					// generate leaves
					for (int dx = -2; dx <= 2; dx++) {
						for (int dz = -2; dz <= 2; dz++) {
							for (int dy = 4; dy <= 5; dy++) {
								if (x + dx < 0 || x + dx >= 16 || z + dz < 0 || z + dz >= 16) {
									continue;
								}
								chunk->set_block(x + dx, y + dy, z + dz, Block::OakLeaves);
							}
						}
					}
					for (int dx = -1; dx <= 1; dx++) {
						for (int dz = -1; dz <= 1; dz++) {
							for (int dy = 6; dy <= 6; dy++) {
								if (x + dx < 0 || x + dx >= 16 || z + dz < 0 || z + dz >= 16) {
									continue;
								}
								chunk->set_block(x + dx, y + dy, z + dz, Block::OakLeaves);
							}
						}
					}
					for (int dx = -1; dx <= 1; dx++) {
						for (int dz = abs(dx) - 1; dz <= 1 - abs(dx); dz++) {
							for (int dy = 7; dy <= 7; dy++) {
								if (x + dx < 0 || x + dx >= 16 || z + dz < 0 || z + dz >= 16) {
									continue;
								}
								chunk->set_block(x + dx, y + dy, z + dz, Block::OakLeaves);
							}
						}
					}

					// generate logs
					for (int dy = 1; dy <= 5; dy++) {
						chunk->set_block(x, y + dy, z, Block::OakLog);
					}
				}
			}

			// Fill water
			if (y < WATER_HEIGHT - 1) {
				for (int y2 = y + 1; y2 < WATER_HEIGHT; y2++) {
					chunk->set_block(x, y2, z, Block::Water);
				}
			}
		}
	}

	// chunk is ready, generate mini-chunks
	chunk->gen_minichunks();

	return chunk;
};

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
	return ivec2((int)floorf(world_coords[0]), (int)floorf(world_coords[1]));
}
