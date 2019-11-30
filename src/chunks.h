// UTIL class filled with useful static functions
#ifndef __CHUNKS_H__
#define __CHUNKS_H__

#include "GL/gl3w.h" // OutputDebugString

#include <assert.h>
#include <cstdint>
#include <vmath.h>

/*
*
* CHUNK FORMAT
*	- first increase x coordinate
*	- then  increase z coordinate
*	- then  increase y coordinate
*
* IDEA:
*   - chunk coordinate = 1/16th of actual coordinate
*
*/

// Chunk size
#define CHUNK_WIDTH 16
#define CHUNK_DEPTH 16
#define CHUNK_HEIGHT 256
#define CHUNK_SIZE (CHUNK_WIDTH * CHUNK_DEPTH * CHUNK_HEIGHT)

enum class Block : uint8_t {
	Air = 0,
	Grass = 1,
	Stone = 2,
};

inline std::string block_name(Block b) {
	switch (b) {
	case Block::Air:
		return "Air";
	case Block::Grass:
		return "Grass";
	case Block::Stone:
		return "Stone";
	default:
		assert(false && "Unknown block type.");
		return "UNKNOWN";
	}
}

static inline void chunk_set(Block* chunk, uint8_t x, uint8_t y, uint8_t z, Block val) {
	chunk[x + z * CHUNK_WIDTH + y * CHUNK_WIDTH * CHUNK_DEPTH] = val;
}

static inline Block chunk_get(Block* chunk, uint8_t x, uint8_t y, uint8_t z) {
	return chunk[x + z * CHUNK_WIDTH + y * CHUNK_WIDTH * CHUNK_DEPTH];
}

class Chunk {
public:
	Block data[16 * 16 * 256];
	vmath::ivec2 coords; // coordinates in chunk format

	//inline vec4 coords_real() {
	//	return coords * 16;
	//}

	// get block at these coordinates
	inline Block get_block(uint8_t x, uint8_t y, uint8_t z) {
		if (!data) {
			char buf[256];
			char *bufp = buf;
			bufp += sprintf(bufp, "No loaded block at (%d, %d, %d)", x, y, z);
			//bufp += sprintf(buf, " in chunk (%d, %d).", coords[0], coords[1]);
			bufp += sprintf(bufp, "\n");
			OutputDebugString(buf);
			return Block::Air;
		}
		return data[x + z * CHUNK_WIDTH + y * CHUNK_WIDTH * CHUNK_DEPTH];
	}

	// set block at these coordinates
	inline void set_block(uint8_t x, uint8_t y, uint8_t z, Block val) {
		data[x + z * CHUNK_WIDTH + y * CHUNK_WIDTH * CHUNK_DEPTH] = val;
	}
};

Chunk* gen_chunk(int, int);

// TODO: This maybe?
//#define EL_TYPE uint8_t
//#define EL_SIZE sizeof(EL_TYPE)

#endif // __CHUNKS_H__
