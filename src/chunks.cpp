#include "chunks.h"
#include <stdlib.h>

// generate us a nice lil chunk
static Block* gen_chunk() {
	Block* result = (Block*)calloc(sizeof(uint8_t) * CHUNK_SIZE, sizeof(uint8_t));

	for (uint8_t y = 0; y < CHUNK_HEIGHT; y++) {
		for (uint8_t x = 0; x < CHUNK_WIDTH; x++) {
			for (uint8_t z = 0; z < CHUNK_DEPTH; z++) {
				// 62 or lower = stone
				if (y <= 62) {
					chunk_set(result, x, y, z, Block::Stone);
				}
				
				// 63 to 64 = grass
				else if (y <= 64) {
					chunk_set(result, x, y, z, Block::Grass);
				}

				// 65 and higher = air
				else {
					chunk_set(result, x, y, z, Block::Air);
				}
			}
		}
	}
};
