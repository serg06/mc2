// UTIL class filled with useful static functions
#ifndef __CHUNKS_H__
#define __CHUNKS_H__

#include <cstdint>

/*
*
* CHUNK FORMAT
*	- first increase x coordinate
*	- then  increase z coordinate
*	- then  increase y coordinate
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

static inline void chunk_set(Block* chunk, uint8_t x, uint8_t y, uint8_t z, Block val) {
	chunk[x + z * CHUNK_WIDTH + y * CHUNK_WIDTH * CHUNK_DEPTH] = val;
}

static inline Block chunk_get(uint8_t* chunk, uint8_t x, uint8_t y, uint8_t z) {
	chunk[x + z * CHUNK_WIDTH + y * CHUNK_WIDTH * CHUNK_DEPTH];
}

Block* gen_chunk();




// TODO: This maybe?
//#define EL_TYPE uint8_t
//#define EL_SIZE sizeof(EL_TYPE)

#endif // __CHUNKS_H__
