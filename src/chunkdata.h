// class for managing chunk data, however we're storing it
#ifndef __CHUNKDATA_H__
#define __CHUNKDATA_H__

#include "block.h"
#include "util.h"

#include "cmake_pch.hxx"

#include <iterator>

// Chunk size
constexpr int BLOCK_MIN_HEIGHT = 0;
constexpr int BLOCK_MAX_HEIGHT = 255;
constexpr int MINIS_PER_CHUNK = 16; // TODO: = (CHUNK_HEIGHT / MINICHUNK_HEIGHT);

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
	inline Metadata() : data(0) {} // TODO: don't set 0?
	inline Metadata(uint8_t data) : data(data) {}

	/* LIQUIDS | LAST 4 BITS | LIQUID LEVEL */

	inline uint8_t get_liquid_level() const {
		return data & 0xF;
	}

	inline void set_liquid_level(const uint8_t water_level) {
		assert(water_level == (water_level & 0xF) && "water level uses more than 4 bits");
		data = (data & 0xF0) | (water_level & 0xF);
	}

	// comparing to Metadata
	inline bool operator==(const Metadata& m) const { return data == m.data; }
	inline bool operator!=(const Metadata& m) const { return data != m.data; }
};

// block lighting
struct Lighting {
private:
	uint8_t data;

public:
	inline uint8_t get_sunlight() const {
		return data >> 4;
	}

	inline void set_sunlight(const uint8_t sunlight) {
		assert(sunlight == (sunlight & 0xF) && "sunlight level uses more than 4 bits");
		data = (sunlight << 4) | (data & 0xF);
	}

	inline uint8_t get_torchlight() const {
		return data & 0xF;
	}

	inline void set_torchlight(const uint8_t torchlight) {
		assert(torchlight == (torchlight & 0xF) && "torchlight level uses more than 4 bits");
		data = (data & 0xF0) | torchlight;
	}

	// comparing to Lighting
	inline bool operator==(const Lighting& l) const { return data == l.data; }
	inline bool operator!=(const Lighting& l) const { return data != l.data; }
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
	ChunkData(const int width, const int height, const int depth) : width(width), height(height), depth(depth) {
		assert(0 < width && "invalid chunk width");
		assert(0 < depth && "invalid chunk depth");
		assert(0 < height && "invalid chunk height");
	}

	inline void allocate() {
		blocks.clear(BlockType::Air);
		metadatas.clear(0);
		lightings.clear(0);
	}

	// TODO: replace allocate() and free() with just reset()
	inline void free() {
		blocks.clear(BlockType::Air);
		metadatas.clear(0);
		lightings.clear(0);
	}

	inline int size() const {
		return width * height * depth;
	}

	// convert coordinates to idx
	constexpr inline int c2idx(const int &x, const int &y, const int &z) const {
		return x + z * width + y * width * depth;
	}
	constexpr inline int c2idx(const vmath::ivec3 &xyz) const { return c2idx(xyz[0], xyz[1], xyz[2]); }


	// get block at these coordinates
	inline BlockType get_block(const int &x, const int &y, const int &z) const {
		assert(0 <= x && x < width && "get_block invalid x coordinate");
		assert(0 <= z && z < depth && "get_block invalid z coordinate");

		// Outside of height range is just air
		if (y < BLOCK_MIN_HEIGHT || y > BLOCK_MAX_HEIGHT) {
			return BlockType::Air;
		}

		return blocks[c2idx(x, y, z)];
	}

	inline BlockType get_block(const vmath::ivec3 &xyz) const { return get_block(xyz[0], xyz[1], xyz[2]); }
	inline BlockType get_block(const vmath::ivec4 &xyz_) const { return get_block(xyz_[0], xyz_[1], xyz_[2]); }

	// set block at these coordinates
	inline void set_block(const int x, const int y, const int z, const BlockType &val) {
		assert(0 <= x && x < width && "set_block invalid x coordinate");
		assert(0 <= y && y < height && "set_block invalid y coordinate");
		assert(0 <= z && z < depth && "set_block invalid z coordinate");

		blocks.set_interval(c2idx(x, y, z), c2idx(x, y, z) + 1, val);
	}

	inline void set_block(const vmath::ivec3 &xyz, const BlockType &val) { return set_block(xyz[0], xyz[1], xyz[2], val); }
	inline void set_block(const vmath::ivec4 &xyz_, const BlockType &val) { return set_block(xyz_[0], xyz_[1], xyz_[2], val); }

	// set blocks in map using array, efficiently
	// relies on x -> z -> y
	inline void set_blocks(BlockType* new_blocks) {
		blocks.clear(BlockType::Air);

		int start = 0;
		BlockType start_block = new_blocks[0];

		for (int y = 0; y < width; y++) {
			for (int z = 0; z < depth; z++) {
				for (int x = 0; x < height; x++) {
					// if different block
					if (new_blocks[c2idx(x, y, z)] != start_block) {
						// add interval
						blocks.set_interval(start, c2idx(x, y, z), start_block);

						// new start
						start = c2idx(x, y, z);
						start_block = new_blocks[c2idx(x, y, z)];
					}
				}
			}
		}

		// add last interval
		blocks.set_interval(start, width*depth*height, start_block);
	}

	/**
	 * Given a cube of chunkdata coordinates, convert it into optimal intervals.
	 * NOTE: Relies on the fact that we go in the order x, z, y.
	 */
	inline std::vector<std::pair<int, int>> optimize_intervals(const vmath::ivec3 &min_xyz, const vmath::ivec3 &max_xyz) {
		// TODO: Make sure it works. Right now I think there's an off-by-one error. (Should be doing <= instead of <.)
		assert(min_xyz[0] < max_xyz[0] && min_xyz[1] < max_xyz[1] && min_xyz[2] < max_xyz[2]);

		std::vector<std::pair<int, int>> result;

		const vmath::ivec3 diffs = max_xyz - min_xyz;

		// if x is 16
		if (diffs[0] == 16) {
			// if z is 16, only need one interval
			if (diffs[2] == 16) {
				result.push_back({ c2idx(min_xyz), c2idx(max_xyz) });
			}
			// x is 16 but z is not 16
			else {
				for (int y = min_xyz[1]; y < max_xyz[1]; y++) {
					result.push_back({
						c2idx(min_xyz[0], y, min_xyz[2]),
						c2idx(max_xyz[0], y, max_xyz[2])
						});
				}
			}
		}
		// neither x nor z are 16
		else {
			for (int y = min_xyz[1]; y < max_xyz[1]; y++) {
				for (int z = min_xyz[2]; z < max_xyz[2]; z++) {
					result.push_back({
						c2idx(min_xyz[0], y, z),
						c2idx(max_xyz[0], y, z)
						});
				}
			}
		}

		return result;
	}

	inline bool all_air() {
		return blocks[0] == BlockType::Air && blocks.num_intervals() == 1;
	}

	inline bool any_air() {
		for (auto iter = blocks.get_interval(0); iter != blocks.end(); iter++) {
			if (iter->first < width*depth*height && iter->second == BlockType::Air) {
				return true;
			}
		}
		//auto blocks_end = std::next(blocks.get_interval(MINICHUNK_SIZE - 1));
		//return std::find(blocks.begin(), blocks_end, BlockType::Air) != blocks_end;
	}

	inline bool any_translucent() {
		for (auto iter = this->blocks.get_interval(0); iter != blocks.end(); iter++) {
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

	// get metadata at these coordinates
	inline Metadata get_metadata(const int &x, const int &y, const int &z) const {
		assert(0 <= x && x < width && "get_metadata invalid x coordinate");
		assert(0 <= z && z < depth && "get_metadata invalid z coordinate");

		// nothing outside of height range
		if (y < BLOCK_MIN_HEIGHT || y > BLOCK_MAX_HEIGHT) {
			return 0;
		}

		return metadatas[c2idx(x, y, z)];
	}

	inline Metadata get_metadata(const vmath::ivec3 &xyz) const { return get_metadata(xyz[0], xyz[1], xyz[2]); }
	inline Metadata get_metadata(const vmath::ivec4 &xyz_) const { return get_metadata(xyz_[0], xyz_[1], xyz_[2]); }

	// set metadata at these coordinates
	inline void set_metadata(const int x, const int y, const int z, const Metadata &val) {
		assert(0 <= x && x < width && "set_metadata invalid x coordinate");
		assert(0 <= y && y < height && "set_metadata invalid y coordinate");
		assert(0 <= z && z < depth && "set_metadata invalid z coordinate");

		metadatas.set_interval(c2idx(x, y, z), c2idx(x, y, z) + 1, val);
	}

	inline void set_metadata(const vmath::ivec3 &xyz, Metadata &val) { return set_metadata(xyz[0], xyz[1], xyz[2], val); }
	inline void set_metadata(const vmath::ivec4 &xyz_, Metadata &val) { return set_metadata(xyz_[0], xyz_[1], xyz_[2], val); }

	inline auto print_y_layer(const int layer) {
		assert(layer < height && "cannot print this layer, too high");

		auto result = std::make_unique<char[]>(16 * 16 * 8);
		char* tmp = result.get();

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
