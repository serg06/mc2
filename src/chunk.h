#pragma once

#include "block.h"
#include "chunkdata.h"
#include "minichunk.h"
#include "render.h"
#include "util.h"

#include <assert.h>
#include <cstdint>
#include <memory>
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
class Chunk {
public:
	vmath::ivec2 coords; // coordinates in chunk format
	std::shared_ptr<MiniChunk> minis[CHUNK_HEIGHT / MINICHUNK_HEIGHT];

	Chunk();
	Chunk(const vmath::ivec2& coords);

	// initialize minichunks by setting coords and allocating space
	void init_minichunks();

	std::shared_ptr<MiniChunk> get_mini_with_y_level(const int y);

	void set_mini_with_y_level(const int y, std::shared_ptr<MiniChunk> mini);

	// get block at these coordinates
	BlockType get_block(const int& x, const int& y, const int& z);

	BlockType get_block(const vmath::ivec3& xyz);
	BlockType get_block(const vmath::ivec4& xyz_);

	// set blocks in map using array, efficiently
	void set_blocks(BlockType* new_blocks);

	// set block at these coordinates
	// TODO: create a set_block_range that takes a min_xyz and max_xyz and efficiently set them.
	void set_block(int x, int y, int z, const BlockType& val);

	void set_block(const vmath::ivec3& xyz, const BlockType& val);
	void set_block(const vmath::ivec4& xyz_, const BlockType& val);

	// get metadata at these coordinates
	Metadata get_metadata(const int& x, const int& y, const int& z);

	Metadata get_metadata(const vmath::ivec3& xyz);
	Metadata get_metadata(const vmath::ivec4& xyz_);

	// set metadata at these coordinates
	void set_metadata(const int x, const int y, const int z, const Metadata& val);

	void set_metadata(const vmath::ivec3& xyz, const Metadata& val);
	void set_metadata(const vmath::ivec4& xyz_, const Metadata& val);

	// TODO: Rename to clear()
	void free();

	std::vector<vmath::ivec2> surrounding_chunks() const;

	std::vector<vmath::ivec2> surrounding_chunks_sides() const;

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
