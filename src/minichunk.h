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
	bool invisible = false;
	MiniChunkMesh* mesh;
	GLuint quad_block_type_buf, quad_corner_buf, quad_corner1_buf, quad_corner2_buf;

	MiniChunk() : ChunkData(MINICHUNK_WIDTH, MINICHUNK_HEIGHT, MINICHUNK_DEPTH) {
		// Fix a bug when rendering too quickly.
		glCreateBuffers(1, &quad_block_type_buf);
		glCreateBuffers(1, &quad_corner_buf);

		glCreateBuffers(1, &quad_corner1_buf);
		glCreateBuffers(1, &quad_corner2_buf);
	}

	// render this minichunk's cubes (that's 4096 cubes, with 12 triangles per cube.)
	void render_cubes(OpenGLInfo* glInfo) {
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

	// render this minichunk's meshes
	void render_meshes(OpenGLInfo* glInfo) {
		// don't draw if covered in all sides
		if (invisible) {
			return;
		}

		auto &quads = mesh->quads3d;

		char buf[256];
		//sprintf(buf, "Minichunk at (%d, %d, %d) is drawing %d quads3d.\n", coords[0] * 16, coords[1], coords[2] * 16, quads.size());
		//OutputDebugString(buf);

		// quad VAO
		glBindVertexArray(glInfo->vao_quad);

		// bind to quads attribute binding point
		glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->quad_block_type_bidx, quad_block_type_buf, 0, sizeof(Block));
		glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->quad_corner_bidx, quad_corner_buf, 0, sizeof(ivec3));

		glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->q_corner1_bidx, quad_corner1_buf, 0, sizeof(ivec3));
		glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->q_corner2_bidx, quad_corner2_buf, 0, sizeof(ivec3));

		// write this chunk's coordinate to coordinates buffer
		glNamedBufferSubData(glInfo->trans_buf, TRANSFORM_BUFFER_COORDS_OFFSET, sizeof(ivec3), coords);

		// DRAW!
		glDrawArraysInstanced(GL_TRIANGLES, 0, 6, quads.size());

		// unbind VAO jic
		glBindVertexArray(0);
	}

	// prepare buf for drawing -- only need to call it when new, or when stuff (or nearby stuff) changes
	void prepare_buf() {
		// TODO
	}

	// get MiniChunk's base coords in real coordinates
	inline vmath::ivec3 real_coords() {
		return { coords[0] * 16, coords[1], coords[2] * 16 };
	}

	void update_quads_buf() {
		if (mesh == nullptr) {
			throw "bad";
		}

		auto &quads = mesh->quads3d;

		ivec3* corners = (ivec3*)malloc(sizeof(ivec3) * quads.size() * 6);
		Block* blocks = (Block*)malloc(sizeof(Block) * quads.size());

		ivec3* corner1s = (ivec3*)malloc(sizeof(ivec3) * quads.size());
		ivec3* corner2s = (ivec3*)malloc(sizeof(ivec3) * quads.size());

		for (int i = 0; i < quads.size(); i++) {
			// update blocks
			blocks[i] = (Block)quads[i].block;

			ivec3 diffs = quads[i].corners[1] - quads[i].corners[0];

			// make sure at least one dimension is killed - i.e, it's a flat quad ( todo. make sure other 2 dimensions are >= 1 size.)
			int num_diffs_0 = 0;
			int zero_idx = 0;

			for (int i = 0; i < 3; i++) {
				if (diffs[i] == 0) {
					num_diffs_0 += 1;
					zero_idx = i;
				}
			}

			assert(num_diffs_0 == 1 && "Invalid quad dimensions.");

			// working indices are always gonna be xy, xz, or yz.
			int working_idx_1, working_idx_2;
			gen_working_indices(zero_idx, working_idx_1, working_idx_2);

			char buf[256];

			// update corners
			for (int j = 0; j < 6; j++) {
				switch (j) {
				case 0:
					// top-left corner
					corners[i * 6 + j] = quads[i].corners[0];
					//sprintf(buf, "Set corners[%d] to (%d, %d, %d)\n", i * 6 + j, quads[i].corners[0][0], quads[i].corners[0][1], quads[i].corners[0][2]);
					//OutputDebugString(buf);
					break;
				case 1:
				case 4:
					// bottom-left corner
					corners[i * 6 + j] = quads[i].corners[0];
					corners[i * 6 + j][working_idx_1] += diffs[working_idx_1];
					break;
				case 2:
				case 3:
					// top-right corner
					corners[i * 6 + j] = quads[i].corners[0];
					corners[i * 6 + j][working_idx_2] += diffs[working_idx_2];
					break;
				case 5:
					// bottom-right corner
					corners[i * 6 + j] = quads[i].corners[1];
					break;
				}
			}

			corner1s[i] = quads[i].corners[0];
			corner2s[i] = quads[i].corners[1];
		}

		// delete buffers if exist
		glDeleteBuffers(1, &quad_block_type_buf);
		glDeleteBuffers(1, &quad_corner_buf);

		glDeleteBuffers(1, &quad_corner1_buf);
		glDeleteBuffers(1, &quad_corner2_buf);

		// create new ones with just the right sizes
		glCreateBuffers(1, &quad_block_type_buf);
		glCreateBuffers(1, &quad_corner_buf);

		glCreateBuffers(1, &quad_corner1_buf);
		glCreateBuffers(1, &quad_corner2_buf);

		// allocate them just enough space
		glNamedBufferStorage(quad_block_type_buf, sizeof(Block) * quads.size(), NULL, GL_DYNAMIC_STORAGE_BIT); // allocate 2 matrices of space for transforms, and allow editing
		glNamedBufferStorage(quad_corner_buf, sizeof(ivec3) * quads.size() * 6, NULL, GL_DYNAMIC_STORAGE_BIT); // allocate 2 matrices of space for transforms, and allow editing

		glNamedBufferStorage(quad_corner1_buf, sizeof(ivec3) * quads.size(), NULL, GL_DYNAMIC_STORAGE_BIT); // allocate 2 matrices of space for transforms, and allow editing
		glNamedBufferStorage(quad_corner2_buf, sizeof(ivec3) * quads.size(), NULL, GL_DYNAMIC_STORAGE_BIT); // allocate 2 matrices of space for transforms, and allow editing

		// fill 'em up!
		glNamedBufferSubData(quad_block_type_buf, 0, sizeof(Block) * quads.size(), blocks);
		glNamedBufferSubData(quad_corner_buf, 0, sizeof(ivec3) * quads.size() * 6, corners);

		glNamedBufferSubData(quad_corner1_buf, 0, sizeof(ivec3) * quads.size(), corner1s);
		glNamedBufferSubData(quad_corner2_buf, 0, sizeof(ivec3) * quads.size(), corner2s);

		OutputDebugString("");
	}
};

#endif // __MINICHUNK_H__
