// class for managing chunk data, however we're storing it
#ifndef __CHUNKDATA_H__
#define __CHUNKDATA_H__

#include "block.h"
#include "util.h"

#include "cmake_pch.hxx"

#include <iterator>

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
	// TODO: unsigned short
	IntervalMap<short, BlockType> blocks;
	Metadata *metadatas = nullptr; // TODO: make this a 4-byte array?
	Lighting *lightings = nullptr;

	int width = 0;
	int height = 0;
	int depth = 0;

	// Memory leak, delete this when un-loading chunk from world.
	ChunkData(int width, int height, int depth) : width(width), height(height), depth(depth) {
		assert(0 < width && "invalid chunk width");
		assert(0 < depth && "invalid chunk depth");
		assert(0 < height && "invalid chunk height");
	}

	inline void allocate() {
		assert(metadatas == nullptr && lightings == nullptr);

		blocks.clear(BlockType::Air);
		metadatas = new Metadata[width * height * depth];
		lightings = new Lighting[width * height * depth];

		memset(lightings, 0, sizeof(Lighting) * width * height * depth);
	}

	// TODO: replace this with unique_ptr
	inline void free() {
		assert(metadatas != nullptr && lightings != nullptr);

		delete[] metadatas;
		delete[] lightings;

		blocks.clear(BlockType::Air);
		metadatas = nullptr;
		lightings = nullptr;
	}

	inline int size() {
		return width * height * depth;
	}

	// convert coordinates to idx
	constexpr inline int c2idx(const int &x, const int &y, const int &z) {
		return x + z * width + y * width * depth;
	}
	constexpr inline int c2idx(const vmath::ivec3 &xyz) { return c2idx(xyz[0], xyz[1], xyz[2]); }


	// get block at these coordinates
	inline BlockType get_block(const int &x, const int &y, const int &z) {
		assert(0 <= x && x < width && "get_block invalid x coordinate");
		assert(0 <= z && z < depth && "get_block invalid z coordinate");

		// Outside of height range is just air
		if (y < BLOCK_MIN_HEIGHT || y > BLOCK_MAX_HEIGHT) {
			return BlockType::Air;
		}

		return blocks[c2idx(x, y, z)];
	}

	inline BlockType get_block(const vmath::ivec3 &xyz) { return get_block(xyz[0], xyz[1], xyz[2]); }
	inline BlockType get_block(const vmath::ivec4 &xyz_) { return get_block(xyz_[0], xyz_[1], xyz_[2]); }

	// set block at these coordinates
	inline void set_block(int x, int y, int z, const BlockType &val) {
		assert(0 <= x && x < width && "set_block invalid x coordinate");
		assert(0 <= y && y < height && "set_block invalid y coordinate");
		assert(0 <= z && z < depth && "set_block invalid z coordinate");

		int idx = c2idx(x, y, z);
		blocks.set_interval(idx, idx + 1, val);
	}

	inline void set_block(const vmath::ivec3 &xyz, const BlockType &val) { return set_block(xyz[0], xyz[1], xyz[2], val); }
	inline void set_block(const vmath::ivec4 &xyz_, const BlockType &val) { return set_block(xyz_[0], xyz_[1], xyz_[2], val); }

	//// set all blocks in this range
	//inline void set_block_range(const vmath::ivec3 &min_xyz, const vmath::ivec3 &max_xyz, const BlockType &val) {
	//	std::vector<std::pair<
	//}

	inline bool all_air() {
		return blocks[0] == BlockType::Air && blocks.num_intervals() == 1;
	}

	inline bool any_air() {
		for (auto iter = blocks.begin(); iter != blocks.end(); iter++) {
			if (iter->first < width*depth*height && iter->second == BlockType::Air) {
				return true;
			}
		}
		//auto blocks_end = std::next(blocks.get_interval(MINICHUNK_SIZE - 1));
		//return std::find(blocks.begin(), blocks_end, BlockType::Air) != blocks_end;
	}

	inline bool any_translucent() {
		for (auto &iter = this->blocks.begin(); iter != blocks.end(); iter++) {
			if (iter->first < width*depth*height && iter->second.is_translucent()) {
				return true;
			}
		}
		//auto blocks_end = std::next(blocks.get_interval(MINICHUNK_SIZE - 1));
		//return std::find_if(blocks.begin(), blocks_end, [](BlockType b) { return b.is_translucent(); }) != blocks_end;
	}

	inline void set_all_air() {
		blocks.clear(BlockType::Air);
	}

	inline int count_air() {
		int num_air = 0;
		for (auto iter = blocks.begin(); iter != blocks.end(); iter++) {
			if (iter->first < width*depth*height && iter->second.is_translucent()) {
				num_air++;
			}
		}
		return num_air;
		//auto blocks_end = std::next(blocks.get_interval(MINICHUNK_SIZE - 1));
		//return std::count(blocks.begin(), blocks_end, BlockType::Air);
	}

	// get metadata at these coordinates
	inline Metadata get_metadata(const int &x, const int &y, const int &z) {
		assert(0 <= x && x < width && "get_metadata invalid x coordinate");
		assert(0 <= z && z < depth && "get_metadata invalid z coordinate");

		// nothing outside of height range
		if (y < BLOCK_MIN_HEIGHT || y > BLOCK_MAX_HEIGHT) {
			return 0;
		}

		return metadatas[c2idx(x, y, z)];
	}

	inline Metadata get_metadata(const vmath::ivec3 &xyz) { return get_metadata(xyz[0], xyz[1], xyz[2]); }
	inline Metadata get_metadata(const vmath::ivec4 &xyz_) { return get_metadata(xyz_[0], xyz_[1], xyz_[2]); }

	// set metadata at these coordinates
	inline void set_metadata(int x, int y, int z, Metadata &val) {
		assert(0 <= x && x < width && "set_metadata invalid x coordinate");
		assert(0 <= y && y < height && "set_metadata invalid y coordinate");
		assert(0 <= z && z < depth && "set_metadata invalid z coordinate");

		metadatas[c2idx(x, y, z)] = val;
	}

	inline void set_metadata(const vmath::ivec3 &xyz, Metadata &val) { return set_metadata(xyz[0], xyz[1], xyz[2], val); }
	inline void set_metadata(const vmath::ivec4 &xyz_, Metadata &val) { return set_metadata(xyz_[0], xyz_[1], xyz_[2], val); }

	// TODO: unique_ptr
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
