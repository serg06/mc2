// class for managing chunk data, however we're storing it
#ifndef __CHUNKDATA_H__
#define __CHUNKDATA_H__

#include "block.h"
#include "vmath.h"

// Chunk size
#define BLOCK_MIN_HEIGHT 0
#define BLOCK_MAX_HEIGHT 255
#define MINIS_PER_CHUNK (CHUNK_HEIGHT / MINICHUNK_HEIGHT)

using namespace vmath;

static inline ivec4 clamp_coords_to_world(ivec4 coords) {
	coords[1] = std::clamp(coords[1], 0, BLOCK_MAX_HEIGHT);
	return coords;
}

// Chunk Data is always stored as width wide and depth deep
class ChunkData {
public:
	Block * data;

	int width;
	int height;
	int depth;

	// Memory leak, delete this when un-loading chunk from world.
	ChunkData(int width, int height, int depth) : ChunkData(new Block[width * height * depth], width, height, depth) {}

	ChunkData(Block* data, int width, int height, int depth) : data(data), width(width), height(height), depth(depth) {
		assert(0 < width && "invalid chunk width");
		assert(0 < depth && "invalid chunk depth");
		assert(0 < height && "invalid chunk height");
	}

	inline int size() {
		return width * height * depth;
	}

	// get a ChunkData object pointing to a sub-piece of this chunk's data
	ChunkData* get_piece(unsigned offset, unsigned size) {
		assert(data != nullptr && "data is null");
		assert(size > 0 && "invalid size");
		assert(offset < this->size() && "invalid offset");
		assert(offset + size <= this->size() && "invalid offset/size");

		// chunk data grows in y direction, so size needs to be divisible by width*depth
		assert((size % (width*depth)) == 0 && "invalid size, not divisible by width*depth");

		int new_height = size / (width*depth);

		return new ChunkData(data + offset, width, new_height, depth);
	}

	// get block at these coordinates
	inline Block get_block(const int &x, const int &y, const int &z) {
		assert(0 <= x && x < width && "get_block invalid x coordinate");
		assert(0 <= z && z < depth && "get_block invalid z coordinate");

		// Outside of height range is just air
		if (y < BLOCK_MIN_HEIGHT || y > BLOCK_MAX_HEIGHT) {
			return Block::Air;
		}

		return data[x + z * width + y * width * depth];
	}

	inline Block get_block(const vmath::ivec3 &xyz) { return get_block(xyz[0], xyz[1], xyz[2]); }
	inline Block get_block(const vmath::ivec4 &xyz_) { return get_block(xyz_[0], xyz_[1], xyz_[2]); }

	// set block at these coordinates
	inline void set_block(int x, int y, int z, Block val) {
		assert(0 <= x && x < width && "set_block invalid x coordinate");
		assert(0 <= y && y < height && "set_block invalid y coordinate");
		assert(0 <= z && z < depth && "set_block invalid z coordinate");

		data[x + z * width + y * width * depth] = val;
	}

	inline void set_block(vmath::ivec3 xyz, Block val) { return set_block(xyz[0], xyz[1], xyz[2], val); }
	inline void set_block(vmath::ivec4 xyz_, Block val) { return set_block(xyz_[0], xyz_[1], xyz_[2], val); }

	inline bool all_air() {
		return std::find_if(data, data + size(), [](Block b) {return b != Block::Air; }) == (data + size());
	}

	inline bool any_air() {
		return std::find(data, data + size(), Block::Air) < (data + size());
	}

	inline bool any_translucent() {
		return std::find_if(data, data + size(), [](Block b) { return b.is_translucent(); }) < (data + size());
	}

	inline void set_all_air() {
		memset(this->data, (uint8_t)Block::Air, sizeof(Block) * size());
	}

	inline int count_air() {
		return std::count(data, data + size(), Block::Air);
	}

	inline char* print_layer(int layer) {
		assert(layer < height && "cannot print this layer, too high");

		char* result = (char*)malloc(sizeof(char) * 16 * 16 * 8); // up to 8 chars per block type
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
