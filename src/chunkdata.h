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


// Chunk Data is always stored as width wide and depth deep
class ChunkData {
public:
	BlockType * data = nullptr;

	int width = 0;
	int height = 0;
	int depth = 0;

	// Memory leak, delete this when un-loading chunk from world.
	ChunkData(int width, int height, int depth) : ChunkData(width, height, depth, nullptr) {}

	ChunkData(int width, int height, int depth, BlockType* data) : width(width), height(height), depth(depth), data(data) {
		assert(0 < width && "invalid chunk width");
		assert(0 < depth && "invalid chunk depth");
		assert(0 < height && "invalid chunk height");
	}

	inline void allocate() {
		data = new BlockType[width * height * depth];
	}

	inline void free() {
		delete[] data;
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

		return data[x + z * width + y * width * depth];
	}

	inline BlockType get_block(const vmath::ivec3 &xyz) { return get_block(xyz[0], xyz[1], xyz[2]); }
	inline BlockType get_block(const vmath::ivec4 &xyz_) { return get_block(xyz_[0], xyz_[1], xyz_[2]); }

	// set block at these coordinates
	inline void set_block(int x, int y, int z, BlockType val) {
		assert(0 <= x && x < width && "set_block invalid x coordinate");
		assert(0 <= y && y < height && "set_block invalid y coordinate");
		assert(0 <= z && z < depth && "set_block invalid z coordinate");

		data[x + z * width + y * width * depth] = val;
	}

	inline void set_block(vmath::ivec3 xyz, BlockType val) { return set_block(xyz[0], xyz[1], xyz[2], val); }
	inline void set_block(vmath::ivec4 xyz_, BlockType val) { return set_block(xyz_[0], xyz_[1], xyz_[2], val); }

	inline bool all_air() {
		return std::find_if(data, data + size(), [](BlockType b) {return b != BlockType::Air; }) == (data + size());
	}

	inline bool any_air() {
		return std::find(data, data + size(), BlockType::Air) < (data + size());
	}

	inline bool any_translucent() {
		return std::find_if(data, data + size(), [](BlockType b) { return b.is_translucent(); }) < (data + size());
	}

	inline void set_all_air() {
		memset(this->data, (uint8_t)BlockType::Air, sizeof(BlockType) * size());
	}

	inline int count_air() {
		return std::count(data, data + size(), BlockType::Air);
	}

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

	inline void free_data() {
		delete[] data;
	}
};


#endif /* __CHUNKDATA_H__ */
