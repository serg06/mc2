#include "chunk.h"
#include "chunkdata.h"
#include "FastNoise.h"
#include "util.h"

#include <assert.h>
#include <cstdlib>
#include <initializer_list>
#include <stdio.h>
#include <stdlib.h>

constexpr int WATER_HEIGHT = 64;

using namespace std;
using namespace vmath;

static inline float softmax(float v, float minv, float maxv) {
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

void Chunk::generate() {
	// generate this chunk
	// NOTE: traverse x, then z, then y, whenever possible
	FastNoise fn;

	// create chunk
	init_minichunks();

#ifdef _DEBUG
	if (coords == ivec2(0, 0)) {
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
