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
	GLuint quad_block_type_buf, quad_corner_buf;

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

	void render_meshes_simple2(OpenGLInfo* glInfo) {
		// don't draw if covered in all sides
		if (invisible) {
			return;
		}

		auto &quads = mesh->quads3d;

		char buf[256];
		sprintf(buf, "Minichunk at (%d, %d, %d) is drawing %d quads3d.\n", coords[0] * 16, coords[1], coords[2] * 16, quads.size());
		OutputDebugString(buf);

		// quad VAO
		glBindVertexArray(glInfo->vao_quad);

		// bind to quads attribute binding point
		glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->quad_block_type_bidx, quad_block_type_buf, 0, sizeof(Block));
		glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->quad_corner_bidx, quad_corner_buf, 0, sizeof(ivec3));

		// write this chunk's coordinate to coordinates buffer
		glNamedBufferSubData(glInfo->trans_buf, TRANSFORM_BUFFER_COORDS_OFFSET, sizeof(ivec3), coords);

		// DRAW!
		//glDrawArraysInstanced(GL_TRIANGLES, 0, 6, quads.size());
		//glDrawArraysInstanced(GL_TRIANGLES, 0, 6, 1);
		//glDrawArraysInstanced(GL_TRIANGLES, 0, 3, 1);
		//glDrawArraysInstanced(GL_TRIANGLES, 0, 12, 1);
		//glDrawArraysInstanced(GL_TRIANGLES, 0, 6*96, 1);
		for (int i = 0; i < quads.size(); i++) {
			glDrawArraysInstancedBaseInstance(GL_TRIANGLES, i * 6, 6, 1, i);
		}

		Block* blocks = (Block*)malloc(sizeof(Block) * quads.size());

		// get blocks from buffer
		glGetNamedBufferSubData(quad_block_type_buf, 0, quads.size(), blocks);

		//OutputDebugString("Buffer's types:\n\t");
		for (int i = 0; i < quads.size(); i++) {
			sprintf(buf, "\t[%d]: buffer says %d, quad says %d.\n", i, (uint8_t)blocks[i], (uint8_t)quads[i].block);
			//OutputDebugString(buf);
			if (blocks[i] != (Block) quads[i].block) {
				throw "wew";
			}
			if (blocks[i] == Block::Air) {
				throw "wew2";
			}
		}

		free(blocks);

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
					sprintf(buf, "Set corners[%d] to (%d, %d, %d)\n", i * 6 + j, quads[i].corners[0][0], quads[i].corners[0][1], quads[i].corners[0][2]);
					OutputDebugString(buf);
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
		}

		glNamedBufferSubData(quad_block_type_buf, 0, sizeof(Block) * quads.size(), blocks);
		glNamedBufferSubData(quad_corner_buf, 0, sizeof(ivec3) * quads.size() * 6, corners);
		OutputDebugString("");
	}
};

#endif // __MINICHUNK_H__
