// UTIL class filled with useful static functions
#ifndef __CHUNK_H__
#define __CHUNK_H__

#include "block.h"
#include "chunkdata.h"
#include "minichunk.h"
#include "render.h"
#include "util.h"

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

static inline void chunk_set(Block* chunk, int x, int y, int z, Block val) {
	chunk[x + z * CHUNK_WIDTH + y * CHUNK_WIDTH * CHUNK_DEPTH] = val;
}

static inline Block chunk_get(Block* chunk, int x, int y, int z) {
	return chunk[x + z * CHUNK_WIDTH + y * CHUNK_WIDTH * CHUNK_DEPTH];
}

static inline std::vector<ivec2> surrounding_chunks_s(ivec2 chunk_coord) {
	return {
		// sides
		chunk_coord + ivec2(1, 0),
		chunk_coord + ivec2(0, 1),
		chunk_coord + ivec2(-1, 0),
		chunk_coord + ivec2(0, -1),

		// corners
		chunk_coord + ivec2(1, 1),
		chunk_coord + ivec2(-1, 1),
		chunk_coord + ivec2(-1, -1),
		chunk_coord + ivec2(1, -1),
	};
}

class Chunk {
public:
	Block* data = nullptr;
	vmath::ivec2 coords; // coordinates in chunk format
	GLuint gl_buf; // opengl buffer with this chunk's data
	MiniChunk minis[CHUNK_HEIGHT / MINICHUNK_HEIGHT]; // TODO: maybe array of non-pointers?

	//inline vec4 coords_real() {
	//	return coords * 16;
	//}

	Chunk() {}

	Chunk(Block* data, vmath::ivec2 coords, GLuint gl_buf) {
		this->data = data;
		this->coords = coords;
		this->gl_buf = gl_buf;
	}

	// split chunk into minichunks
	inline void gen_minichunks() {
		assert(data);

		for (int i = 0; i < MINIS_PER_CHUNK; i++) {
			// create mini and populate it
			MiniChunk mini;
			mini.data = data + i * MINICHUNK_SIZE;
			mini.coords = ivec3(coords[0], i*MINICHUNK_HEIGHT, coords[1]);

			// create buffer
			glCreateBuffers(1, &mini.gl_buf);
			glNamedBufferStorage(mini.gl_buf, MINICHUNK_SIZE * sizeof(Block), NULL, GL_DYNAMIC_STORAGE_BIT);

			// fill buffer
			glNamedBufferSubData(mini.gl_buf, 0, MINICHUNK_SIZE * sizeof(Block), mini.data);

			// add it to our minis list
			minis[i] = mini;
		}
	}

	// get block at these coordinates
	inline Block get_block(int x, int y, int z) {
		assert(0 <= x && x < CHUNK_WIDTH && "chunk get_block invalid x coordinate");
		assert(0 <= y && y < CHUNK_HEIGHT && "chunk get_block invalid y coordinate");
		assert(0 <= z && z < CHUNK_DEPTH && "chunk get_block invalid z coordinate");

		return chunk_get(data, x, y, z);
	}

	// set block at these coordinates
	inline void set_block(int x, int y, int z, Block val) {
		assert(0 <= x && x < CHUNK_WIDTH && "chunk set_block invalid x coordinate");
		assert(0 <= y && y < CHUNK_HEIGHT && "chunk set_block invalid y coordinate");
		assert(0 <= z && z < CHUNK_DEPTH && "chunk set_block invalid z coordinate");

		data[x + z * CHUNK_WIDTH + y * CHUNK_WIDTH * CHUNK_DEPTH] = val;
	}

	// render this chunk
	inline void render(OpenGLInfo* glInfo) {
		for (auto mini : minis) {
			mini.render(glInfo);
		}
	}

	inline std::vector<ivec2> surrounding_chunks() {
		return surrounding_chunks_s(coords);
	}
};

Chunk* gen_chunk(int, int);

// TODO: This maybe?
//#define EL_TYPE uint8_t
//#define EL_SIZE sizeof(EL_TYPE)

#endif // __CHUNK_H__
