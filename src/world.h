#pragma once

#include "chunk.h"
#include "world_utils.h"

#include "messaging.h"
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

class WorldDataPart
{
public:
	WorldDataPart(zmq::context_t* const ctx_);

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

	// generate chunks if they don't exist yet
	void gen_chunks_if_required(const vector<vmath::ivec2>& chunk_coords);

	// generate multiple chunks
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

	// Handle any messages on the message bus
	void handle_messages();

private:
	BusNode bus;
};
