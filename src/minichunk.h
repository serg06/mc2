#ifndef __MINICHUNK_H__
#define __MINICHUNK_H__

#include "block.h"
#include "render.h"
#include "util.h"

class MiniChunk;

class MiniChunk {
public:
	Block * data; // 16x16x16, indexed same way as chunk
	vmath::ivec3 coords; // coordinates in minichunk format (chunk base x / 16, chunk base y, chunk base z / 16) (NOTE: y NOT DIVIDED BY 16 (YET?))
	GLuint gl_buf; // each mini gets its own buf -- easy this way for now
	bool covered_on_all_sides = false;
	
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

	// render this minichunk!
	void render(OpenGLInfo* glInfo) {
		// don't draw if covered in all sides
		if (covered_on_all_sides) {
			return;
		}

		// cube VAO
		glBindVertexArray(glInfo->vao_cube);

		// bind to chunk-types attribute binding point
		glVertexArrayVertexBuffer(glInfo->vao_cube, glInfo->chunk_types_bidx, gl_buf, 0, sizeof(Block));

		// write this chunk's coordinate to coordinates buffer
		glNamedBufferSubData(glInfo->trans_buf, TRANSFORM_BUFFER_COORDS_OFFSET, sizeof(ivec3), coords); // Add base chunk coordinates to transformation data
		
		// draw!
		glDrawArraysInstanced(GL_TRIANGLES, 0, 36, MINICHUNK_SIZE);
	}

	// prepare buf for drawing -- only need to call it when new, or when stuff (or nearby stuff) changes
	void prepare_buf() {
		// TODO
	}
};

#endif // __MINICHUNK_H__
