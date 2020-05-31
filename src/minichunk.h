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
	MiniCoords();

	MiniCoords(const vmath::ivec3& coords_);

	void set_coords(const vmath::ivec3& coords_);

	const vmath::ivec3& get_coords() const;

	// get MiniChunk's base coords in real coordinates
	vmath::ivec3 real_coords() const;

	// get MiniChunk's base coords in real coordinates
	vmath::ivec3 real_coords(vmath::ivec3& coords);

	// get MiniChunk's center coords in real coordinates
	vmath::vec3 center_coords_v3() const;

	// get MiniChunk's center coords in real coordinates
	vmath::vec4 center_coords_v41() const;

	// get valid neighboring mini coords
	std::vector<vmath::ivec3> neighbors();

	// get all neighboring mini coords, even invalid ones (above/below)
	std::vector<vmath::ivec3> all_neighbors();
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
	MiniRender();

	// Hack for now, will prob remove
	MiniRender(const MiniRender& other);

	virtual  void set_coords(const vmath::ivec3& coords_);

	void set_mesh(std::unique_ptr<MiniChunkMesh> mesh_);

	void set_water_mesh(std::unique_ptr<MiniChunkMesh> water_mesh_);

	bool get_invisible() const;

	void set_invisible(const bool invisible);

	// render this minichunk's texture meshes
	void render_meshes(const OpenGLInfo* glInfo);

	// render this minichunk's water meshes
	void render_water_meshes(const OpenGLInfo* glInfo);

	// assumes mesh lock
	void update_quads_buf(const OpenGLInfo* glInfo);

	// TODO: remove this from render.cpp?
	void recreate_vao(const OpenGLInfo* glInfo, const GLuint size);
};

class MiniChunk : public MiniCoords, public ChunkData
{
public:
	MiniChunk();

	// Hack for now, will prob remove
	MiniChunk(const MiniChunk& other);

	MiniChunk(MiniChunk&& other) = delete;

	char* print_layer(int face, int layer);
};
