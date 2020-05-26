#pragma once

#include "block.h"
#include "chunkdata.h"
#include "minichunkmesh.h"
#include "render.h"
#include "util.h"
#include "vmath.h"

#include <assert.h>
#include <algorithm> // std::find
#include <memory>
#include <mutex>
#include <vector>

constexpr int MINICHUNK_WIDTH = 16;
constexpr int MINICHUNK_HEIGHT = 16;
constexpr int MINICHUNK_DEPTH = 16;
constexpr int MINICHUNK_SIZE = MINICHUNK_WIDTH * MINICHUNK_DEPTH * MINICHUNK_HEIGHT;

class MiniCoords
{
private:
	vmath::ivec3 coords;

public:
	MiniCoords() : coords({ 0, 0, 0 })
	{
	}

	MiniCoords(const vmath::ivec3& coords_) : coords(coords_)
	{
	}

	virtual inline void set_coords(const vmath::ivec3& coords_)
	{
		coords = coords_;
	}

	const inline vmath::ivec3& get_coords() const {
		return coords;
	}

	// get MiniChunk's base coords in real coordinates
	inline vmath::ivec3 real_coords() const {
		return { coords[0] * 16, coords[1], coords[2] * 16 };
	}

	// get MiniChunk's base coords in real coordinates
	static inline vmath::ivec3 real_coords(vmath::ivec3& coords) {
		return { coords[0] * 16, coords[1], coords[2] * 16 };
	}

	// get MiniChunk's center coords in real coordinates
	inline vmath::vec3 center_coords_v3() const {
		return { coords[0] * 16.0f + 8.0f, coords[1] + 8.0f, coords[2] * 16.0f + 8.0f };
	}

	// get MiniChunk's center coords in real coordinates
	inline vmath::vec4 center_coords_v41() const {
		return { coords[0] * 16.0f + 8.0f, coords[1] + 8.0f, coords[2] * 16.0f + 8.0f, 1.0f };
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
		// n/e/s/w/u/d
		return { coords + IEAST, coords + IWEST, coords + INORTH, coords + ISOUTH, coords + IUP * MINICHUNK_HEIGHT , coords + IDOWN * MINICHUNK_HEIGHT };
	}
};

class MiniRender : public MiniCoords
{
private:
	std::unique_ptr<MiniChunkMesh> mesh;
	std::unique_ptr<MiniChunkMesh> water_mesh;
	bool meshes_updated;

	// TODO: When someone else sets invisibility, we want to delete bufs as well.
	GLuint quad_data_buf;
	GLuint base_coords_buf;

	// number of quads inside the buffer, as reading from mesh is not always reliable
	GLuint num_nonwater_quads;
	GLuint num_water_quads;

	// vao
	GLuint vao;

	bool invisible;

public:
	MiniRender()
		: MiniCoords(),
		mesh(nullptr), water_mesh(nullptr), meshes_updated(false),
		quad_data_buf(0), base_coords_buf(0),
		num_nonwater_quads(0), num_water_quads(0),
		vao(0), invisible(false)
	{
	}

	// Hack for now, will prob remove
	MiniRender(const MiniRender& other)
		: MiniCoords(other),
		mesh(other.mesh != nullptr ? std::make_unique<MiniChunkMesh>(*other.mesh) : nullptr),
		water_mesh(other.water_mesh != nullptr ? std::make_unique<MiniChunkMesh>(*other.water_mesh) : nullptr),
		meshes_updated(other.meshes_updated),
		quad_data_buf(other.quad_data_buf), base_coords_buf(other.base_coords_buf),
		num_nonwater_quads(other.num_nonwater_quads), num_water_quads(other.num_water_quads),
		vao(other.vao), invisible(other.invisible)
	{
	}

	virtual inline void set_coords(const vmath::ivec3& coords_)
	{
		MiniCoords::set_coords(coords_);

		// update coords buf
		glDeleteBuffers(1, &base_coords_buf);
		glCreateBuffers(1, &base_coords_buf);
		glNamedBufferStorage(base_coords_buf, sizeof(get_coords()), get_coords(), NULL);
	}

	inline void set_mesh(std::unique_ptr<MiniChunkMesh> mesh_) {
		std::swap(this->mesh, mesh_);
		meshes_updated = true;
	}

	inline void set_water_mesh(std::unique_ptr<MiniChunkMesh> water_mesh_) {
		std::swap(this->water_mesh, water_mesh_);
		meshes_updated = true;
	}

	inline bool get_invisible() const {
		return invisible;
	}

	inline void set_invisible(const bool invisible) {
		this->invisible = invisible;

		// TODO: if set to invisible, also mark buffers/vao for deletion?
	}

	// render this minichunk's texture meshes
	inline void render_meshes(const OpenGLInfo* glInfo) {
		// don't draw if covered in all sides
		if (invisible || mesh == nullptr) {
			return;
		}

		// update quads if needed
		if (meshes_updated) {
			meshes_updated = false;
			update_quads_buf(glInfo);
		}

		if (num_nonwater_quads == 0) {
			return;
		}

		// quad VAO
		glBindVertexArray(vao);

		// DRAW!
		glDrawArrays(GL_POINTS, 0, num_nonwater_quads);
	}

	// render this minichunk's water meshes
	inline void render_water_meshes(const OpenGLInfo* glInfo) {
		// don't draw if covered in all sides
		if (invisible || water_mesh == nullptr) {
			return;
		}

		// update quads if needed
		if (meshes_updated) {
			meshes_updated = false;
			update_quads_buf(glInfo);
		}

		if (num_water_quads == 0) {
			return;
		}

		// quad VAO
		glBindVertexArray(vao);

		// DRAW!
		glDrawArrays(GL_POINTS, num_nonwater_quads, num_water_quads);
	}

	// assumes mesh lock
	void update_quads_buf(const OpenGLInfo* glInfo) {
		if (mesh == nullptr || water_mesh == nullptr) {
			throw "bad";
		}

		auto& quads = mesh->get_quads();
		auto& water_quads = water_mesh->get_quads();

		// if no quads, we done
		if (quads.size() + water_quads.size() == 0) {
			invisible = true;
			return;
		}

		recreate_vao(glInfo, quads.size() + water_quads.size());

		num_nonwater_quads = quads.size();
		num_water_quads = water_quads.size();

		// map
		Quad3D* gpu_quads = (Quad3D*)glMapNamedBufferRange(quad_data_buf, 0, sizeof(Quad3D) * (quads.size() + water_quads.size()), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);

		// update quads
		std::copy(quads.begin(), quads.end(), gpu_quads);

		// update water quads
		std::copy(water_quads.begin(), water_quads.end(), gpu_quads + quads.size());

		// flush
		glFlushMappedNamedBufferRange(quad_data_buf, 0, sizeof(Quad3D) * (quads.size() + water_quads.size()));

		// unmap
		glUnmapNamedBuffer(quad_data_buf);

#ifdef _DEBUG

		// quads error check
		for (int i = 0; i < quads.size(); i++) {
			// error check:
			// make sure at least one dimension is killed - i.e. it's a flat quad ( todo. make sure other 2 dimensions are >= 1 size.)
			ivec3 diffs = quads[i].corner2 - quads[i].corner1;

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
		}


		// water quads error check
		for (int i = 0; i < water_quads.size(); i++) {
			// error check:
			// make sure at least one dimension is killed - i.e. it's a flat quad ( todo. make sure other 2 dimensions are >= 1 size.)
			ivec3 diffs = water_quads[i].corner2 - water_quads[i].corner1;

			int num_diffs_0 = 0;
			int zero_idx = 0;

			for (int i = 0; i < 3; i++) {
				if (diffs[i] == 0) {
					num_diffs_0 += 1;
					zero_idx = i;
				}
			}

			// this assert has saved me so many times!
			assert(num_diffs_0 == 1 && "Invalid water quad dimensions.");
		}

#endif
	}

	// TODO: remove this from render.cpp?
	inline void recreate_vao(const OpenGLInfo* glInfo, const GLuint size) {
		// delete
		glDeleteBuffers(1, &quad_data_buf);
		glDeleteVertexArrays(1, &vao);

		// create
		glCreateBuffers(1, &quad_data_buf);
		glCreateVertexArrays(1, &vao);

		// allocate
		glNamedBufferStorage(quad_data_buf, sizeof(Quad3D) * size, NULL, GL_MAP_WRITE_BIT);

		// vao: create VAO for Quads, so we can tell OpenGL how to use it when it's bound

		// vao: enable all Quad's attributes, 1 at a time
		glEnableVertexArrayAttrib(vao, glInfo->q_block_type_attr_idx);
		glEnableVertexArrayAttrib(vao, glInfo->q_corner1_attr_idx);
		glEnableVertexArrayAttrib(vao, glInfo->q_corner2_attr_idx);
		glEnableVertexArrayAttrib(vao, glInfo->q_face_attr_idx);
		glEnableVertexArrayAttrib(vao, glInfo->q_base_coords_attr_idx);
		glEnableVertexArrayAttrib(vao, glInfo->q_lighting_attr_idx);
		glEnableVertexArrayAttrib(vao, glInfo->q_metadata_attr_idx);

		// vao: set up formats for Quad's attributes, 1 at a time
		glVertexArrayAttribIFormat(vao, glInfo->q_block_type_attr_idx, 1, GL_UNSIGNED_BYTE, offsetof(Quad3D, block));
		glVertexArrayAttribIFormat(vao, glInfo->q_corner1_attr_idx, 3, GL_INT, offsetof(Quad3D, corner1));
		glVertexArrayAttribIFormat(vao, glInfo->q_corner2_attr_idx, 3, GL_INT, offsetof(Quad3D, corner2));
		glVertexArrayAttribIFormat(vao, glInfo->q_face_attr_idx, 3, GL_INT, offsetof(Quad3D, face));
		glVertexArrayAttribIFormat(vao, glInfo->q_lighting_attr_idx, 1, GL_UNSIGNED_BYTE, offsetof(Quad3D, lighting));
		glVertexArrayAttribIFormat(vao, glInfo->q_metadata_attr_idx, 1, GL_UNSIGNED_BYTE, offsetof(Quad3D, metadata));

		glVertexArrayAttribIFormat(vao, glInfo->q_base_coords_attr_idx, 3, GL_INT, 0);

		// vao: match attributes to binding indices
		glVertexArrayAttribBinding(vao, glInfo->q_block_type_attr_idx, glInfo->quad_data_bidx);
		glVertexArrayAttribBinding(vao, glInfo->q_corner1_attr_idx, glInfo->quad_data_bidx);
		glVertexArrayAttribBinding(vao, glInfo->q_corner2_attr_idx, glInfo->quad_data_bidx);
		glVertexArrayAttribBinding(vao, glInfo->q_face_attr_idx, glInfo->quad_data_bidx);
		glVertexArrayAttribBinding(vao, glInfo->q_lighting_attr_idx, glInfo->quad_data_bidx);
		glVertexArrayAttribBinding(vao, glInfo->q_metadata_attr_idx, glInfo->quad_data_bidx);

		glVertexArrayAttribBinding(vao, glInfo->q_base_coords_attr_idx, glInfo->q_base_coords_bidx);

		// vao: match attributes to buffers
		glVertexArrayVertexBuffer(vao, glInfo->quad_data_bidx, quad_data_buf, 0, sizeof(Quad3D));

		glVertexArrayVertexBuffer(vao, glInfo->q_base_coords_bidx, base_coords_buf, 0, sizeof(vmath::ivec3));

		// vao: extra properties
		glBindVertexArray(vao);

		// instance attribute
		glVertexAttribDivisor(glInfo->q_base_coords_attr_idx, 1);

		glBindVertexArray(0);
	}
};

class MiniChunk : public MiniCoords, public ChunkData
{
public:
	MiniChunk() : ChunkData(MINICHUNK_WIDTH, MINICHUNK_HEIGHT, MINICHUNK_DEPTH)
	{
	}

	// Hack for now, will prob remove
	MiniChunk(const MiniChunk& other) : MiniCoords(other), ChunkData(other)
	{
	}

	MiniChunk(MiniChunk&& other) = delete;

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
