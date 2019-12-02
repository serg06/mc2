#ifndef __MINICHUNK_H__
#define __MINICHUNK_H__

#include "block.h"
#include "chunkdata.h"
#include "minichunkmesh.h"
#include "render.h"
#include "util.h"

#include <algorithm> // std::find

#define MINICHUNK_WIDTH 16
#define MINICHUNK_HEIGHT 16
#define MINICHUNK_DEPTH 16
#define MINICHUNK_SIZE (MINICHUNK_WIDTH * MINICHUNK_DEPTH * MINICHUNK_HEIGHT)

class MiniChunk : public ChunkData {
public:
	vmath::ivec3 coords; // coordinates in minichunk format (chunk base x / 16, chunk base y, chunk base z / 16) (NOTE: y NOT DIVIDED BY 16 (YET?))
	GLuint block_types_buf; // each mini gets its own buf -- easy this way for now
	GLuint quads_buf; // each mini gets its own buf -- easy this way for now
	bool invisible = false;
	MiniChunkMesh* mesh;

	MiniChunk() : ChunkData(MINICHUNK_WIDTH, MINICHUNK_HEIGHT, MINICHUNK_DEPTH) {

	}
	
	// render this minichunk!
	void render(OpenGLInfo* glInfo) {
		// don't draw if covered in all sides
		if (invisible) {
			return;
		}

		// cube VAO
		glBindVertexArray(glInfo->vao_cube);

		// bind to chunk-types attribute binding point
		glVertexArrayVertexBuffer(glInfo->vao_cube, glInfo->chunk_types_bidx, block_types_buf, 0, sizeof(Block));

		// write this chunk's coordinate to coordinates buffer
		glNamedBufferSubData(glInfo->trans_buf, TRANSFORM_BUFFER_COORDS_OFFSET, sizeof(ivec3), coords); // Add base chunk coordinates to transformation data
		
		// draw!
		glDrawArraysInstanced(GL_TRIANGLES, 0, 36, MINICHUNK_SIZE);

		// unbind VAO jic
		glBindVertexArray(0);
	}

	//// render this minichunk using mesh strategy
	//void render_meshes(OpenGLInfo* glInfo) {
	//	// don't draw if covered in all sides
	//	if (invisible) {
	//		return;
	//	}

	//	//// square VAO
	//	//glBindVertexArray(glInfo->vao_cube);

	//	// rectangle VAO
	//	glBindVertexArray(glInfo->vao_rectangle);

	//	//// bind to chunk-types attribute binding point
	//	//glVertexArrayVertexBuffer(glInfo->vao_cube, glInfo->chunk_types_bidx, block_types_buf, 0, sizeof(Block));

	//	// bind to quads attribute binding point
	//	glVertexArrayVertexBuffer(glInfo->vao_rectangle, glInfo->quads_bidx, quads_buf, 0, sizeof(Quad));

	//	// write this chunk's coordinate to coordinates buffer
	//	// TODO: Figure out why this works. Cuz since we're writing to a uniform buffer with std140, shouldn't this start at a 3-byte boundary?
	//	glNamedBufferSubData(glInfo->trans_buf, TRANSFORM_BUFFER_COORDS_OFFSET, sizeof(ivec3), coords); // Add base chunk coordinates to transformation data

	//	// draw!
	//	glDrawArraysInstanced(GL_TRIANGLES, 0, 36, MINICHUNK_SIZE);

	//	// unbind VAO jic
	//	glBindVertexArray(0);
	//}

	// prepare buf for drawing -- only need to call it when new, or when stuff (or nearby stuff) changes
	void prepare_buf() {
		// TODO
	}

	// get MiniChunk's base coords in real coordinates
	inline vmath::ivec3 real_coords() {
		return { coords[0] * 16, coords[1], coords[2] * 16 };
	}
};

#endif // __MINICHUNK_H__
