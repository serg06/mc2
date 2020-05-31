#pragma once

#include "world_utils.h"

#include "chunk.h"

#include "vmath.h"
#include "zmq.hpp"

#include <queue>
#include <unordered_map>
#include <unordered_set>

// Rendering part
#include "minichunkmesh.h"
#include "render.h"

using namespace std;

class WorldMeshPart : public WorldCommonPart
{
public:
	WorldMeshPart(zmq::context_t* const ctx_);

	// convert 2D quads to 3D quads
	// face: for offset
	static inline vector<Quad3D> quads_2d_3d(const vector<Quad2D>& quads2d, const int layers_idx, const int layer_no, const vmath::ivec3& face);

	// generate layer by grabbing face blocks directly from the minichunk
	static inline void gen_layer_generalized(const std::shared_ptr<MiniChunk> mini, const std::shared_ptr<MiniChunk> face_mini, const int layers_idx, const int layer_no, const vmath::ivec3 face, BlockType(&result)[16][16]);

	static inline bool is_face_visible(const BlockType& block, const BlockType& face_block);
	void gen_layer(const std::shared_ptr<MeshGenRequest> req, const int layers_idx, const int layer_no, const vmath::ivec3& face, BlockType(&result)[16][16]);

	// given 2D array of block numbers, generate optimal quads
	static inline vector<Quad2D> gen_quads(const BlockType(&layer)[16][16], /* const Metadata(&metadata_layer)[16][16], */ bool(&merged)[16][16]);

	static inline void mark_as_merged(bool(&merged)[16][16], const vmath::ivec2& start, const vmath::ivec2& max_size);

	// given a layer and start point, find its best dimensions
	static inline vmath::ivec2 get_max_size(const BlockType(&layer)[16][16], const bool(&merged)[16][16], const vmath::ivec2& start_point, const BlockType& block_type);

	bool check_if_covered(std::shared_ptr<MeshGenRequest> req);

	MeshGenResult* gen_minichunk_mesh_from_req(std::shared_ptr<MeshGenRequest> req);
	std::unique_ptr<MiniChunkMesh> gen_minichunk_mesh(std::shared_ptr<MeshGenRequest> req);
};
