#ifndef __MINICHUNK_H__
#define __MINICHUNK_H__

#include "block.h"
#include "chunkdata.h"
#include "minichunkmesh.h"
#include "render.h"
#include "util.h"

#include <algorithm> // std::find
#include <vector> // std::find

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
	MiniChunkMesh* water_mesh;
	GLuint quad_block_type_buf = 0, quad_corner1_buf = 0, quad_corner2_buf = 0, quad_face_buf = 0;
	GLuint water_quad_block_type_buf = 0, water_quad_corner1_buf = 0, water_quad_corner2_buf = 0, water_quad_face_buf = 0;

	MiniChunk() : ChunkData(MINICHUNK_WIDTH, MINICHUNK_HEIGHT, MINICHUNK_DEPTH) {

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
		glNamedBufferSubData(glInfo->trans_buf, TRANSFORM_BUFFER_COORDS_OFFSET, sizeof(ivec4), vec4(coords[0], coords[1], coords[2], 1.0)); // Add base chunk coordinates to transformation data

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

		//char buf[256];
		//sprintf(buf, "Minichunk at (%d, %d, %d) is drawing %d quads3d.\n", coords[0] * 16, coords[1], coords[2] * 16, quads.size());
		//OutputDebugString(buf);

		// quad VAO
		glBindVertexArray(glInfo->vao_quad);

		// write this chunk's coordinate to coordinates buffer
		glNamedBufferSubData(glInfo->trans_buf, TRANSFORM_BUFFER_COORDS_OFFSET, sizeof(ivec3), coords);

		if (quads.size() > 0) {
			// bind to quads attribute binding point
			glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->quad_block_type_bidx, quad_block_type_buf, 0, sizeof(Block));
			glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->q_corner1_bidx, quad_corner1_buf, 0, sizeof(ivec3));
			glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->q_corner2_bidx, quad_corner2_buf, 0, sizeof(ivec3));
			glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->q_face_bidx, quad_face_buf, 0, sizeof(ivec3));

			// DRAW!
			glDrawArraysInstanced(GL_TRIANGLES, 0, 6, quads.size());
		}

		// unbind VAO jic
		glBindVertexArray(0);
	}

	// render this minichunk's water meshes
	void render_water_meshes(OpenGLInfo* glInfo) {
		// don't draw if covered in all sides
		if (invisible) {
			return;
		}

		auto &water_quads = water_mesh->quads3d;

		//char buf[256];
		//sprintf(buf, "Minichunk at (%d, %d, %d) is drawing %d quads3d.\n", coords[0] * 16, coords[1], coords[2] * 16, quads.size());
		//OutputDebugString(buf);

		// quad VAO
		glBindVertexArray(glInfo->vao_quad);

		// write this chunk's coordinate to coordinates buffer
		glNamedBufferSubData(glInfo->trans_buf, TRANSFORM_BUFFER_COORDS_OFFSET, sizeof(ivec3), coords);

		if (water_quads.size() > 0) {
			// bind to quads attribute binding point
			glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->quad_block_type_bidx, water_quad_block_type_buf, 0, sizeof(Block));
			glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->q_corner1_bidx, water_quad_corner1_buf, 0, sizeof(ivec3));
			glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->q_corner2_bidx, water_quad_corner2_buf, 0, sizeof(ivec3));
			glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->q_face_bidx, water_quad_face_buf, 0, sizeof(ivec3));

			// DRAW!
			glDrawArraysInstanced(GL_TRIANGLES, 0, 6, water_quads.size());
		}

		// unbind VAO jic
		glBindVertexArray(0);
	}

	// get MiniChunk's base coords in real coordinates
	inline vmath::ivec3 real_coords() {
		return { coords[0] * 16, coords[1], coords[2] * 16 };
	}

	// get MiniChunk's center coords in real coordinates
	inline vmath::vec3 center_coords_v3() {
		return { coords[0] * 16.0f + 8.0f, coords[1] + 8.0f, coords[2] * 16.0f + 8.0f };
	}

	// get neighboring mini coords
	inline std::vector<vmath::ivec3> neighbors() {
		// n/e/s/w
		std::vector<vmath::ivec3> result = { coords + IEAST, coords + IWEST, coords + INORTH, coords + ISOUTH };

		// up/down
		if (coords[1] < BLOCK_MAX_HEIGHT - MINICHUNK_HEIGHT) {
			result.push_back(coords + IUP);
		}
		if (coords[1] > 0) {
			result.push_back(coords + IDOWN);
		}

		return result;
	}


	void update_quads_buf() {
		if (mesh == nullptr) {
			throw "bad";
		}

		if (water_mesh == nullptr) {
			throw "bad";
		}

		auto &quads = mesh->quads3d;
		auto &water_quads = water_mesh->quads3d;

		Block* blocks = new Block[quads.size()];
		ivec3* corner1s = new ivec3[quads.size()];
		ivec3* corner2s = new ivec3[quads.size()];
		ivec3* faces = new ivec3[quads.size()];

		Block* water_blocks = new Block[water_quads.size()];
		ivec3* water_corner1s = new ivec3[water_quads.size()];
		ivec3* water_corner2s = new ivec3[water_quads.size()];
		ivec3* water_faces = new ivec3[water_quads.size()];

		for (int i = 0; i < quads.size(); i++) {
			// update blocks
			blocks[i] = (Block)quads[i].block;
			faces[i] = quads[i].face;

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

			corner1s[i] = quads[i].corners[0];
			corner2s[i] = quads[i].corners[1];
		}

		for (int i = 0; i < water_quads.size(); i++) {
			// update blocks
			water_blocks[i] = (Block)water_quads[i].block;
			water_faces[i] = water_quads[i].face;

			ivec3 diffs = water_quads[i].corners[1] - water_quads[i].corners[0];

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

			water_corner1s[i] = water_quads[i].corners[0];
			water_corner2s[i] = water_quads[i].corners[1];
		}

		// delete buffers if exist
		glDeleteBuffers(1, &quad_block_type_buf);
		glDeleteBuffers(1, &quad_corner1_buf);
		glDeleteBuffers(1, &quad_corner2_buf);
		glDeleteBuffers(1, &quad_face_buf);

		glDeleteBuffers(1, &water_quad_block_type_buf);
		glDeleteBuffers(1, &water_quad_corner1_buf);
		glDeleteBuffers(1, &water_quad_corner2_buf);
		glDeleteBuffers(1, &water_quad_face_buf);

		// create new ones with just the right sizes
		glCreateBuffers(1, &quad_block_type_buf);
		glCreateBuffers(1, &quad_corner1_buf);
		glCreateBuffers(1, &quad_corner2_buf);
		glCreateBuffers(1, &quad_face_buf);

		glCreateBuffers(1, &water_quad_block_type_buf);
		glCreateBuffers(1, &water_quad_corner1_buf);
		glCreateBuffers(1, &water_quad_corner2_buf);
		glCreateBuffers(1, &water_quad_face_buf);

		// allocate them just enough space
		if (quads.size() > 0) {
			glNamedBufferStorage(quad_block_type_buf, sizeof(Block) * quads.size(), blocks, NULL); // allocate 2 matrices of space for transforms, and allow editing
			glNamedBufferStorage(quad_corner1_buf, sizeof(ivec3) * quads.size(), corner1s, NULL); // allocate 2 matrices of space for transforms, and allow editing
			glNamedBufferStorage(quad_corner2_buf, sizeof(ivec3) * quads.size(), corner2s, NULL); // allocate 2 matrices of space for transforms, and allow editing
			glNamedBufferStorage(quad_face_buf, sizeof(ivec3) * quads.size(), faces, NULL); // allocate 2 matrices of space for transforms, and allow editing
		}

		if (water_quads.size() > 0) {
			glNamedBufferStorage(water_quad_block_type_buf, sizeof(Block) * water_quads.size(), water_blocks, NULL); // allocate 2 matrices of space for transforms, and allow editing
			glNamedBufferStorage(water_quad_corner1_buf, sizeof(ivec3) * water_quads.size(), water_corner1s, NULL); // allocate 2 matrices of space for transforms, and allow editing
			glNamedBufferStorage(water_quad_corner2_buf, sizeof(ivec3) * water_quads.size(), water_corner2s, NULL); // allocate 2 matrices of space for transforms, and allow editing
			glNamedBufferStorage(water_quad_face_buf, sizeof(ivec3) * water_quads.size(), water_faces, NULL); // allocate 2 matrices of space for transforms, and allow editing
		}

		// delete malloc'd stuff
		delete[] blocks, corner1s, corner2s, faces;
		delete[] water_blocks, water_corner1s, water_corner2s, water_faces;
	}

};

#endif // __MINICHUNK_H__
