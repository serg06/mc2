#pragma once

#include "block.h"
#include "util.h"
#include "vmath.h"

#include <iterator>

// Chunk size
constexpr int BLOCK_MIN_HEIGHT = 0;
constexpr int BLOCK_MAX_HEIGHT = 255;
constexpr int MINIS_PER_CHUNK = 16; // TODO: = (CHUNK_HEIGHT / MINICHUNK_HEIGHT);


// block metadata
// stores some extra info depending on block type
// e.g. flowing water => last 4 bits store water level
union Metadata {
private:
	uint8_t data;

public:
	Metadata();
	Metadata(uint8_t data_);

	/* LIQUIDS | LAST 4 BITS | LIQUID LEVEL */

	uint8_t get_liquid_level() const;

	void set_liquid_level(const uint8_t water_level);

	// comparing to Metadata
	bool operator==(const Metadata& m) const;
	bool operator!=(const Metadata& m) const;

	// convert to base type
	operator uint8_t() const;
};

// block lighting
struct Lighting {
private:
	uint8_t data;

public:
	uint8_t get_sunlight() const;

	void set_sunlight(const uint8_t sunlight);

	uint8_t get_torchlight() const;

	void set_torchlight(const uint8_t torchlight);

	// comparing to Lighting
	bool operator==(const Lighting& l) const;
	bool operator!=(const Lighting& l) const;
};

// Chunk Data is always stored as width wide and depth deep
class ChunkData {
public:
	// TODO: unsigned short
	IntervalMap<short, BlockType> blocks;
	IntervalMap<short, Metadata> metadatas;
	IntervalMap<short, Metadata> lightings;

	const int width;
	const int height;
	const int depth;

	// Memory leak, delete this when un-loading chunk from world.
	ChunkData(const int width, const int height, const int depth);

	// Copy
	ChunkData(const ChunkData& other);

	void allocate();

	// TODO: replace allocate() and clear() with just reset()
	void clear();

	int size() const;

	// convert coordinates to idx
	constexpr  int c2idx(const int& x, const int& y, const int& z) const;
	constexpr  int c2idx(const vmath::ivec3& xyz) const;


	// get block at these coordinates
	BlockType get_block(const int& x, const int& y, const int& z) const;

	BlockType get_block(const vmath::ivec3& xyz) const;
	BlockType get_block(const vmath::ivec4& xyz_) const;

	// set block at these coordinates
	void set_block(const int x, const int y, const int z, const BlockType& val);

	void set_block(const vmath::ivec3& xyz, const BlockType& val);
	void set_block(const vmath::ivec4& xyz_, const BlockType& val);

	// set blocks in map using array, efficiently
	// relies on x -> z -> y
	void set_blocks(BlockType* new_blocks);

	/**
	 * Given a cube of chunkdata coordinates, convert it into optimal intervals.
	 * NOTE: Relies on the fact that we go in the order x, z, y.
	 */
	std::vector<std::pair<int, int>> optimize_intervals(const vmath::ivec3& min_xyz, const vmath::ivec3& max_xyz);

	bool all_air();

	bool any_air();

	bool any_translucent();

	void set_all_air();

	// get metadata at these coordinates
	Metadata get_metadata(const int& x, const int& y, const int& z) const;

	Metadata get_metadata(const vmath::ivec3& xyz) const;
	Metadata get_metadata(const vmath::ivec4& xyz_) const;

	// set metadata at these coordinates
	void set_metadata(const int x, const int y, const int z, const Metadata& val);

	void set_metadata(const vmath::ivec3& xyz, Metadata& val);
	void set_metadata(const vmath::ivec4& xyz_, Metadata& val);

	auto print_y_layer(const int layer);
};
