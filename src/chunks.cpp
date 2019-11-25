#include "chunks.h"
#include "GL/gl3w.h"
#include <stdlib.h>
#include <stdio.h>



// generate us a nice lil chunk
Block* gen_chunk() {
	Block* result = (Block*)calloc(sizeof(uint8_t) * CHUNK_SIZE, sizeof(uint8_t));
	char buf[256];

	for (int y = 0; y < CHUNK_HEIGHT; y++) {
		for (int x = 0; x < CHUNK_WIDTH; x++) {
			for (int z = 0; z < CHUNK_DEPTH; z++) {
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

	return result;
};
