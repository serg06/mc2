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
#include <memory>
#include <stdio.h> 

constexpr int CHUNK_WIDTH = 16;
constexpr int CHUNK_HEIGHT = 256;
constexpr int CHUNK_DEPTH = 16;
constexpr int CHUNK_SIZE = CHUNK_WIDTH * CHUNK_DEPTH * CHUNK_HEIGHT;

namespace {
	static std::unique_ptr<BlockType[]> __chunk_tmp_storage = std::make_unique<BlockType[]>(CHUNK_SIZE);
}

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

static inline std::vector<ivec2> surrounding_chunks_s(const ivec2& chunk_coord) {
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
static inline std::vector<ivec2> surrounding_chunks_sides_s(const ivec2& chunk_coord) {
	return {
		// sides
		chunk_coord + ivec2(1, 0),
		chunk_coord + ivec2(0, 1),
		chunk_coord + ivec2(-1, 0),
		chunk_coord + ivec2(0, -1),
	};
}

class Chunk {
public:
	vmath::ivec2 coords; // coordinates in chunk format
	MiniChunk minis[CHUNK_HEIGHT / MINICHUNK_HEIGHT];

	Chunk() : Chunk({ 0, 0 }) {}
	Chunk(const vmath::ivec2& coords) : coords(coords) {}

	// convert coordinates to idx
	static inline int c2idx_chunk(const int &x, const int &y, const int &z) {
		return x + z * CHUNK_WIDTH + y * CHUNK_WIDTH * CHUNK_DEPTH;
	}
	static inline int c2idx_chunk(const vmath::ivec3 &xyz) { return c2idx_chunk(xyz[0], xyz[1], xyz[2]); }


	// initialize minichunks by setting coords and allocating space
	inline void init_minichunks() {
		for (int i = 0; i < MINIS_PER_CHUNK; i++) {
			// create mini and populate it
			minis[i].set_coords({ coords[0], i*MINICHUNK_HEIGHT, coords[1] });
			minis[i].allocate();
			minis[i].set_all_air();
		}
	}

	inline MiniChunk* get_mini_with_y_level(int y) {
		return &minis[y/16];
	}

	// get block at these coordinates
	inline BlockType get_block(const int &x, const int &y, const int &z) {
		return get_mini_with_y_level(y)->get_block(x, y % MINICHUNK_HEIGHT, z);
	}

	inline BlockType get_block(const vmath::ivec3 &xyz) { return get_block(xyz[0], xyz[1], xyz[2]); }
	inline BlockType get_block(const vmath::ivec4 &xyz_) { return get_block(xyz_[0], xyz_[1], xyz_[2]); }

	// set blocks in map using array, efficiently
	inline void set_blocks(BlockType* new_blocks) {
		for (int y = 0; y < BLOCK_MAX_HEIGHT; y += MINICHUNK_HEIGHT) {
			get_mini_with_y_level(y)->set_blocks(new_blocks + MINICHUNK_WIDTH * MINICHUNK_DEPTH * y);
		}
	}

	// set block at these coordinates
	// TODO: create a set_block_range that takes a min_xyz and max_xyz and efficiently set them.
	inline void set_block(int x, int y, int z, const BlockType &val) {
		get_mini_with_y_level(y)->set_block(x, y % MINICHUNK_HEIGHT, z, val);
	}

	inline void set_block(const vmath::ivec3 &xyz, const BlockType &val) { return set_block(xyz[0], xyz[1], xyz[2], val); }
	inline void set_block(const vmath::ivec4 &xyz_, const BlockType &val) { return set_block(xyz_[0], xyz_[1], xyz_[2], val); }

	// get metadata at these coordinates
	inline Metadata get_metadata(const int &x, const int &y, const int &z) {
		return get_mini_with_y_level(y)->get_metadata(x, y % MINICHUNK_HEIGHT, z);
	}

	inline Metadata get_metadata(const vmath::ivec3 &xyz) { return get_metadata(xyz[0], xyz[1], xyz[2]); }
	inline Metadata get_metadata(const vmath::ivec4 &xyz_) { return get_metadata(xyz_[0], xyz_[1], xyz_[2]); }

	// set metadata at these coordinates
	inline void set_metadata(const int x, const int y, const int z, const Metadata &val) {
		get_mini_with_y_level(y)->set_metadata(x, y % MINICHUNK_HEIGHT, z, val);
	}

	inline void set_metadata(const vmath::ivec3 &xyz, const Metadata &val) { return set_metadata(xyz[0], xyz[1], xyz[2], val); }
	inline void set_metadata(const vmath::ivec4 &xyz_, const Metadata &val) { return set_metadata(xyz_[0], xyz_[1], xyz_[2], val); }


	// TODO: replace this with unique_ptr
	inline void free() {
		for (auto &mini : minis) {
			mini.free();
		}
	}


	// render this chunk
	inline void render(OpenGLInfo* glInfo) {
		for (auto &mini : minis) {
			mini.render_meshes(glInfo);
		}
	}

	inline std::vector<ivec2> surrounding_chunks() const {
		return surrounding_chunks_s(coords);
	}

	inline std::vector<ivec2> surrounding_chunks_sides() const {
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
