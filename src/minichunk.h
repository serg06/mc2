#include "block.h"
#include "util.h"

class MiniChunk {
public:
	Block * data; // 16x16x16, indexed same way as chunk
	vmath::ivec3 coords; // coordinates in minichunk format (chunk base x / 16, chunk base y, chunk base z / 16) (NOTE: y NOT DIVIDED BY 16 (YET?))

	// get block at these coordinates, relative to minichunk coords
	inline Block get_block(int x, int y, int z) {
		assert(0 <= x && x < CHUNK_WIDTH && "minichunk get_block invalid x coordinate");
		assert(0 <= y && y < MINICHUNK_HEIGHT && "minichunk get_block invalid y coordinate");
		assert(0 <= z && z < CHUNK_DEPTH && "minichunk get_block invalid z coordinate");

		return data[x + z * CHUNK_WIDTH + y * CHUNK_WIDTH * CHUNK_DEPTH];
	}

	// set block at these coordinates
	inline void set_block(int x, int y, int z, Block val) {
		assert(0 <= x && x < CHUNK_WIDTH && "minichunk set_block invalid x coordinate");
		assert(0 <= y && y < MINICHUNK_HEIGHT && "minichunk set_block invalid y coordinate");
		assert(0 <= z && z < CHUNK_DEPTH && "minichunk set_block invalid z coordinate");

		data[x + z * CHUNK_WIDTH + y * CHUNK_WIDTH * CHUNK_DEPTH] = val;
	}
};
