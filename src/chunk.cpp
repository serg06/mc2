#include "chunk.h"
#include "chunkdata.h"
#include "FastNoise.h"
#include "util.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

constexpr int WATER_HEIGHT = 64;

static std::unique_ptr<BlockType[]> __chunk_tmp_storage = std::make_unique<BlockType[]>(CHUNK_SIZE);

using namespace std;
using namespace vmath;

static float softmax(float v, float minv, float maxv) {
	assert(maxv > minv);
	assert(minv <= v && v <= maxv);

	float dist = maxv - minv;
	float dist_rad = dist / 2.0f;
	float orig = v;

	// center everything
	v -= minv;
	v -= dist_rad;

	// squeeze from -1 to 1
	v /= dist_rad;

	int strength = 3;
	for (int i = 0; i < strength; i++) {
		// apply softmax
		v *= abs(v);
	}

	// stretch back out
	v *= dist_rad;

	// recenter at original point
	v += dist_rad;
	v += minv;

	return v;
}

static std::vector<vmath::ivec2> surrounding_chunks_s(const vmath::ivec2& chunk_coord) {
	return {
		// sides
		chunk_coord + vmath::ivec2(1, 0),
		chunk_coord + vmath::ivec2(0, 1),
		chunk_coord + vmath::ivec2(-1, 0),
		chunk_coord + vmath::ivec2(0, -1),

		// corners
		chunk_coord + vmath::ivec2(1, 1),
		chunk_coord + vmath::ivec2(-1, 1),
		chunk_coord + vmath::ivec2(-1, -1),
		chunk_coord + vmath::ivec2(1, -1),
	};
}

// get surrounding chunks, but only the ones on the sides
static std::vector<vmath::ivec2> surrounding_chunks_sides_s(const vmath::ivec2& chunk_coord) {
	return {
		// sides
		chunk_coord + vmath::ivec2(1, 0),
		chunk_coord + vmath::ivec2(0, 1),
		chunk_coord + vmath::ivec2(-1, 0),
		chunk_coord + vmath::ivec2(0, -1),
	};
}

// convert coordinates to idx
static int c2idx_chunk(const int& x, const int& y, const int& z) {
	return x + z * CHUNK_WIDTH + y * CHUNK_WIDTH * CHUNK_DEPTH;
}
static int c2idx_chunk(const vmath::ivec3& xyz) { return c2idx_chunk(xyz[0], xyz[1], xyz[2]); }


/* Chunk */


Chunk::Chunk() : Chunk({ 0, 0 }) {}
Chunk::Chunk(const vmath::ivec2& coords) : coords(coords) {}

// initialize minichunks by setting coords and allocating space
void Chunk::init_minichunks() {
	for (int i = 0; i < MINIS_PER_CHUNK; i++) {
		// create mini and populate it
		minis[i] = std::make_shared<MiniChunk>();
		minis[i]->set_coords({ coords[0], i * MINICHUNK_HEIGHT, coords[1] });
		minis[i]->allocate();
		minis[i]->set_all_air();
	}
}

std::shared_ptr<MiniChunk> Chunk::get_mini_with_y_level(const int y) {
	return 0 <= y && y <= 255 ? minis[y / 16] : nullptr;
}

void Chunk::set_mini_with_y_level(const int y, std::shared_ptr<MiniChunk> mini) {
	if (0 <= y && y <= 255)
	{
		minis[y / 16] = mini;
	}
}

// get block at these coordinates
BlockType Chunk::get_block(const int& x, const int& y, const int& z) {
	return get_mini_with_y_level(y) == nullptr ? BlockType::Air : get_mini_with_y_level(y)->get_block(x, y % MINICHUNK_HEIGHT, z);
}

BlockType Chunk::get_block(const vmath::ivec3& xyz) { return get_block(xyz[0], xyz[1], xyz[2]); }
BlockType Chunk::get_block(const vmath::ivec4& xyz_) { return get_block(xyz_[0], xyz_[1], xyz_[2]); }

// set blocks in map using array, efficiently
void Chunk::set_blocks(BlockType* new_blocks) {
	for (int y = 0; y < BLOCK_MAX_HEIGHT; y += MINICHUNK_HEIGHT) {
		std::shared_ptr<MiniChunk> mini = get_mini_with_y_level(y);

		// If someone else has a copy, make a copy before updating
		bool copy = mini.use_count() > 1;
		if (copy)
		{
			mini = std::make_shared<MiniChunk>(*mini);
		}

		mini->set_blocks(new_blocks + MINICHUNK_WIDTH * MINICHUNK_DEPTH * y);

		if (copy)
		{
			set_mini_with_y_level(y, mini);
		}
	}
}

// set block at these coordinates
// TODO: create a set_block_range that takes a min_xyz and max_xyz and efficiently set them.
void Chunk::set_block(int x, int y, int z, const BlockType& val) {
	std::shared_ptr<MiniChunk> mini = get_mini_with_y_level(y);

	// If someone else has a copy, make a copy before updating
	bool copy = mini.use_count() > 1;
	if (copy)
	{
		mini = std::make_shared<MiniChunk>(*mini);
	}

	mini->set_block(x, y % MINICHUNK_HEIGHT, z, val);

	if (copy)
	{
		set_mini_with_y_level(y, mini);
	}
}

void Chunk::set_block(const vmath::ivec3& xyz, const BlockType& val) { return set_block(xyz[0], xyz[1], xyz[2], val); }
void Chunk::set_block(const vmath::ivec4& xyz_, const BlockType& val) { return set_block(xyz_[0], xyz_[1], xyz_[2], val); }

// get metadata at these coordinates
Metadata Chunk::get_metadata(const int& x, const int& y, const int& z) {
	return get_mini_with_y_level(y)->get_metadata(x, y % MINICHUNK_HEIGHT, z);
}

Metadata Chunk::get_metadata(const vmath::ivec3& xyz) { return get_metadata(xyz[0], xyz[1], xyz[2]); }
Metadata Chunk::get_metadata(const vmath::ivec4& xyz_) { return get_metadata(xyz_[0], xyz_[1], xyz_[2]); }

// set metadata at these coordinates
void Chunk::set_metadata(const int x, const int y, const int z, const Metadata& val) {
	get_mini_with_y_level(y)->set_metadata(x, y % MINICHUNK_HEIGHT, z, val);
}

void Chunk::set_metadata(const vmath::ivec3& xyz, const Metadata& val) { return set_metadata(xyz[0], xyz[1], xyz[2], val); }
void Chunk::set_metadata(const vmath::ivec4& xyz_, const Metadata& val) { return set_metadata(xyz_[0], xyz_[1], xyz_[2], val); }

void Chunk::clear() {
	for (auto& mini : minis) {
		mini.reset();
	}
}

std::vector<vmath::ivec2> Chunk::surrounding_chunks() const {
	return surrounding_chunks_s(coords);
}

std::vector<vmath::ivec2> Chunk::surrounding_chunks_sides() const {
	return surrounding_chunks_sides_s(coords);
}

void Chunk::generate() {
	// generate this chunk
	// NOTE: traverse x, then z, then y, whenever possible
	FastNoise fn;

	// create chunk
	init_minichunks();

#ifdef _DEBUG
	if (coords == vmath::ivec2(0, 0)) {
		OutputDebugString("Warning: Generating chunk for {0, 0}.\n");
	}
#endif

	// create chunk data array
	memset(__chunk_tmp_storage.get(), 0, CHUNK_SIZE * sizeof(BlockType));

	// fill data
	for (int z = 0; z < CHUNK_DEPTH; z++) {
		for (int x = 0; x < CHUNK_WIDTH; x++) {
			// get height at this location
			double y = fn.GetSimplex((FN_DECIMAL)(x + coords[0] * 16) / 2.0, (FN_DECIMAL)(z + coords[1] * 16) / 2.0);
			y += fn.GetPerlin((FN_DECIMAL)(x + coords[0] * 16) / 2.0, (FN_DECIMAL)(z + coords[1] * 16) / 2.0);
			y += fn.GetCellular((FN_DECIMAL)(x + coords[0] * 16) / 2.0, (FN_DECIMAL)(z + coords[1] * 16) / 2.0) / 2.0;
			y /= 2.5;

			y = (y + 1.0) / 2.0; // normalize to [0.0, 1.0]
			y *= 64; // variation of around 32
			y += 38; // minimum height 40

			// fill everything under that height
			for (int i = 0; i < y; i++) {
				__chunk_tmp_storage[c2idx_chunk(x, i, z)] = BlockType::Stone;
			}
			__chunk_tmp_storage[c2idx_chunk(x, (int)floor(y), z)] = BlockType::Grass;

			// generate tree if we wanna
			if (y >= WATER_HEIGHT) {
				float w = fn.GetWhiteNoise((FN_DECIMAL)(x + coords[0] * 16), (FN_DECIMAL)(z + coords[1] * 16));
				w = (w + 1.0) / 2.0; // normalize random value to [0.0, 1.0]
				// 1/256 chance to make tree
				if (w <= (1.0f / 256.0f)) {
					// generate leaves
					for (int dy = 4; dy <= 5; dy++) {
						for (int dz = -2; dz <= 2; dz++) {
							for (int dx = -2; dx <= 2; dx++) {
								if (x + dx < 0 || x + dx >= 16 || z + dz < 0 || z + dz >= 16) {
									continue;
								}
								__chunk_tmp_storage[c2idx_chunk(x + dx, y + dy, z + dz)] = BlockType::OakLeaves;
							}
						}
					}
					for (int dy = 6; dy <= 6; dy++) {
						for (int dz = -1; dz <= 1; dz++) {
							for (int dx = -1; dx <= 1; dx++) {
								if (x + dx < 0 || x + dx >= 16 || z + dz < 0 || z + dz >= 16) {
									continue;
								}
								__chunk_tmp_storage[c2idx_chunk(x + dx, y + dy, z + dz)] = BlockType::OakLeaves;
							}
						}
					}
					for (int dy = 7; dy <= 7; dy++) {
						for (int dx = -1; dx <= 1; dx++) {
							for (int dz = abs(dx) - 1; dz <= 1 - abs(dx); dz++) {
								if (x + dx < 0 || x + dx >= 16 || z + dz < 0 || z + dz >= 16) {
									continue;
								}
								__chunk_tmp_storage[c2idx_chunk(x + dx, y + dy, z + dz)] = BlockType::OakLeaves;
							}
						}
					}

					// generate logs
					for (int dy = 1; dy <= 5; dy++) {
						__chunk_tmp_storage[c2idx_chunk(x, y + dy, z)] = BlockType::OakWood;
					}
				}
			}

			// Fill water
			if (y < WATER_HEIGHT - 1) {
				for (int y2 = y + 1; y2 < WATER_HEIGHT; y2++) {
					__chunk_tmp_storage[c2idx_chunk(x, y2, z)] = BlockType::StillWater;
				}
			}
		}
	}

	set_blocks(__chunk_tmp_storage.get());

#ifdef _DEBUG
	OutputDebugString("");
#endif
}
