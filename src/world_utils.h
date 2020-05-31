#pragma once

#include "chunk.h"
#include "vmath.h"
#include "zmq.hpp"

#include <functional>
#include <queue>
#include <unordered_map>
#include <unordered_set>

// Rendering part
#include "minichunkmesh.h"
#include "render.h"

using namespace std;

struct Quad2D {
	BlockType block;
	vmath::ivec2 corners[2];
	uint8_t lighting = 0; // TODO: max instead?
	Metadata metadata = 0;
};

bool operator==(const Quad2D& lhs, const Quad2D& rhs);

struct MeshGenResult
{
	MeshGenResult(const vmath::ivec3& coords_, bool invisible_, const std::unique_ptr<MiniChunkMesh>& mesh_, const std::unique_ptr<MiniChunkMesh>& water_mesh_) = delete;
	MeshGenResult(const vmath::ivec3& coords_, bool invisible_, std::unique_ptr<MiniChunkMesh>&& mesh_, std::unique_ptr<MiniChunkMesh>&& water_mesh_)
		: coords(coords_), invisible(invisible_), mesh(std::move(mesh_)), water_mesh(std::move(water_mesh_))
	{
	}
	MeshGenResult(const MeshGenResult& other) = delete;
	MeshGenResult(MeshGenResult&& other) noexcept
	{
		if (this != &other)
		{
			coords = other.coords;
			invisible = other.invisible;
			mesh = std::move(other.mesh);
			water_mesh = std::move(other.water_mesh);
		}
	}

	MeshGenResult& operator=(const MeshGenResult&& other) = delete;
	MeshGenResult& operator=(MeshGenResult&& other)
	{
		if (this != &other)
		{
			coords = other.coords;
			invisible = other.invisible;
			mesh = std::move(other.mesh);
			water_mesh = std::move(other.water_mesh);
		}
		return *this;
	}

	vmath::ivec3 coords;
	bool invisible;
	std::unique_ptr<MiniChunkMesh> mesh;
	std::unique_ptr<MiniChunkMesh> water_mesh;
};

struct MeshGenRequestData
{
	std::shared_ptr<MiniChunk> self;
	std::shared_ptr<MiniChunk> north;
	std::shared_ptr<MiniChunk> south;
	std::shared_ptr<MiniChunk> east;
	std::shared_ptr<MiniChunk> west;
	std::shared_ptr<MiniChunk> up;
	std::shared_ptr<MiniChunk> down;
};

struct MeshGenRequest
{
	vmath::ivec3 coords;
	std::shared_ptr<MeshGenRequestData> data;
};

// get chunk-coordinates of chunk containing the block at (x, _, z)
vmath::ivec2 get_chunk_coords(const int x, const int z);

// get chunk-coordinates of chunk containing the block at (x, _, z)
vmath::ivec2 get_chunk_coords(const float x, const float z);

// get minichunk-coordinates of minichunk containing the block at (x, y, z)
vmath::ivec3 get_mini_coords(const int x, const int y, const int z);
vmath::ivec3 get_mini_coords(const vmath::ivec3& xyz);

// given a block's real-world coordinates, return that block's coordinates relative to its chunk
vmath::ivec3 get_chunk_relative_coordinates(const int x, const int y, const int z);

// given a block's real-world coordinates, return that block's coordinates relative to its mini
vmath::ivec3 get_mini_relative_coords(const int x, const int y, const int z);
vmath::ivec3 get_mini_relative_coords(const vmath::ivec3& xyz);

/**
* Call the callback with (x,y,z,value,face) of all blocks along the line
* segment from point 'origin' in vector direction 'direction' of length
* 'radius'. 'radius' may be infinite.
*
* 'face' is the normal vector of the face of that block that was entered.
* It should not be used after the callback returns.
*
* If the callback returns a true value, the traversal will be stopped.
*/
void raycast(const vec4& origin, const vec4& direction, int radius, vmath::ivec3* result_coords, vmath::ivec3* result_face, const std::function <bool(const vmath::ivec3& coords, const vmath::ivec3& face)>& stop_check);

// get chunk-coordinates of chunk containing the block at (x, _, z)
vmath::ivec2 get_chunk_coords(const int x, const int z);

// get chunk-coordinates of chunk containing the block at (x, _, z)
vmath::ivec2 get_chunk_coords(const float x, const float z);

// get minichunk-coordinates of minichunk containing the block at (x, y, z)
vmath::ivec3 get_mini_coords(const int x, const int y, const int z);
vmath::ivec3 get_mini_coords(const vmath::ivec3& xyz);

// given a block's real-world coordinates, return that block's coordinates relative to its chunk
vmath::ivec3 get_chunk_relative_coordinates(const int x, const int y, const int z);

// given a block's real-world coordinates, return that block's coordinates relative to its mini
vmath::ivec3 get_mini_relative_coords(const int x, const int y, const int z);
vmath::ivec3 get_mini_relative_coords(const vmath::ivec3& xyz);

