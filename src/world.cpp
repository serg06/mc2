#include "world.h"

#include "contiguous_hashmap.h"
#include "chunk.h"
#include "chunkdata.h"
#include "messaging.h"
#include "minichunkmesh.h"
#include "render.h"
#include "shapes.h"
#include "util.h"

#include "vmath.h"
#include "zmq_addon.hpp"

#include <assert.h>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <queue>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <sstream>
#include <string>

// radius from center of minichunk that must be included in view frustum
constexpr float FRUSTUM_MINI_RADIUS_ALLOWANCE = 28.0f;

WorldDataPart::WorldDataPart(zmq::context_t* const ctx_) : bus(ctx_)
{
#ifdef _DEBUG
	bus.out.setsockopt(ZMQ_SUBSCRIBE, "", 0);
#else
	for (const auto& m : msg::world_thread_incoming)
	{
		// TODO: Upgrade zmq and replace this with .set()
		bus.out.setsockopt(ZMQ_SUBSCRIBE, m.c_str(), m.size());
	}
#endif // _DEBUG
}

// update tick to *new_tick*
void WorldDataPart::update_tick(const int new_tick) {
	// can only grow, not shrink
	if (new_tick <= current_tick) {
		return;
	}

	current_tick = new_tick;

	// propagate any water we need to propagate
	while (!water_propagation_queue.empty()) {
		// get item from queue
		const auto& [tick, xyz] = water_propagation_queue.top();

		// if tick is in the future, ignore
		if (tick > current_tick) {
			break;
		}

		// do it
		water_propagation_queue.pop();
		propagate_water(xyz[0], xyz[1], xyz[2]);
	}
}

// enqueue mesh generation of this mini
// expects mesh lock
void WorldDataPart::enqueue_mesh_gen(std::shared_ptr<MiniChunk> mini, const bool front_of_queue) {
	assert(mini != nullptr && "seriously?");

	// check if mini in set
	MeshGenRequest* req = new MeshGenRequest();
	req->coords = mini->get_coords();
	req->data = std::make_shared<MeshGenRequestData>();
	req->data->self = mini;

#define ADD(ATTR, DIRECTION)\
		{\
			std::shared_ptr<MiniChunk> minip_ = get_mini(mini->get_coords() + DIRECTION);\
			req->data->ATTR = minip_;\
		}

	ADD(up, IUP);
	ADD(down, IDOWN);
	ADD(east, IEAST);
	ADD(west, IWEST);
	ADD(north, INORTH);
	ADD(south, ISOUTH);
#undef ADD

	// TODO: Figure out how to do zero-copy messaging since we don't need to copy msg::MESH_GEN_REQ (it's static const)
	std::vector<zmq::const_buffer> message({
		zmq::buffer(msg::MESH_GEN_REQUEST),
		zmq::buffer(&req, sizeof(req))
		});

	auto ret = zmq::send_multipart(bus.in, message, zmq::send_flags::dontwait);
	assert(ret);

#ifdef _DEBUG
	std::stringstream out;
	out << "Enqueue coords " << vec2str(req->coords) << "\n";
	OutputDebugString(out.str().c_str());
#endif //_DEBUG

#ifdef SLEEPS
	// Just in case
	std::this_thread::sleep_for(std::chrono::microseconds(1));
#endif // SLEEPS
}

// add chunk to chunk coords (x, z)
void WorldDataPart::add_chunk(const int x, const int z, Chunk* chunk) {
	const vmath::ivec2 coords = { x, z };
	const auto search = chunk_map.find(coords);

	// if element already exists, error
	if (search != chunk_map.end()) {
		throw "Tried to add chunk but it already exists.";
	}

	// insert our chunk
	if (chunk == nullptr) {
		throw "Wew";
	}
	chunk_map[coords] = chunk;

	// invalidate that chunk in cache
	for (int i = 0; i < 5; i++) {
		if (chunk_cache_ivec2[i] == coords) {
			chunk_cache[i] = nullptr;
			chunk_cache_ivec2[i] = vmath::ivec2(std::numeric_limits<int>::max());
		}
	}
}

// generate chunks if they don't exist yet
void WorldDataPart::gen_chunks_if_required(const vector<vmath::ivec2>& chunk_coords) {
	// don't wanna generate duplicates
	std::unordered_set<vmath::ivec2, vecN_hash> to_generate;

	for (auto coords : chunk_coords) {
		const auto search = chunk_map.find(coords);

		// if doesn't exist, need to generate it
		if (search == chunk_map.end()) {
			to_generate.insert(coords);
		}
	}

	if (to_generate.size() > 0) {
		gen_chunks(to_generate);
	}
}

// generate multiple chunks
void WorldDataPart::gen_chunks(const std::unordered_set<vmath::ivec2, vecN_hash>& to_generate) {
	// Instead of generating chunks, we request the ChunkGenThread to do it for us.
	// TODO: Send one request with a vector of coords?
	// TODO: Instead of having a queue, have mesh and chunk gen threads have a "center" position (player's location),
	//         then sort the vector when it changes. Can do that with a Priority Heap, where priority = distance to center.
	for (const vmath::ivec2& coords : to_generate)
	{
		ChunkGenRequest* req = new ChunkGenRequest;
		req->coords = coords;
		std::vector<zmq::const_buffer> message({
			zmq::buffer(msg::CHUNK_GEN_REQUEST),
			zmq::buffer(&req, sizeof(req))
			});

		auto ret = zmq::send_multipart(bus.in, message, zmq::send_flags::dontwait);
		assert(ret);
	}

	return;
}

// get chunk or nullptr (using cache) (TODO: LRU?)
Chunk* WorldDataPart::get_chunk(const int x, const int z) {
	const vmath::ivec2 coords = { x, z };

	// if in cache, return
	for (int i = 0; i < 5; i++) {
		// start at chunk_cache_clock_hand and search backwards
		if (chunk_cache_ivec2[(chunk_cache_clock_hand - i + 5) % 5] == coords) {
			return chunk_cache[(chunk_cache_clock_hand - i + 5) % 5];
		}
	}

	// not in cache, get normally
	Chunk* result = get_chunk_(x, z);

	// save in cache
	chunk_cache_clock_hand = (chunk_cache_clock_hand + 1) % 5;
	chunk_cache[chunk_cache_clock_hand] = result;
	chunk_cache_ivec2[chunk_cache_clock_hand] = coords;

	return result;
}

Chunk* WorldDataPart::get_chunk(const vmath::ivec2& xz) { return get_chunk(xz[0], xz[1]); }

// get chunk or nullptr (no cache)
Chunk* WorldDataPart::get_chunk_(const int x, const int z) {
	const auto search = chunk_map.find({ x, z });

	// if doesn't exist, return null
	if (search == chunk_map.end()) {
		return nullptr;
	}

	return *search;
}

// get mini or nullptr
std::shared_ptr<MiniChunk> WorldDataPart::get_mini(const int x, const int y, const int z) {
	const auto search = chunk_map.find({ x, z });

	// if chunk doesn't exist, return null
	if (search == chunk_map.end()) {
		return nullptr;
	}

	Chunk* chunk = *search;
	return chunk->get_mini_with_y_level((y / 16) * 16); // TODO: Just y % 16?
}

std::shared_ptr<MiniChunk> WorldDataPart::get_mini(const vmath::ivec3& xyz) { return get_mini(xyz[0], xyz[1], xyz[2]); }

// generate chunks near player
void WorldDataPart::gen_nearby_chunks(const vmath::vec4& position, const int& distance) {
	assert(distance >= 0 && "invalid distance");

	const vmath::ivec2 chunk_coords = get_chunk_coords(position[0], position[2]);
	const vector<vmath::ivec2> coords = gen_circle(distance, chunk_coords);

	gen_chunks_if_required(coords);
}

// get chunk that contains block at (x, _, z)
Chunk* WorldDataPart::get_chunk_containing_block(const int x, const int z) {
	return get_chunk((int)floorf((float)x / 16.0f), (int)floorf((float)z / 16.0f));
}

// get minichunk that contains block at (x, y, z)
std::shared_ptr<MiniChunk> WorldDataPart::get_mini_containing_block(const int x, const int y, const int z) {
	Chunk* chunk = get_chunk_containing_block(x, z);
	if (chunk == nullptr) {
		return nullptr;
	}
	return chunk->get_mini_with_y_level((y / 16) * 16);
}


// get minichunks that touch any face of the block at (x, y, z)
std::vector<std::shared_ptr<MiniChunk>> WorldDataPart::get_minis_touching_block(const int x, const int y, const int z) {
	vector<std::shared_ptr<MiniChunk>> result;
	vector<vmath::ivec3> potential_mini_coords;

	const vmath::ivec3 mini_coords = get_mini_coords(x, y, z);
	const vmath::ivec3 mini_relative_coords = get_mini_relative_coords(x, y, z);

	potential_mini_coords.push_back(mini_coords);

	if (mini_relative_coords[0] == 0) potential_mini_coords.push_back(mini_coords + IWEST);
	if (mini_relative_coords[0] == 15) potential_mini_coords.push_back(mini_coords + IEAST);

	if (mini_relative_coords[1] == 0 && y > 0) potential_mini_coords.push_back(mini_coords + IDOWN * MINICHUNK_HEIGHT);
	if (mini_relative_coords[1] == 15 && y + MINICHUNK_HEIGHT < 256) potential_mini_coords.push_back(mini_coords + IUP * MINICHUNK_HEIGHT);
	if (mini_relative_coords[2] == 0) potential_mini_coords.push_back(mini_coords + INORTH);
	if (mini_relative_coords[2] == 15) potential_mini_coords.push_back(mini_coords + ISOUTH);

	for (auto& coords : potential_mini_coords) {
		const auto mini = get_mini(coords);
		if (mini != nullptr) {
			result.push_back(mini);
		}
	}

	return result;
}

// get a block's type
// inefficient when called repeatedly - if you need multiple blocks from one mini/chunk, use get_mini (or get_chunk) and mini.get_block.
BlockType WorldDataPart::get_type(const int x, const int y, const int z) {
	Chunk* chunk = get_chunk_containing_block(x, z);

	if (!chunk) {
		return BlockType::Air;
	}

	const vmath::ivec3 chunk_coords = get_chunk_relative_coordinates(x, y, z);

	return chunk->get_block(chunk_coords);
}

BlockType WorldDataPart::get_type(const vmath::ivec3& xyz) { return get_type(xyz[0], xyz[1], xyz[2]); }
BlockType WorldDataPart::get_type(const vmath::ivec4& xyz_) { return get_type(xyz_[0], xyz_[1], xyz_[2]); }

// set a block's type
// inefficient when called repeatedly
void WorldDataPart::set_type(const int x, const int y, const int z, const BlockType& val) {
	Chunk* chunk = get_chunk_containing_block(x, z);

	if (!chunk) {
		return;
	}

	const vmath::ivec3 chunk_coords = get_chunk_relative_coordinates(x, y, z);
	chunk->set_block(chunk_coords, val);
}

void WorldDataPart::set_type(const vmath::ivec3& xyz, const BlockType& val) { return set_type(xyz[0], xyz[1], xyz[2], val); }
void WorldDataPart::set_type(const vmath::ivec4& xyz_, const BlockType& val) { return set_type(xyz_[0], xyz_[1], xyz_[2], val); }

// when a mini updates, update its and its neighbors' meshes, if required.
// mini: the mini that changed
// block: the mini-coordinates of the block that was added/deleted
// TODO: Use block.
void WorldDataPart::on_mini_update(std::shared_ptr<MiniChunk> mini, const vmath::ivec3& block) {
	// for now, don't care if something was done in an unloaded mini
	if (mini == nullptr) {
		return;
	}

	// regenerate neighbors' meshes
	const auto neighbors = get_minis_touching_block(block[0], block[1], block[2]);
	for (auto& neighbor : neighbors) {
		if (neighbor != mini) {
			enqueue_mesh_gen(neighbor, true);
		}
	}

	// regenerate own meshes
	enqueue_mesh_gen(mini, true);

	// finally, add nearby waters to propagation queue
	// TODO: do this smarter?
	schedule_water_propagation(block);
	schedule_water_propagation_neighbors(block);
}

// update meshes
void WorldDataPart::on_block_update(const vmath::ivec3& block) {
	std::shared_ptr<MiniChunk> mini = get_mini_containing_block(block[0], block[1], block[2]);
	vmath::ivec3 mini_coords = get_mini_relative_coords(block[0], block[1], block[2]);
	on_mini_update(mini, block);
}

void WorldDataPart::destroy_block(const int x, const int y, const int z) {
	// update data
	std::shared_ptr<MiniChunk> mini = get_mini_containing_block(x, y, z);
	const vmath::ivec3 mini_coords = get_mini_relative_coords(x, y, z);
	mini->set_block(mini_coords, BlockType::Air);

	// regenerate textures for all neighboring minis (TODO: This should be a maximum of 3 neighbors, since >=3 sides of the destroyed block are facing its own mini.)
	on_mini_update(mini, { x, y, z });
}

void WorldDataPart::destroy_block(const vmath::ivec3& xyz) { return destroy_block(xyz[0], xyz[1], xyz[2]); };

void WorldDataPart::add_block(const int x, const int y, const int z, const BlockType& block) {
	// update data
	std::shared_ptr<MiniChunk> mini = get_mini_containing_block(x, y, z);
	const vmath::ivec3& mini_coords = get_mini_relative_coords(x, y, z);
	mini->set_block(mini_coords, block);

	// regenerate textures for all neighboring minis (TODO: This should be a maximum of 3 neighbors, since the block always has at least 3 sides inside its mini.)
	on_mini_update(mini, { x, y, z });
}

void WorldDataPart::add_block(const vmath::ivec3& xyz, const BlockType& block) { return add_block(xyz[0], xyz[1], xyz[2], block); };

// TODO
Metadata WorldDataPart::get_metadata(const int x, const int y, const int z) {
	Chunk* chunk = get_chunk_containing_block(x, z);

	if (!chunk) {
		return 0;
	}

	const vmath::ivec3 chunk_coords = get_chunk_relative_coordinates(x, y, z);
	return chunk->get_metadata(chunk_coords);
}

Metadata WorldDataPart::get_metadata(const vmath::ivec3& xyz) { return get_metadata(xyz[0], xyz[1], xyz[2]); }
Metadata WorldDataPart::get_metadata(const vmath::ivec4& xyz_) { return get_metadata(xyz_[0], xyz_[1], xyz_[2]); }

// TODO
void WorldDataPart::set_metadata(const int x, const int y, const int z, const Metadata& val) {
	Chunk* chunk = get_chunk_containing_block(x, z);

	if (!chunk) {
		OutputDebugString("Warning: Set metadata for unloaded chunk.\n");
		return;
	}

	vmath::ivec3 chunk_coords = get_chunk_relative_coordinates(x, y, z);
	chunk->set_metadata(chunk_coords, val);
}

void WorldDataPart::set_metadata(const vmath::ivec3& xyz, const Metadata& val) { return set_metadata(xyz[0], xyz[1], xyz[2], val); }
void WorldDataPart::set_metadata(const vmath::ivec4& xyz_, const Metadata& val) { return set_metadata(xyz_[0], xyz_[1], xyz_[2], val); }

// TODO
void WorldDataPart::schedule_water_propagation(const vmath::ivec3& xyz) {
	// push to water propagation priority queue
	water_propagation_queue.push({ current_tick + 5, xyz });
}

void WorldDataPart::schedule_water_propagation_neighbors(const vmath::ivec3& xyz) {
	auto directions = { INORTH, ISOUTH, IEAST, IWEST, IDOWN };
	for (const auto& ddir : directions) {
		schedule_water_propagation(xyz + ddir);
	}
}

// get liquid at (x, y, z) and propagate it
void WorldDataPart::propagate_water(int x, int y, int z) {
	vmath::ivec3 coords = { x, y, z };

	// get chunk which block is in, and if it's unloaded, don't both propagating
	auto chunk = get_chunk_containing_block(x, z);
	if (chunk == nullptr) {
		return;
	}

	// get block at propagation location
	auto block = get_type(x, y, z);

	// if we're air or flowing water, adjust height
	if (block == BlockType::Air || block == BlockType::FlowingWater) {
#ifdef _DEBUG
		char buf[256];
		sprintf(buf, "Propagating water at (%d, %d, %d)\n", x, y, z);
		OutputDebugString(buf);
#endif // _DEBUG

		uint8_t water_level = block == BlockType::FlowingWater ? get_metadata(x, y, z).get_liquid_level() : block == BlockType::StillWater ? 7 : 0;
		uint8_t new_water_level = water_level;

		// if water on top, max height
		auto top_block = get_type(coords + IUP);
		if (top_block == BlockType::StillWater || top_block == BlockType::FlowingWater) {
			// update water level if needed
			new_water_level = 7; // max
			if (new_water_level != water_level) {
				set_type(x, y, z, BlockType::FlowingWater);
				set_metadata(x, y, z, new_water_level);
				schedule_water_propagation_neighbors(coords);
				on_block_update(coords);
			}
			return;
		}

		/* UPDATE WATER LEVEL FOR CURRENT BLOCK BY CHECKING SIDES */

		// record highest water level in side blocks, out of side blocks that are ON a block
		uint8_t highest_side_water = 0;
		auto directions = { INORTH, ISOUTH, IEAST, IWEST };
		for (auto& ddir : directions) {
			// BEAUTIFUL - don't inherit height from nearby water UNLESS it's ON A SOLID BLOCK!
			BlockType under_side_block = get_type(coords + ddir + IDOWN);
			if (under_side_block.is_nonsolid()) {
				continue;
			}

			BlockType side_block = get_type(coords + ddir);

			// if side block is still, its level is max
			if (side_block == BlockType::StillWater) {
				highest_side_water = 7;
				break;
			}

			// if side block is flowing, update highest side water
			else if (side_block == BlockType::FlowingWater) {
				auto side_water_level = get_metadata(coords + ddir).get_liquid_level();
				if (side_water_level > highest_side_water) {
					highest_side_water = side_water_level;
					if (side_water_level == 7) {
						break;
					}
				}
			}
		}

		/* UPDATE WATER LEVEL FOR CURRENT BLOCK IF IT'S CHANGED */

		// update water level if needed
		new_water_level = highest_side_water - 1;
		if (new_water_level != water_level) {
			// if water level in range, set it
			if (0 <= new_water_level && new_water_level <= 7) {
				set_type(x, y, z, BlockType::FlowingWater);
				set_metadata(x, y, z, new_water_level);
				schedule_water_propagation_neighbors(coords);
				on_block_update(coords);
			}
			// otherwise destroy water
			else if (block == BlockType::FlowingWater) {
				set_type(x, y, z, BlockType::Air);
				schedule_water_propagation_neighbors(coords);
				on_block_update(coords);
			}
		}
	}
}

// given water at (x, y, z), find all directions which lead to A shortest path down
// radius = 4
// TODO
std::unordered_set<vmath::ivec3, vecN_hash> WorldDataPart::find_shortest_water_path(int x, int y, int z) {
	vmath::ivec3 coords = { x, y, z };
	assert(get_type(coords + IDOWN).is_solid() && "block under starter block is non-solid!");

	// extract a 9x9 radius of blocks which we can traverse (1) and goals (2)

	constexpr unsigned radius = 4;
	constexpr unsigned invalid_path = 0;
	constexpr unsigned valid_path = 1;
	constexpr unsigned goal = 2;

	static_assert(radius > 0);
	uint8_t extracted[radius * 2 - 1][radius * 2 - 1];
	memset(extracted, invalid_path, sizeof(extracted));

	// for every block in radius
	for (int dx = -radius; dx <= radius; dx++) {
		for (int dz = -radius; dz <= radius; dz++) {
			// if the block is empty
			if (get_type(x + dx, y, z + dz) == BlockType::Air) {
				// if the block below it is solid, it's a valid path to take
				if (get_type(x + dx, y - 1, z + dz).is_solid()) {
					extracted[x + dx][z + dz] = valid_path;
				}
				// if the block below it is non-solid, it's a goal
				else {
					extracted[x + dx][z + dz] = goal;
				}
			}
		}
	}

	// find all shortest paths (1) to goal (2) using bfs
	struct search_item {
		int distance = -1;
		bool reachable_from_west = false;
		bool reachable_from_east = false;
		bool reachable_from_north = false;
		bool reachable_from_south = false;
	};

	search_item search_items[radius * 2 + 1][radius * 2 + 1];

	std::queue<vmath::ivec2> bfs_queue;

	// set very center
	search_items[radius][radius].distance = 0;

	// set items around center
	search_items[radius + 1][radius].distance = 1;
	search_items[radius + 1][radius].reachable_from_west = true;

	search_items[radius - 1][radius].distance = 1;
	search_items[radius - 1][radius].reachable_from_east = true;

	search_items[radius][radius + 1].distance = 1;
	search_items[radius][radius + 1].reachable_from_north = true;

	search_items[radius][radius - 1].distance = 1;
	search_items[radius][radius - 1].reachable_from_south = true;

	// insert items around center into queue
	// TODO: do clever single for loop instead of creating std::vector?
	std::vector<std::pair<int, int>> nearby = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
	for (auto& [dx, dy] : nearby) {
		bfs_queue.push({ dx, dy });
	}

	// store queue items that have shortest distance
	int found_distance = -1;
	std::vector<search_item*> found; // TODO: only store valid directions. Or even better, just have 4 bools, then convert them to coords when returning.

	// perform bfs
	while (!bfs_queue.empty()) {
		// get first item in bfs
		const auto item_coords = bfs_queue.front();
		bfs_queue.pop();
		auto& item = search_items[item_coords[0] + radius][item_coords[1] + radius];
		const auto block_type = extracted[item_coords[0] + radius][item_coords[1] + radius];

		// if invalid location, yeet out
		if (block_type == invalid_path) {
			break;
		}

		// if we've already found, and this is too big, yeet out
		if (found_distance >= 0 && found_distance < item.distance) {
			break;
		}

		// if item is a goal, add it to found
		if (block_type == goal) {
			assert(found_distance == -1 || found_distance == item.distance);
			found_distance = item.distance;
			found.push_back(&item);
		}
		// otherwise, add its neighbors
		else {
			for (auto& [dx, dy] : nearby) {
				// if item_coords + [dx, dy] in range
				if (abs(item_coords[0] + dx) <= radius && abs(item_coords[1] + dy) <= radius) {
					// add it
					bfs_queue.push({ item_coords[0] + dx, item_coords[1] + dy });
				}
			}
		}
	}

	std::unordered_set<vmath::ivec3, vecN_hash> reachable_from_dirs;
	for (auto& result : found) {
		if (result->reachable_from_east) {
			reachable_from_dirs.insert(IEAST);
		}
		if (result->reachable_from_west) {
			reachable_from_dirs.insert(IWEST);
		}
		if (result->reachable_from_north) {
			reachable_from_dirs.insert(INORTH);
		}
		if (result->reachable_from_south) {
			reachable_from_dirs.insert(ISOUTH);
		}
	}

	return reachable_from_dirs;
}

// For a certain corner, get height of flowing water at that corner
float WorldDataPart::get_water_height(const vmath::ivec3& corner) {
#ifdef _DEBUG
	bool any_flowing_water = false;
#endif

	vector<float> water_height_factors;
	water_height_factors.reserve(4);

	for (int i = 0; i < 4; i++) {
		const int dx = (i % 1 == 0) ? 0 : -1; //  0, -1,  0, -1
		const int dz = (i / 2 == 0) ? 0 : -1; //  0,  0, -1, -1

		const vmath::ivec3 block_coords = corner + vmath::ivec3(dx, 0, dz);
		const BlockType block = get_type(block_coords);
		const Metadata metadata = get_metadata(block_coords);

		switch ((BlockType::Value)block) {
		case BlockType::Air:
			water_height_factors.push_back(0);
			break;
		case BlockType::FlowingWater:
#ifdef _DEBUG
			any_flowing_water = true;
#endif
			water_height_factors.push_back(metadata.get_liquid_level());
			break;
		case BlockType::StillWater:
			water_height_factors.push_back(8);
			break;
		default:
			water_height_factors.push_back(7);
			break;
		}
	}

#ifdef _DEBUG
	assert(any_flowing_water && "called get_water_height for a corner without any nearby flowing water!");
#endif

	assert(!water_height_factors.empty());

	return std::accumulate(water_height_factors.begin(), water_height_factors.end(), 0) / water_height_factors.size();
}

void WorldDataPart::handle_messages()
{
	// Receive all messages
	std::vector<zmq::message_t> message;
	auto ret = zmq::recv_multipart(bus.out, std::back_inserter(message), zmq::recv_flags::dontwait);
	while (ret)
	{
		// Get chunk gen response
		if (message[0].to_string_view() == msg::CHUNK_GEN_RESPONSE)
		{
			// Extract result
			ChunkGenResponse* response_ = *message[1].data<ChunkGenResponse*>();
			std::unique_ptr<ChunkGenResponse> response(response_);

#ifdef _DEBUG
			std::stringstream out;
			out << "WorldData: Received chunk for " << vec2str(response->coords) << "\n";
			OutputDebugString(out.str().c_str());
#endif // _DEBUG

			// Update chunk!
			Chunk* chunk = response->chunk.release();

			// make sure it's not a duplicate
			if (get_chunk(chunk->coords))
			{
				OutputDebugStringA("Warn: Duplicate chunk generated.\n");
			}
			else
			{
				add_chunk(response->coords[0], response->coords[1], chunk);
			}

			// Now we must enqueue all minis and neighboring minis for meshing
			for (int i = 0; i < MINIS_PER_CHUNK; i++)
			{
				enqueue_mesh_gen(chunk->minis[i]);
			}

			Chunk* c;
#define ENQUEUE(chunk_ivec2)\
			c = get_chunk(chunk_ivec2);\
			if (c)\
			{\
				for (int i = 0; i < MINIS_PER_CHUNK; i++)\
				{\
					enqueue_mesh_gen(c->minis[i]);\
				}\
			}

			ENQUEUE(chunk->coords + ivec2(1, 0));
			ENQUEUE(chunk->coords + ivec2(-1, 0));
			ENQUEUE(chunk->coords + ivec2(0, 1));
			ENQUEUE(chunk->coords + ivec2(0, -1));
#undef ENQUEUE
		}
#ifdef _DEBUG
		else if (message[0].to_string_view() == msg::TEST)
		{
			std::stringstream s;
			s << "WorldData: " << msg::multi_to_str(message) << "\n";
			OutputDebugString(s.str().c_str());
		}
		else
		{
			std::stringstream s;
			s << "WorldData: Unknown msg [" << message[0].to_string_view() << "]" << "\n";
			OutputDebugString(s.str().c_str());
		}
#endif // _DEBUG

		message.clear();
		ret = zmq::recv_multipart(bus.out, std::back_inserter(message), zmq::recv_flags::dontwait);
	}
}

/**
 * ? Get NEGATIVE? height liquid should be draw at, given negative water level.
 * (I.e. fullness level goes from 0 (full) to 7 (almost empty), and we return a similar ratio.)
 * /
constexpr  float liquid_level_to_height(int liquid_level) {
	// if empty (8 (or more)), height is 0 -- WHY? Shouldn't it be 1?
	if (liquid_level >= 8) {
		liquid_level = 0;
	}

	// liquidLevel is in [0,   7  ]
	// result      is in [1/9, 8/9]
	return (liquid_level + 1) / 9.0f;
}

 float get_liquid_height(int x, int y, int z, BlockType block) {
	int sumDivisor = 0;
	float heightSum = 0.0F;

	// for all blocks around the corner
	for (int i = 0; i < 4; ++i) {

		// (newX, y, newZ) is one block surrounding the corner (x, y, z)
		int newX = x - (i & 1); // goes x, x-1, x, x-1
		int newZ = z - (i >> 1 & 1); // goes z, z, z-1, z-1

		// if same liquid on top, set to max height
		if (get_type(newX, y + 1, newZ) == block) {
			return 1.0f;
		}

		// get material at (newX, y, newZ)
		BlockType newBlock = get_type(newX, y, newZ);

		// if same material as the liquid we're deciding height for,
		if (newBlock == block)
		{
			// get liquid level at (newX, y, newZ)
			// NOTE: liquid level 0 = max, 7 = min.
			int liquidLevel = get_metadata(newX, y, newZ).get_liquid_level();

			// ? sanity check + if minimum level
			if (liquidLevel >= 8 || liquidLevel == 0)
			{
				heightSum += liquid_level_to_height(liquidLevel) * 10.0F;
				sumDivisor += 10;
			}

			heightSum += liquid_level_to_height(liquidLevel);
			++sumDivisor;
		}
		// if newMaterial is different than given material and non-solid (e.g. air or a different liquid)
		else if (!newBlock.isSolid())
		{
			// increase sum/divisor, but have a much smaller effect than when same liquid
			++heightSum;
			++sumDivisor;
		}
	}

	return 1.0F - heightSum / (float)sumDivisor;
}
*/
