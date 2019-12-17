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
#include <stdio.h> 
#include <vmath.h>

#define CHUNK_WIDTH 16
#define CHUNK_HEIGHT 256
#define CHUNK_DEPTH 16
#define CHUNK_SIZE (CHUNK_WIDTH * CHUNK_DEPTH * CHUNK_HEIGHT)

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

// get surrounding chunks, but only the ones on the sides
static inline std::vector<ivec2> surrounding_chunks_sides_s(ivec2 chunk_coord) {
	return {
		// sides
		chunk_coord + ivec2(1, 0),
		chunk_coord + ivec2(0, 1),
		chunk_coord + ivec2(-1, 0),
		chunk_coord + ivec2(0, -1),
	};
}

class Chunk : public ChunkData {
public:
	vmath::ivec2 coords; // coordinates in chunk format
	MiniChunk minis[CHUNK_HEIGHT / MINICHUNK_HEIGHT];

	Chunk() : ChunkData(CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_DEPTH) {}

	Chunk(Block* data, vmath::ivec2 coords) : ChunkData(data, CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_DEPTH), coords(coords) {}

	// split chunk into minichunks
	inline void gen_minichunks() {
		assert(data != nullptr);

		for (int i = 0; i < MINIS_PER_CHUNK; i++) {
			// create mini and populate it
			minis[i].data = data + i * MINICHUNK_SIZE;
			minis[i].coords = ivec3(coords[0], i*MINICHUNK_HEIGHT, coords[1]);

			//// TODO: Use this as a primary method of drawing, before meshes are generated?
			//// create CUBE buffer
			//glCreateBuffers(1, &mini.block_types_buf);
			//glNamedBufferStorage(mini.block_types_buf, MINICHUNK_SIZE * sizeof(Block), NULL, GL_DYNAMIC_STORAGE_BIT);

			//// fill CUBE buffer, cuz we already have all the data we need
			//glNamedBufferSubData(mini.block_types_buf, 0, MINICHUNK_SIZE * sizeof(Block), mini.data);
		}
	}

	inline MiniChunk* get_mini_with_y_level(int y) {
		if (!data) {
			return nullptr;
		}

		// TODO: Remove loop.
		for (int i = 0; i < MINIS_PER_CHUNK; i++) {
			if (minis[i].coords[1] == y) {
				return &(minis[i]);
			}
		}

		// Somehow reached here, bug.
		throw "Bug";
	}

	// render this chunk
	inline void render(OpenGLInfo* glInfo) {
		for (auto &mini : minis) {
			//mini.render_cubes(glInfo);
			mini.render_meshes(glInfo);
		}
	}

	inline std::vector<ivec2> surrounding_chunks() {
		return surrounding_chunks_s(coords);
	}

	inline std::vector<ivec2> surrounding_chunks_sides() {
		return surrounding_chunks_sides_s(coords);
	}
};

// simple chunk hash function
struct chunk_hash
{
	std::size_t operator() (const Chunk* chunk) const
	{
		return std::hash<int>()(chunk->coords[0]) ^ std::hash<int>()(chunk->coords[1]);
	}
};


Chunk* gen_chunk_data(int, int);

inline Chunk* gen_chunk_data(ivec2 vec) { return gen_chunk_data(vec[0], vec[1]); }

// TODO: This maybe?
//#define EL_TYPE uint8_t
//#define EL_SIZE sizeof(EL_TYPE)

#endif // __CHUNK_H__
