#ifndef __MINICHUNK_H__
#define __MINICHUNK_H__

#include "block.h"
#include "chunkdata.h"
#include "minichunkmesh.h"
#include "render.h"
#include "util.h"

#include <algorithm> // std::find
#include <mutex>
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
	MiniChunkMesh* mesh = nullptr;
	MiniChunkMesh* water_mesh = nullptr;
	bool meshes_updated = false;
	// TODO: When someone else sets invisibility, we want to delete bufs as well.
	GLuint quad_block_type_buf = 0, quad_corner1_buf = 0, quad_corner2_buf = 0, quad_face_buf = 0;

	// number of quads inside the buffer, as reading from mesh is not always reliable
	GLuint num_nonwater_quads = 0;
	GLuint num_water_quads = 0;

	std::mutex mesh_lock;

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
		if (invisible || mesh == nullptr) {
			return;
		}

		// update quads if needed
		mesh_lock.lock();
		if (meshes_updated) {
			meshes_updated = false;
			update_quads_buf();
		}
		mesh_lock.unlock();

		auto &quads = mesh->quads3d;

		if (quads.size() == 0) {
			return;
		}

		//char buf[256];
		//sprintf(buf, "Minichunk at (%d, %d, %d) is drawing %d quads3d.\n", coords[0] * 16, coords[1], coords[2] * 16, quads.size());
		//OutputDebugString(buf);

		// quad VAO
		glBindVertexArray(glInfo->vao_quad);

		// write this chunk's coordinate to coordinates buffer
		glNamedBufferSubData(glInfo->trans_buf, TRANSFORM_BUFFER_COORDS_OFFSET, sizeof(ivec3), coords);

		// bind to quads attribute binding point
		glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->quad_block_type_bidx, quad_block_type_buf, 0, sizeof(Block));
		glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->q_corner1_bidx, quad_corner1_buf, 0, sizeof(ivec3));
		glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->q_corner2_bidx, quad_corner2_buf, 0, sizeof(ivec3));
		glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->q_face_bidx, quad_face_buf, 0, sizeof(ivec3));

		// DRAW!
		glDrawArraysInstanced(GL_TRIANGLES, 0, 6, quads.size());

		// unbind VAO jic
		glBindVertexArray(0);
	}

	// render this minichunk's water meshes
	void render_water_meshes(OpenGLInfo* glInfo) {
		// don't draw if covered in all sides
		if (invisible || water_mesh == nullptr) {
			return;
		}

		// update quads if needed
		mesh_lock.lock();
		if (meshes_updated) {
			meshes_updated = false;
			update_quads_buf();
		}
		mesh_lock.unlock();
		
		auto &quads = mesh->quads3d;
		auto &water_quads = water_mesh->quads3d;

		if (water_quads.size() == 0) {
			return;
		}

		//char buf[256];
		//sprintf(buf, "Minichunk at (%d, %d, %d) is drawing %d quads3d.\n", coords[0] * 16, coords[1], coords[2] * 16, quads.size());
		//OutputDebugString(buf);

		// quad VAO
		glBindVertexArray(glInfo->vao_quad);

		// write this chunk's coordinate to coordinates buffer
		glNamedBufferSubData(glInfo->trans_buf, TRANSFORM_BUFFER_COORDS_OFFSET, sizeof(ivec3), coords);

		// bind to quads attribute binding point
		glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->quad_block_type_bidx, quad_block_type_buf, 0, sizeof(Block));
		glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->q_corner1_bidx, quad_corner1_buf, 0, sizeof(ivec3));
		glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->q_corner2_bidx, quad_corner2_buf, 0, sizeof(ivec3));
		glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->q_face_bidx, quad_face_buf, 0, sizeof(ivec3));

		// DRAW!
		glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, 6, water_quads.size(), quads.size());

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

	// get valid neighboring mini coords
	inline std::vector<vmath::ivec3> neighbors() {
		// n/e/s/w
		std::vector<vmath::ivec3> result = { coords + IEAST, coords + IWEST, coords + INORTH, coords + ISOUTH };

		// up/down
		if (coords[1] < BLOCK_MAX_HEIGHT - MINICHUNK_HEIGHT) {
			result.push_back(coords + IUP * MINICHUNK_HEIGHT);
		}
		if (coords[1] > 0) {
			result.push_back(coords + IDOWN * MINICHUNK_HEIGHT);
		}

		return result;
	}

	// get all neighboring mini coords, even invalid ones (above/below)
	inline std::vector<vmath::ivec3> all_neighbors() {
		// n/e/s/w
		return { coords + IEAST, coords + IWEST, coords + INORTH, coords + ISOUTH, coords + IUP * MINICHUNK_HEIGHT , coords + IDOWN * MINICHUNK_HEIGHT };
	}

	// assumes mesh lock
	void update_quads_buf() {
		if (mesh == nullptr || water_mesh == nullptr) {
			throw "bad";
		}

		auto &quads = mesh->quads3d;
		auto &water_quads = water_mesh->quads3d;

		// delete buffers if exist
		glDeleteBuffers(1, &quad_block_type_buf);
		glDeleteBuffers(1, &quad_corner1_buf);
		glDeleteBuffers(1, &quad_corner2_buf);
		glDeleteBuffers(1, &quad_face_buf);

		num_nonwater_quads = quads.size();
		num_water_quads = water_quads.size();

		// if no quads, we done
		if (quads.size() + water_quads.size() == 0) {
			invisible = true;
			return;
		}

		// quads exist, let's set up buffers

		// create
		glCreateBuffers(1, &quad_block_type_buf);
		glCreateBuffers(1, &quad_corner1_buf);
		glCreateBuffers(1, &quad_corner2_buf);
		glCreateBuffers(1, &quad_face_buf);

		// allocate
		glNamedBufferStorage(quad_block_type_buf, sizeof(Block) * (quads.size() + water_quads.size()), NULL, GL_MAP_WRITE_BIT);
		glNamedBufferStorage(quad_corner1_buf, sizeof(ivec3) * (quads.size() + water_quads.size()), NULL, GL_MAP_WRITE_BIT);
		glNamedBufferStorage(quad_corner2_buf, sizeof(ivec3) * (quads.size() + water_quads.size()), NULL, GL_MAP_WRITE_BIT);
		glNamedBufferStorage(quad_face_buf, sizeof(ivec3) * (quads.size() + water_quads.size()), NULL, GL_MAP_WRITE_BIT);

		// map
		// TODO: Try without invalidate range? Maybe glfwSwapBuffers will be faster?
		Block* blocks = (Block*)glMapNamedBufferRange(quad_block_type_buf, 0, sizeof(Block) * (quads.size() + water_quads.size()), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
		ivec3* corner1s = (ivec3*)glMapNamedBufferRange(quad_corner1_buf, 0, sizeof(ivec3) * (quads.size() + water_quads.size()), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
		ivec3* corner2s = (ivec3*)glMapNamedBufferRange(quad_corner2_buf, 0, sizeof(ivec3) * (quads.size() + water_quads.size()), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
		ivec3* faces = (ivec3*)glMapNamedBufferRange(quad_face_buf, 0, sizeof(ivec3) * (quads.size() + water_quads.size()), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);

		// update quads
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

			// this assert has saved me so many times!
			assert(num_diffs_0 == 1 && "Invalid quad dimensions.");

			// working indices are always gonna be xy, xz, or yz.
			int working_idx_1, working_idx_2;
			gen_working_indices(zero_idx, working_idx_1, working_idx_2);

			corner1s[i] = quads[i].corners[0];
			corner2s[i] = quads[i].corners[1];
		}

		// update water quads
		for (int i = 0; i < water_quads.size(); i++) {
			// update blocks
			blocks[i + quads.size()] = (Block)water_quads[i].block;
			faces[i + quads.size()] = water_quads[i].face;

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

			corner1s[i + quads.size()] = water_quads[i].corners[0];
			corner2s[i + quads.size()] = water_quads[i].corners[1];
		}

		// flush
		glFlushMappedNamedBufferRange(quad_block_type_buf, 0, sizeof(Block) * (quads.size() + water_quads.size()));
		glFlushMappedNamedBufferRange(quad_corner1_buf, 0, sizeof(ivec3) * (quads.size() + water_quads.size()));
		glFlushMappedNamedBufferRange(quad_corner2_buf, 0, sizeof(ivec3) * (quads.size() + water_quads.size()));
		glFlushMappedNamedBufferRange(quad_face_buf, 0, sizeof(ivec3) * (quads.size() + water_quads.size()));

		// unmap
		glUnmapNamedBuffer(quad_block_type_buf);
		glUnmapNamedBuffer(quad_corner1_buf);
		glUnmapNamedBuffer(quad_corner2_buf);
		glUnmapNamedBuffer(quad_face_buf);
	}

	inline char* print_layer(int face, int layer) {
		assert(layer < height && "cannot print this layer, too high");
		assert(0 <= face && face <= 2);

		char* result = new char[16 * 16 * 8]; // up to 8 chars per block type
		char* tmp = result;

		int working_idx_1, working_idx_2;
		gen_working_indices(face, working_idx_1, working_idx_2);

		for (int u = 0; u < 16; u++) {
			tmp += sprintf(tmp, "[ ");

			for (int v = 0; v < 16; v++) {
				ivec3 coords = { 0, 0, 0 };
				coords[face] = layer;
				coords[working_idx_1] = u;
				coords[working_idx_2] = v;

				tmp += sprintf(tmp, "%d ", (uint8_t)get_block(coords));
			}

			tmp += sprintf(tmp, "]\n");
		}

		return result;
	}

};

#endif // __MINICHUNK_H__
