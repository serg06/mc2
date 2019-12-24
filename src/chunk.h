// UTIL class filled with useful static functions
#ifndef __CHUNK_H__
#define __CHUNK_H__

#include "block.h"
#include "chunkdata.h"
#include "minichunk.h"
#include "render.h"
#include "util.h"

#include "cmake_pch.hxx"

#include <assert.h>
#include <cstdint>
#include <stdio.h> 

constexpr int CHUNK_WIDTH = 16;
constexpr int CHUNK_HEIGHT = 256;
constexpr int CHUNK_DEPTH = 16;
constexpr int CHUNK_SIZE = CHUNK_WIDTH * CHUNK_DEPTH * CHUNK_HEIGHT;

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

	Chunk() : Chunk({ 0, 0 }) {}
	Chunk(vmath::ivec2 coords) : coords(coords), ChunkData(CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_DEPTH) {};

	// split chunk into minichunks
	inline void gen_minichunks() {
		assert(data != nullptr);

		for (int i = 0; i < MINIS_PER_CHUNK; i++) {
			// create mini and populate it
			minis[i].data = data + i * MINICHUNK_SIZE;
			minis[i].set_coords({ coords[0], i*MINICHUNK_HEIGHT, coords[1] });
		}
	}

	inline MiniChunk* get_mini_with_y_level(int y) {
		if (!data) {
			return nullptr;
		}

		// TODO: Remove loop.
		for (int i = 0; i < MINIS_PER_CHUNK; i++) {
			if (minis[i].get_coords()[1] == y) {
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

	// generate this chunk
	void generate();
};

// simple chunk hash function
struct chunk_hash
{
	std::size_t operator() (const Chunk* chunk) const
	{
		return std::hash<int>()(chunk->coords[0]) ^ std::hash<int>()(chunk->coords[1]);
	}
};

#endif // __CHUNK_H__
