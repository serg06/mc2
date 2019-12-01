// class for managing chunk data, however we're storing it
#ifndef __CHUNKDATA_H__
#define __CHUNKDATA_H__

#include "block.h"
#include "vmath.h"

// Chunk size
#define CHUNK_WIDTH 16
#define CHUNK_DEPTH 16
#define CHUNK_HEIGHT 256
#define CHUNK_SIZE (CHUNK_WIDTH * CHUNK_DEPTH * CHUNK_HEIGHT)

#define MINICHUNK_HEIGHT 16
#define MINICHUNK_SIZE (CHUNK_WIDTH * CHUNK_DEPTH * MINICHUNK_HEIGHT)

#define BLOCK_MAX_HEIGHT 255
#define MINIS_PER_CHUNK (CHUNK_HEIGHT / MINICHUNK_HEIGHT)

using namespace vmath;

static inline ivec4 clamp_coords_to_world(ivec4 coords) {
	coords[1] = std::clamp(coords[1], 0, BLOCK_MAX_HEIGHT);
	return coords;
}

// Chunk Data is always stored as CHUNK_WIDTH wide and CHUNK_DEPTH deep
class ChunkData {
	Block* data;
	int size;
	int height;

	ChunkData(Block* data, int size) {
		this->data = data;
		this->size = size;
		this->height = size / (CHUNK_WIDTH * CHUNK_DEPTH);

		assert(0 < size && size <= CHUNK_SIZE && "invalid chunk size");
		assert(0 < height && height <= CHUNK_HEIGHT && "invalid chunk height somehow");
	}

	// get block at these coordinates
	inline Block get_block(int x, int y, int z) {
		assert(0 <= x && x < CHUNK_WIDTH && "chunk get_block invalid x coordinate");
		assert(0 <= y && y < CHUNK_HEIGHT && "chunk get_block invalid y coordinate");
		assert(0 <= z && z < CHUNK_DEPTH && "chunk get_block invalid z coordinate");

		return data[x + z * CHUNK_WIDTH + y * CHUNK_WIDTH * CHUNK_DEPTH];
	}

	inline Block get_block(vmath::ivec3 xyz) { return get_block(xyz[0], xyz[1], xyz[2]); }
	inline Block get_block(vmath::ivec4 xyz_) { return get_block(xyz_[0], xyz_[1], xyz_[2]); }

	// set block at these coordinates
	inline void set_block(int x, int y, int z, Block val) {
		assert(0 <= x && x < CHUNK_WIDTH && "chunk set_block invalid x coordinate");
		assert(0 <= y && y < CHUNK_HEIGHT && "chunk set_block invalid y coordinate");
		assert(0 <= z && z < CHUNK_DEPTH && "chunk set_block invalid z coordinate");

		data[x + z * CHUNK_WIDTH + y * CHUNK_WIDTH * CHUNK_DEPTH] = val;
	}

	inline void set_block(vmath::ivec3 xyz, Block val) { return set_block(xyz[0], xyz[1], xyz[2], val); }
	inline void set_block(vmath::ivec4 xyz_, Block val) { return set_block(xyz_[0], xyz_[1], xyz_[2], val); }
};


#endif /* __CHUNKDATA_H__ */
