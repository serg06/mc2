#pragma once

#include "chunk.h"
#include "world_utils.h"

#include "vmath.h"
#include "zmq.hpp"

#include <queue>
#include <unordered_map>
#include <unordered_set>

// Rendering part
#include "contiguous_hashmap.h"
#include "minichunkmesh.h"
#include "render.h"

using namespace std;

class WorldRenderPart : public WorldCommonPart
{
public:
	WorldRenderPart(zmq::context_t* ctx_);

	std::unordered_map<vmath::ivec3, std::shared_ptr<MiniRender>, vecN_hash> mesh_map;

	// count how many times render() has been called
	int rendered = 0;

	// get mini render component or nullptr
	std::shared_ptr<MiniRender> get_mini_render_component(const int x, const int y, const int z);
	std::shared_ptr<MiniRender> get_mini_render_component(const vmath::ivec3& xyz);

	// get mini render component or nullptr
	std::shared_ptr<MiniRender> get_mini_render_component_or_generate(const int x, const int y, const int z);
	std::shared_ptr<MiniRender> get_mini_render_component_or_generate(const vmath::ivec3& xyz);

	void update_meshes();
	void render(OpenGLInfo* glInfo, GlfwInfo* windowInfo, const vmath::vec4(&planes)[6], const vmath::ivec3& staring_at);

	// check if a mini is visible in a frustum
	static inline bool mini_in_frustum(const MiniRender* mini, const vmath::vec4(&planes)[6]);

	static inline float intbound(const float s, const float ds);
	void highlight_block(const OpenGLInfo* glInfo, const GlfwInfo* windowInfo, const int x, const int y, const int z);
	void highlight_block(const OpenGLInfo* glInfo, const GlfwInfo* windowInfo, const vmath::ivec3& xyz);
};

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

class WorldDataPart : public WorldCommonPart
{
public:
	WorldDataPart(zmq::context_t* ctx_);

	// map of (chunk coordinate) -> chunk
	contiguous_hashmap<vmath::ivec2, Chunk*, vecN_hash> chunk_map;

	// get_chunk cache
	Chunk* chunk_cache[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
	vmath::ivec2 chunk_cache_ivec2[5] = { vmath::ivec2(INT_MAX), vmath::ivec2(INT_MAX), vmath::ivec2(INT_MAX), vmath::ivec2(INT_MAX), vmath::ivec2(INT_MAX) };
	int chunk_cache_clock_hand = 0; // clock cache

	// what tick the world is at
	// TODO: private
	int current_tick = 0;
	
	// water propagation min-priority queue
	// maps <tick to propagate water at> to <coordinate of water>
	// TODO: uniqueness (have hashtable which maps coords -> tick, and always keep earliest tick when adding)
	// TODO: If I never add anything else to this queue (like fire/lava/etc), change it to a normal queue.
	std::priority_queue<std::pair<int, vmath::ivec3>, std::vector<std::pair<int, vmath::ivec3>>, std::greater<std::pair<int, vmath::ivec3>>> water_propagation_queue;
	
	// update tick to *new_tick*
	void update_tick(const int new_tick);

	// enqueue mesh generation of this mini
	// expects mesh lock
	void enqueue_mesh_gen(std::shared_ptr<MiniChunk> mini, const bool front_of_queue = false);

	// add chunk to chunk coords (x, z)
	void add_chunk(const int x, const int z, Chunk* chunk);

	// get multiple chunks -- much faster than get_chunk_generate_if_required when n > 1
	std::unordered_set<Chunk*, chunk_hash> get_chunks_generate_if_required(const vector<vmath::ivec2>& chunk_coords);

	// generate chunks if they don't exist yet
	void gen_chunks_if_required(const vector<vmath::ivec2>& chunk_coords);

	// generate all chunks (much faster than gen_chunk)
	void gen_chunks(const vector<vmath::ivec2>& to_generate);

	// generate all chunks (much faster than gen_chunk)
	void gen_chunks(const std::unordered_set<vmath::ivec2, vecN_hash>& to_generate);

	// get chunk or nullptr (using cache) (TODO: LRU?)
	Chunk* get_chunk(const int x, const int z);
	Chunk* get_chunk(const vmath::ivec2& xz);

	// get chunk or nullptr (no cache)
	Chunk* get_chunk_(const int x, const int z);

	// get mini or nullptr
	std::shared_ptr<MiniChunk> get_mini(const int x, const int y, const int z);
	std::shared_ptr<MiniChunk> get_mini(const vmath::ivec3& xyz);

	// generate chunks near player
	void gen_nearby_chunks(const vmath::vec4& position, const int& distance);

	// get chunk that contains block at (x, _, z)
	Chunk* get_chunk_containing_block(const int x, const int z);

	// get minichunk that contains block at (x, y, z)
	std::shared_ptr<MiniChunk> get_mini_containing_block(const int x, const int y, const int z);

	// get minichunks that touch any face of the block at (x, y, z)
	vector<std::shared_ptr<MiniChunk>> get_minis_touching_block(const int x, const int y, const int z);

	// get a block's type
	// inefficient when called repeatedly - if you need multiple blocks from one mini/chunk, use get_mini (or get_chunk) and mini.get_block.
	BlockType get_type(const int x, const int y, const int z);
	BlockType get_type(const vmath::ivec3& xyz);
	BlockType get_type(const vmath::ivec4& xyz_);

	// set a block's type
	// inefficient when called repeatedly
	void set_type(const int x, const int y, const int z, const BlockType& val);
	void set_type(const vmath::ivec3& xyz, const BlockType& val);
	void set_type(const vmath::ivec4& xyz_, const BlockType& val);

	// when a mini updates, update its and its neighbors' meshes, if required.
	// mini: the mini that changed
	// block: the mini-coordinates of the block that was added/deleted
	// TODO: Use block.
	void on_mini_update(std::shared_ptr<MiniChunk> mini, const vmath::ivec3& block);

	// update meshes
	void on_block_update(const vmath::ivec3& block);

	void destroy_block(const int x, const int y, const int z);

	void destroy_block(const vmath::ivec3& xyz);

	void add_block(const int x, const int y, const int z, const BlockType& block);

	void add_block(const vmath::ivec3& xyz, const BlockType& block);


	// TODO
	Metadata get_metadata(const int x, const int y, const int z);
	Metadata get_metadata(const vmath::ivec3& xyz);
	Metadata get_metadata(const vmath::ivec4& xyz_);

	// TODO
	void set_metadata(const int x, const int y, const int z, const Metadata& val);
	void set_metadata(const vmath::ivec3& xyz, const Metadata& val);
	void set_metadata(const vmath::ivec4& xyz_, const Metadata& val);

	// TODO
	void schedule_water_propagation(const vmath::ivec3& xyz);
	void schedule_water_propagation_neighbors(const vmath::ivec3& xyz);

	// get liquid at (x, y, z) and propagate it
	void propagate_water(int x, int y, int z);

	// given water at (x, y, z), find all directions which lead to A shortest path down
	// radius = 4
	// TODO
	std::unordered_set<vmath::ivec3, vecN_hash> find_shortest_water_path(int x, int y, int z);

	// For a certain corner, get height of flowing water at that corner
	float get_water_height(const vmath::ivec3& corner);
};
