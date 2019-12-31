// class for managing chunk data, however we're storing it
#ifndef __CHUNKDATA_H__
#define __CHUNKDATA_H__

#include "block.h"

#include "cmake_pch.hxx"

// Chunk size
#define BLOCK_MIN_HEIGHT 0
#define BLOCK_MAX_HEIGHT 255
#define MINIS_PER_CHUNK (CHUNK_HEIGHT / MINICHUNK_HEIGHT)

using namespace vmath;


static inline ivec4 clamp_coords_to_world(const ivec4 &coords) {
	return { coords[0], std::clamp(coords[1], 0, BLOCK_MAX_HEIGHT), coords[2], 0 };
}

static inline ivec3 clamp_coords_to_world(const ivec3 &coords) {
	return { coords[0], std::clamp(coords[1], 0, BLOCK_MAX_HEIGHT), coords[2] };
}

// block metadata
// stores some extra info depending on block type
// e.g. flowing water => last 4 bits store water level
union Metadata {
private:
	uint8_t data;

public:
	Metadata() : data(0) {} // TODO: don't set 0?
	Metadata(uint8_t data) : data(data) {}

	/* LIQUIDS | LAST 4 BITS | LIQUID LEVEL */

	uint8_t get_liquid_level() {
		return data & 0xF;
	}

	void set_liquid_level(uint8_t water_level) {
		assert(water_level == (water_level & 0xF) && "water level uses more than 4 bits");
		data = (data & 0xF0) | (water_level & 0xF);
	}
};

// block lighting
struct Lighting {
private:
	uint8_t data;

public:
	uint8_t get_sunlight() {
		return data >> 4;
	}

	void set_sunlight(uint8_t sunlight) {
		assert(sunlight == (sunlight & 0xF) && "sunlight level uses more than 4 bits");
		data = (sunlight << 4) | (data & 0xF);
	}

	uint8_t get_torchlight() {
		return data & 0xF;
	}

	void set_torchlight(uint8_t torchlight) {
		assert(torchlight == (torchlight & 0xF) && "torchlight level uses more than 4 bits");
		data = (data & 0xF0) | torchlight;
	}
};


// Chunk Data is always stored as width wide and depth deep
class ChunkData {
public:
	BlockType *blocks = nullptr;
	Metadata *metadatas = nullptr; // TODO: make this a 4-byte array?
	Lighting *lightings = nullptr;

	int width = 0;
	int height = 0;
	int depth = 0;

	// Memory leak, delete this when un-loading chunk from world.
	ChunkData(int width, int height, int depth) : ChunkData(width, height, depth, nullptr, nullptr, nullptr) {}

	ChunkData(int width, int height, int depth, BlockType* blocks, Metadata* metadatas, Lighting* lightings) : width(width), height(height), depth(depth), blocks(blocks), metadatas(metadatas), lightings(lightings) {
		assert(0 < width && "invalid chunk width");
		assert(0 < depth && "invalid chunk depth");
		assert(0 < height && "invalid chunk height");
	}

	inline void allocate() {
		blocks = new BlockType[width * height * depth];
		metadatas = new Metadata[width * height * depth];
		lightings = new Lighting[width * height * depth];
	}

	inline void free() {
		delete[] blocks;
	}

	inline int size() {
		return width * height * depth;
	}

	// get block at these coordinates
	inline BlockType get_block(const int &x, const int &y, const int &z) {
		assert(0 <= x && x < width && "get_block invalid x coordinate");
		assert(0 <= z && z < depth && "get_block invalid z coordinate");

		// Outside of height range is just air
		if (y < BLOCK_MIN_HEIGHT || y > BLOCK_MAX_HEIGHT) {
			return BlockType::Air;
		}

		return blocks[x + z * width + y * width * depth];
	}

	inline BlockType get_block(const vmath::ivec3 &xyz) { return get_block(xyz[0], xyz[1], xyz[2]); }
	inline BlockType get_block(const vmath::ivec4 &xyz_) { return get_block(xyz_[0], xyz_[1], xyz_[2]); }

	// set block at these coordinates
	inline void set_block(int x, int y, int z, const BlockType &val) {
		assert(0 <= x && x < width && "set_block invalid x coordinate");
		assert(0 <= y && y < height && "set_block invalid y coordinate");
		assert(0 <= z && z < depth && "set_block invalid z coordinate");

		blocks[x + z * width + y * width * depth] = val;
	}

	inline void set_block(const vmath::ivec3 &xyz, const BlockType &val) { return set_block(xyz[0], xyz[1], xyz[2], val); }
	inline void set_block(const vmath::ivec4 &xyz_, const BlockType &val) { return set_block(xyz_[0], xyz_[1], xyz_[2], val); }

	inline bool all_air() {
		return std::find_if(blocks, blocks + size(), [](BlockType b) {return b != BlockType::Air; }) == (blocks + size());
	}

	inline bool any_air() {
		return std::find(blocks, blocks + size(), BlockType::Air) < (blocks + size());
	}

	inline bool any_translucent() {
		return std::find_if(blocks, blocks + size(), [](BlockType b) { return b.is_translucent(); }) < (blocks + size());
	}

	inline void set_all_air() {
		memset(this->blocks, (uint8_t)BlockType::Air, sizeof(BlockType) * size());
	}

	inline int count_air() {
		return std::count(blocks, blocks + size(), BlockType::Air);
	}

	// get metadata at these coordinates
	inline Metadata get_metadata(const int &x, const int &y, const int &z) {
		assert(0 <= x && x < width && "get_metadata invalid x coordinate");
		assert(0 <= z && z < depth && "get_metadata invalid z coordinate");

		// nothing outside of height range
		if (y < BLOCK_MIN_HEIGHT || y > BLOCK_MAX_HEIGHT) {
			return 0;
		}

		return metadatas[x + z * width + y * width * depth];
	}

	inline Metadata get_metadata(const vmath::ivec3 &xyz) { return get_metadata(xyz[0], xyz[1], xyz[2]); }
	inline Metadata get_metadata(const vmath::ivec4 &xyz_) { return get_metadata(xyz_[0], xyz_[1], xyz_[2]); }

	// set metadata at these coordinates
	inline void set_metadata(int x, int y, int z, Metadata val) {
		assert(0 <= x && x < width && "set_metadata invalid x coordinate");
		assert(0 <= y && y < height && "set_metadata invalid y coordinate");
		assert(0 <= z && z < depth && "set_metadata invalid z coordinate");

		metadatas[x + z * width + y * width * depth] = val;
	}

	inline void set_metadata(const vmath::ivec3 &xyz, Metadata val) { return set_metadata(xyz[0], xyz[1], xyz[2], val); }
	inline void set_metadata(const vmath::ivec4 &xyz_, Metadata val) { return set_metadata(xyz_[0], xyz_[1], xyz_[2], val); }

	inline char* print_y_layer(int layer) {
		assert(layer < height && "cannot print this layer, too high");

		char* result = new char[16 * 16 * 8]; // up to 8 chars per block type
		char* tmp = result;

		for (int x = 0; x < width; x++) {
			tmp += sprintf(tmp, "[ ");

			for (int z = 0; z < depth; z++) {
				tmp += sprintf(tmp, "%d ", (uint8_t)get_block(x, layer, z));
			}

			tmp += sprintf(tmp, "]\n");
		}

		return result;
	}
};


#endif /* __CHUNKDATA_H__ */
