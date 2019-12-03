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

using namespace std;
using namespace vmath;

double noise22(float vec[2]) {
	double result = noise2(vec);
	result = (result + 1.0f) / 2.0f;
	assert(result >= 0);
	assert(result <= 1);
	return result;
}

Chunk* gen_chunk(int chunkX, int chunkZ) {
	char buf[256];
	FastNoise fn;

	// create chunk
	Chunk* chunk = new Chunk();
	chunk->set_all_air();
	chunk->coords = { chunkX, chunkZ };

	// create its buffer
	glCreateBuffers(1, &chunk->gl_buf);
	glNamedBufferStorage(chunk->gl_buf, CHUNK_SIZE * sizeof(Block), NULL, GL_DYNAMIC_STORAGE_BIT);

	//// fill data
	//for (int x = 0; x < CHUNK_WIDTH; x++) {
	//	for (int z = 0; z < CHUNK_DEPTH; z++) {

	//		// get height at this location
	//		double y = fn.GetSimplex((FN_DECIMAL)(x + chunkX * 16), (FN_DECIMAL)(z + chunkZ * 16));

	//		y = (y + 1.0) / 2.0; // normalize to [0.0, 1.0]
	//		y *= 12; // variation of around 6
	//		y += 55; // minimum height 60

	//		// fill everything under that height
	//		for (int i = 0; i < y; i++) {
	//			chunk->set_block(x, i, z, Block::Stone);
	//		}
	//		chunk->set_block(x, (int)floor(y), z, Block::Grass);
	//	}
	//}

	// DEBUG: Custom chunks.
	chunk->set_block(0, 0, 0, Block::Stone);
	chunk->set_block(0, 0, 1, Block::Stone);
	chunk->set_block(1, 0, 0, Block::Stone);
	chunk->set_block(1, 0, 1, Block::Stone);

	chunk->set_block(0, 1, 0, Block::Stone);
	chunk->set_block(0, 1, 1, Block::Stone);
	chunk->set_block(1, 1, 0, Block::Stone);
	chunk->set_block(1, 1, 1, Block::Stone);


	// fill buffer
	glNamedBufferSubData(chunk->gl_buf, 0, CHUNK_SIZE * sizeof(Block), chunk->data);

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
