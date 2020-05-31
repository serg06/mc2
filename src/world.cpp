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

static constexpr void gen_working_indices(const int& layers_idx, int& working_idx_1, int& working_idx_2) {
	switch (layers_idx) {
	case 0:
		working_idx_1 = 2; // z
		working_idx_2 = 1; // y
		break;
	case 1:
		working_idx_1 = 0; // x
		working_idx_2 = 2; // z
		break;
	case 2:
		working_idx_1 = 0; // x
		working_idx_2 = 1; // y
		break;
	}
	return;
}

bool operator==(const Quad2D& lhs, const Quad2D& rhs) {
	auto& lc1 = lhs.corners[0];
	auto& lc2 = lhs.corners[1];

	auto& rc1 = rhs.corners[0];
	auto& rc2 = rhs.corners[1];

	return
		(lhs.block == rhs.block) &&
		((lc1 == rc1 && lc2 == rc2) || (lc2 == rc1 && lc1 == rc2));
}

// get chunk-coordinates of chunk containing the block at (x, _, z)
vmath::ivec2 get_chunk_coords(const int x, const int z) {
	return get_chunk_coords((float)x, (float)z);
}

// get chunk-coordinates of chunk containing the block at (x, _, z)
vmath::ivec2 get_chunk_coords(const float x, const float z) {
	return { (int)floorf(x / 16.0f), (int)floorf(z / 16.0f) };
}

// get minichunk-coordinates of minichunk containing the block at (x, y, z)
vmath::ivec3 get_mini_coords(const int x, const int y, const int z) {
	return { (int)floorf((float)x / 16.0f), (y / 16) * 16, (int)floorf((float)z / 16.0f) };
}

vmath::ivec3 get_mini_coords(const vmath::ivec3& xyz) { return get_mini_coords(xyz[0], xyz[1], xyz[2]); }

// given a block's real-world coordinates, return that block's coordinates relative to its chunk
vmath::ivec3 get_chunk_relative_coordinates(const int x, const int y, const int z) {
	return vmath::ivec3(posmod(x, CHUNK_WIDTH), y, posmod(z, CHUNK_DEPTH));
}

// given a block's real-world coordinates, return that block's coordinates relative to its mini
vmath::ivec3 get_mini_relative_coords(const int x, const int y, const int z) {
	return vmath::ivec3(posmod(x, MINICHUNK_WIDTH), y % MINICHUNK_HEIGHT, posmod(z, MINICHUNK_DEPTH));
}

vmath::ivec3 get_mini_relative_coords(const vmath::ivec3& xyz) { return get_mini_relative_coords(xyz[0], xyz[1], xyz[2]); }

float intbound(const float s, const float ds)
{
	// Some kind of edge case, see:
	// http://gamedev.stackexchange.com/questions/47362/cast-ray-to-select-block-in-voxel-game#comment160436_49423
	const bool sIsInteger = round(s) == s;
	if (ds < 0 && sIsInteger) {
		return 0;
	}

	return (ds > 0 ? ceil(s) - s : s - floor(s)) / abs(ds);
}

WorldCommonPart::WorldCommonPart(zmq::context_t* const ctx_) : ctx(ctx_), bus_in(msg::ctx, zmq::socket_type::pub), bus_out(msg::ctx, zmq::socket_type::sub) {
	//bus_in.setsockopt(ZMQ_SNDHWM, 1000 * 1000);
	bus_in.connect(addr::MSG_BUS_IN);
	bus_out.setsockopt(ZMQ_SUBSCRIBE, "", 0);
	//bus_out.setsockopt(ZMQ_RCVHWM, 1000 * 1000);
	bus_out.connect(addr::MSG_BUS_OUT);
}

WorldRenderPart::WorldRenderPart(zmq::context_t* ctx_) : WorldCommonPart(ctx_) {}
WorldDataPart::WorldDataPart(zmq::context_t* ctx_) : WorldCommonPart(ctx_) {}
World::World(zmq::context_t* ctx_) : WorldRenderPart(ctx_), WorldDataPart(ctx_) {}

void World::exit() {
	std::vector<zmq::const_buffer> message({
		zmq::buffer(msg::EXIT)
		});

	auto ret = zmq::send_multipart(WorldRenderPart::bus_in, message, zmq::send_flags::dontwait);
	assert(ret);

	// TODO: Completely separate mesh-gen and world, so that I can make App
	//   a non-static variable, so that the destructor is called, so that I
	//   don't have to call these manually.
	WorldRenderPart::bus_in.close();
	WorldRenderPart::bus_out.close();
	WorldDataPart::bus_in.close();
	WorldDataPart::bus_out.close();
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

	auto ret = zmq::send_multipart(bus_in, message, zmq::send_flags::dontwait);
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

// get multiple chunks -- much faster than get_chunk_generate_if_required when n > 1
std::unordered_set<Chunk*, chunk_hash> WorldDataPart::get_chunks_generate_if_required(const vector<vmath::ivec2>& chunk_coords) {
	// don't wanna get duplicates
	std::unordered_set<Chunk*, chunk_hash> result;

	// gen if required
	gen_chunks_if_required(chunk_coords);

	// fetch
	for (auto coords : chunk_coords) {
		result.insert(chunk_map[coords]);
	}

	return result;
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

// generate all chunks (much faster than gen_chunk)
void WorldDataPart::gen_chunks(const vector<vmath::ivec2>& to_generate) {
	std::unordered_set<vmath::ivec2, vecN_hash> set;
	for (auto coords : to_generate) {
		set.insert(coords);
	}
	return gen_chunks(set);
}

// generate all chunks (much faster than gen_chunk)
void WorldDataPart::gen_chunks(const std::unordered_set<vmath::ivec2, vecN_hash>& to_generate) {
	// get pointers ready
	vector<Chunk*> chunks(to_generate.size());

	// generate chunks and set pointers
	int i = 0;
	for (auto coords : to_generate) {
		// generate it
		Chunk* c = new Chunk(coords);
		c->generate();

		// add it to world so that we can use get_type in following code
		add_chunk(coords[0], coords[1], c);

		// add it to our pointers
		chunks[i] = c;

		i++;
	}

	// chunks we need to generate minis for
	std::unordered_set<Chunk*, chunk_hash> to_generate_minis;

	// for every chunk we just generated
	for (int i = 0; i < to_generate.size(); i++) {
		// get chunk
		Chunk* chunk = chunks[i];

		// need to generate minis for it
		to_generate_minis.insert(chunk);

		// and need to regenerate minis for its neighbors
		for (auto coords : chunk->surrounding_chunks()) {
			// get the neighbor
			Chunk* neighbor = get_chunk(coords[0], coords[1]);

			// if neighbor exists, add it to lists of chunks we need to regenerate minis in
			if (neighbor != nullptr) {
				to_generate_minis.insert(neighbor);
			}
		}
	}

	// figure out all minis to mesh
	vector<std::shared_ptr<MiniChunk>> minis_to_mesh;

	for (auto chunk : to_generate_minis) {
		for (auto& mini : chunk->minis) {
			minis_to_mesh.push_back(mini);
		}
	}

	// add them all to queue
	for (auto& mini : minis_to_mesh) {
		enqueue_mesh_gen(mini);
	}
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

// get mini render component or nullptr
std::shared_ptr<MiniRender> WorldRenderPart::get_mini_render_component(const int x, const int y, const int z) {
	const auto search = mesh_map.find({ x, y, z });

	// if chunk doesn't exist, return null
	if (search == mesh_map.end()) {
		return nullptr;
	}

	return search->second;
}

std::shared_ptr<MiniRender> WorldRenderPart::get_mini_render_component(const vmath::ivec3& xyz) { return get_mini_render_component(xyz[0], xyz[1], xyz[2]); }

// get mini render component or nullptr
std::shared_ptr<MiniRender> WorldRenderPart::get_mini_render_component_or_generate(const int x, const int y, const int z) {
	std::shared_ptr<MiniRender> result = get_mini_render_component(x, y, z);
	if (result == nullptr)
	{
		result = std::make_shared<MiniRender>();
		result->set_coords({ x, y, z });
		mesh_map[{x, y, z}] = result;
	}

	return result;
}

std::shared_ptr<MiniRender> WorldRenderPart::get_mini_render_component_or_generate(const vmath::ivec3& xyz) { return get_mini_render_component_or_generate(xyz[0], xyz[1], xyz[2]); }

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
vector<std::shared_ptr<MiniChunk>> WorldDataPart::get_minis_touching_block(const int x, const int y, const int z) {
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

bool WorldRenderPart::check_if_covered(std::shared_ptr<MeshGenRequest> req) {
	// if contains any translucent blocks, don't know how to handle that yet
	// TODO?
	if (req->data->self->any_translucent()) {
		return false;
	}

	// none are air, so only check outside blocks
	for (int miniY = 0; miniY < MINICHUNK_HEIGHT; miniY++) {
		for (int miniZ = 0; miniZ < CHUNK_DEPTH; miniZ++) {
			for (int miniX = 0; miniX < CHUNK_WIDTH; miniX++) {
				const auto& mini_coords = req->data->self->get_coords();
				const vmath::ivec3 coords = { mini_coords[0] * CHUNK_WIDTH + miniX, mini_coords[1] + miniY,  mini_coords[2] * CHUNK_DEPTH + miniZ };

				// if along east wall, check east
				if (miniX == CHUNK_WIDTH - 1) {
					if (req->data->east && req->data->east->get_block(get_mini_relative_coords(coords)).is_translucent()) return false;
				}
				// if along west wall, check west
				if (miniX == 0) {
					if (req->data->west && req->data->west->get_block(get_mini_relative_coords(coords)).is_translucent()) return false;
				}

				// if along north wall, check north
				if (miniZ == 0) {
					if (req->data->north && req->data->north->get_block(get_mini_relative_coords(coords)).is_translucent()) return false;
				}
				// if along south wall, check south
				if (miniZ == CHUNK_DEPTH - 1) {
					if (req->data->south && req->data->south->get_block(get_mini_relative_coords(coords)).is_translucent()) return false;
				}

				// if along bottom wall, check bottom
				if (miniY == 0) {
					if (req->data->down && req->data->down->get_block(get_mini_relative_coords(coords)).is_translucent()) return false;
				}
				// if along top wall, check top
				if (miniY == MINICHUNK_HEIGHT - 1) {
					if (req->data->up && req->data->up->get_block(get_mini_relative_coords(coords)).is_translucent()) return false;
				}
			}
		}
	}

	return true;
}

void WorldRenderPart::update_meshes()
{
	// Receive all mesh-gen results
	std::vector<zmq::message_t> message;
	auto ret = zmq::recv_multipart(bus_out, std::back_inserter(message), zmq::recv_flags::dontwait);
	while (ret)
	{
		// TODO: Filter
		if (message[0].to_string_view() == msg::MESH_GEN_RESPONSE)
		{
			// Extract result
			MeshGenResult* mesh_ = *message[1].data<MeshGenResult*>();
			std::unique_ptr<MeshGenResult> mesh(mesh_);

			// Update mesh!
			std::shared_ptr<MiniRender> mini = get_mini_render_component_or_generate(mesh->coords);
			mini->set_mesh(std::move(mesh->mesh));
			mini->set_water_mesh(std::move(mesh->water_mesh));
		}

		message.clear();
		ret = zmq::recv_multipart(bus_out, std::back_inserter(message), zmq::recv_flags::dontwait);
	}
}

void WorldRenderPart::render(OpenGLInfo* glInfo, GlfwInfo* windowInfo, const vmath::vec4(&planes)[6], const vmath::ivec3& staring_at) {
	// collect all the minis we're gonna draw
	vector<MiniRender*> minis_to_draw;

	for (auto& [coords, mini] : mesh_map)
	{
		if (!mini->get_invisible())
		{
			if (mini_in_frustum(mini.get(), planes))
			{
				minis_to_draw.push_back(mini.get());
			}
		}
	}

	if (minis_to_draw.size() == 0) return;

	// draw them
	glUseProgram(glInfo->game_rendering_program);

	//glBindFramebuffer(GL_DRAW_FRAMEBUFFER, glInfo->fbo_out.get_fbo());

	// Bind and clear terrain buffer
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, glInfo->fbo_terrain.get_fbo());
	const GLfloat one = 1.0f;
	glClearBufferfv(GL_COLOR, 0, color_sky_blue);
	glClearBufferfv(GL_DEPTH, 0, &one);
	glEnable(GL_BLEND);

	// draw terrain
	for (auto& mini : minis_to_draw) {
		mini->render_meshes(glInfo);
	}

	// highlight block
	if (staring_at[1] >= 0) {
		highlight_block(glInfo, windowInfo, staring_at);
	}

	// Bind and clear water buffer
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, glInfo->fbo_water.get_fbo());
	glClearBufferfv(GL_COLOR, 0, color_sky_blue);
	glClearBufferfv(GL_DEPTH, 0, &one);
	glDisable(GL_BLEND); // DEBUG

	// draw water onto water fbo
	for (auto& mini : minis_to_draw) {
		mini->render_water_meshes(glInfo);
	}

	// merge water fbo onto terrain fbo
	merge_fbos(glInfo, glInfo->fbo_terrain.get_fbo(), glInfo->fbo_water);

	// send it all to output fbo
	glBlitNamedFramebuffer(glInfo->fbo_terrain.get_fbo(), glInfo->fbo_out.get_fbo(), 0, 0, glInfo->fbo_out.get_width(), glInfo->fbo_out.get_height(), 0, 0, glInfo->fbo_out.get_width(), glInfo->fbo_out.get_height(), GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);

	// re-bind output fbo
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, glInfo->fbo_out.get_fbo());

	rendered++;
}

// check if a mini is visible in a frustum
bool WorldRenderPart::mini_in_frustum(const MiniRender* mini, const vmath::vec4(&planes)[6]) {
	return sphere_in_frustrum(mini->center_coords_v3(), FRUSTUM_MINI_RADIUS_ALLOWANCE, planes);
}

// convert 2D quads to 3D quads
// face: for offset
vector<Quad3D> WorldRenderPart::quads_2d_3d(const vector<Quad2D>& quads2d, const int layers_idx, const int layer_no, const vmath::ivec3& face) {
	vector<Quad3D> result(quads2d.size());

	// working variable

	// most efficient to traverse working_idx_1 then working_idx_2;
	int working_idx_1, working_idx_2;
	gen_working_indices(layers_idx, working_idx_1, working_idx_2);

	// for each quad
	for (int i = 0; i < quads2d.size(); i++) {
		auto& quad3d = result[i];
		auto& quad2d = quads2d[i];

		// set block
		quad3d.block = (uint8_t)quad2d.block;

		// convert both corners to 3D coordinates
		quad3d.corner1[layers_idx] = layer_no;
		quad3d.corner1[working_idx_1] = quad2d.corners[0][0];
		quad3d.corner1[working_idx_2] = quad2d.corners[0][1];

		quad3d.corner2[layers_idx] = layer_no;
		quad3d.corner2[working_idx_1] = quad2d.corners[1][0];
		quad3d.corner2[working_idx_2] = quad2d.corners[1][1];

		// set face
		quad3d.face = face;

		// set metadata
		quad3d.metadata = quad2d.metadata;
	}

	return result;
}

// generate layer by grabbing face blocks directly from the minichunk
void WorldRenderPart::gen_layer_generalized(const std::shared_ptr<MiniChunk> mini, const std::shared_ptr<MiniChunk> face_mini, const int layers_idx, const int layer_no, const vmath::ivec3 face, BlockType(&result)[16][16]) {
	// most efficient to traverse working_idx_1 then working_idx_2;
	int working_idx_1, working_idx_2;
	gen_working_indices(layers_idx, working_idx_1, working_idx_2);

	// coordinates of current block
	vmath::ivec3 coords = { 0, 0, 0 };
	coords[layers_idx] = layer_no;

	// reset all to air
	memset(result, (uint8_t)BlockType::Air, sizeof(result));
	if (!mini)
	{
		OutputDebugString("Uh oh 3501\n");
		return;
	}

	// for each coordinate
	for (int v = 0; v < 16; v++) {
		for (int u = 0; u < 16; u++) {
			coords[working_idx_1] = u;
			coords[working_idx_2] = v;

			// get block at these coordinates
			const BlockType block = mini->get_block(coords);

			// skip air blocks
			if (block == BlockType::Air) {
				continue;
			}

			// no face mini => face is air
			else if (face_mini == nullptr) {
				result[u][v] = block;
			}

			// face mini exists
			else {
				// get face block
				vmath::ivec3 face_coords = coords + face;
				face_coords[layers_idx] = (face_coords[layers_idx] + 16) % 16;
				const BlockType face_block = face_mini->get_block(face_coords);

				// if block's face is visible, set it
				if (is_face_visible(block, face_block)) {
					result[u][v] = block;
				}
			}
		}
	}
}

bool WorldRenderPart::is_face_visible(const BlockType& block, const BlockType& face_block) {
	return face_block.is_transparent() || (block != BlockType::StillWater && block != BlockType::FlowingWater && face_block.is_translucent()) || (face_block.is_translucent() && !block.is_translucent());
}

void WorldRenderPart::gen_layer(const std::shared_ptr<MeshGenRequest> req, const int layers_idx, const int layer_no, const vmath::ivec3& face, BlockType(&result)[16][16]) {
	// get coordinates of a random block
	vmath::ivec3 coords = { 0, 0, 0 };
	coords[layers_idx] = layer_no;
	const vmath::ivec3 face_coords = coords + face;

	// figure out which mini has our face layer (usually ours)
	std::shared_ptr<MiniChunk> mini = req->data->self;
	std::shared_ptr<MiniChunk> face_mini = nullptr;
	if (in_range(face_coords, vmath::ivec3(0, 0, 0), vmath::ivec3(15, 15, 15))) {
		face_mini = mini;
	}
	else {
		const auto face_mini_coords = mini->get_coords() + (layers_idx == 1 ? face * 16 : face);

#define SET_COORDS(ATTR)\
			if (face_mini == nullptr && req->data->ATTR && req->data->ATTR->get_coords() == face_mini_coords)\
			{\
				face_mini = req->data->ATTR;\
			}

		SET_COORDS(up);
		SET_COORDS(down);
		SET_COORDS(north);
		SET_COORDS(south);
		SET_COORDS(east);
		SET_COORDS(west);
#undef SET_COORDS
	}

	// generate layer
	gen_layer_generalized(mini, face_mini, layers_idx, layer_no, face, result);
}

// given 2D array of block numbers, generate optimal quads
vector<Quad2D> WorldRenderPart::gen_quads(const BlockType(&layer)[16][16], /* const Metadata(&metadata_layer)[16][16], */ bool(&merged)[16][16]) {
	memset(merged, false, sizeof(merged));

	vector<Quad2D> result;

	for (int i = 0; i < 16; i++) {
		for (int j = 0; j < 16; j++) {
			// skip merged blocks
			if (merged[i][j]) continue;

			const BlockType block = layer[i][j];

			// skip air
			if (block == BlockType::Air) continue;

			// get max size of this quad
			const vmath::ivec2 max_size = get_max_size(layer, merged, { i, j }, block);

			// add it to results
			const vmath::ivec2 start = { i, j };
			Quad2D q;
			q.block = block;
			q.corners[0] = start;
			q.corners[1] = start + max_size;
			//if (block == BlockType::FlowingWater) {
			//	q.metadata = metadata_layer[i][j];
			//}

			// mark all as merged
			mark_as_merged(merged, start, max_size);

			// wew
			result.push_back(q);
		}
	}

	return result;
}

void WorldRenderPart::mark_as_merged(bool(&merged)[16][16], const vmath::ivec2& start, const vmath::ivec2& max_size) {
	for (int i = start[0]; i < start[0] + max_size[0]; i++) {
		for (int j = start[1]; j < start[1] + max_size[1]; j++) {
			merged[i][j] = true;
		}
	}
}

// given a layer and start point, find its best dimensions
vmath::ivec2 WorldRenderPart::get_max_size(const BlockType(&layer)[16][16], const bool(&merged)[16][16], const vmath::ivec2& start_point, const BlockType& block_type) {
	assert(block_type != BlockType::Air);
	assert(!merged[start_point[0]][start_point[1]] && "bruh");

	// TODO: Search width with find() instead of a for loop?

	// max width and height
	vmath::ivec2 max_size = { 1, 1 };

	// no meshing of flowing water -- TODO: allow meshing if max height? (I.e. if it's flowing straight down.)
	if (block_type == BlockType::FlowingWater) {
		return max_size;
	}

	// maximize height first, because it's better memory-wise
	for (int j = start_point[1] + 1, i = start_point[0]; j < 16; j++) {
		// if extended by 1, add 1 to max height
		if (layer[i][j] == block_type && !merged[i][j]) {
			max_size[1]++;
		}
		// else give up
		else {
			break;
		}
	}

	// maximize width now that we've maximized height

	// for each width
	bool stop = false;
	for (int i = start_point[0] + 1; i < 16; i++) {
		// check if entire height is correct
		for (int j = start_point[1]; j < start_point[1] + max_size[1]; j++) {
			// if wrong block type, give up on extending width
			if (layer[i][j] != block_type || merged[i][j]) {
				stop = true;
				break;
			}
		}

		if (stop) {
			break;
		}

		// yep, entire height is correct! Extend max width and keep going
		max_size[0]++;
	}

	return max_size;
}

void WorldRenderPart::highlight_block(const OpenGLInfo* glInfo, const GlfwInfo* windowInfo, const int x, const int y, const int z) {
	// Figure out mini-relative quads
	Quad3D quads[6];

	const vmath::ivec3 mini_coords = get_mini_coords(x, y, z);

	const vmath::ivec3 relative_coords = get_chunk_relative_coordinates(x, y, z);
	const vmath::ivec3 block_coords = { relative_coords[0], y % 16, relative_coords[2] };

	for (int i = 0; i < 6; i++) {
		quads[i].block = BlockType::Outline; // outline
		quads[i].lighting = 0; // TODO: set to max instead?
		quads[i].metadata = 0;
	}

	// SOUTH
	//	bottom-left corner
	quads[0].corner1 = block_coords + vmath::ivec3(1, 0, 1);
	//	top-right corner
	quads[0].corner2 = block_coords + vmath::ivec3(0, 1, 1);
	quads[0].face = vmath::ivec3(0, 0, 1);

	// NORTH
	//	bottom-left corner
	quads[1].corner1 = block_coords + vmath::ivec3(0, 0, 0);
	//	top-right corner
	quads[1].corner2 = block_coords + vmath::ivec3(1, 1, 0);
	quads[1].face = vmath::ivec3(0, 0, -1);

	// EAST
	//	bottom-left corner
	quads[2].corner1 = block_coords + vmath::ivec3(1, 1, 1);
	//	top-right corner
	quads[2].corner2 = block_coords + vmath::ivec3(1, 0, 0);
	quads[2].face = vmath::ivec3(1, 0, 0);

	// WEST
	//	bottom-left corner
	quads[3].corner1 = block_coords + vmath::ivec3(0, 1, 0);
	//	top-right corner
	quads[3].corner2 = block_coords + vmath::ivec3(0, 0, 1);
	quads[3].face = vmath::ivec3(-1, 0, 0);

	// UP
	//	bottom-left corner
	quads[4].corner1 = block_coords + vmath::ivec3(0, 1, 0);
	//	top-right corner
	quads[4].corner2 = block_coords + vmath::ivec3(1, 1, 1);
	quads[4].face = vmath::ivec3(0, 1, 0);

	// DOWN
	//	bottom-left corner
	quads[5].corner1 = block_coords + vmath::ivec3(0, 0, 1);
	//	top-right corner
	quads[5].corner2 = block_coords + vmath::ivec3(1, 0, 0);
	quads[5].face = vmath::ivec3(0, -1, 0);

	GLuint quad_data_buf;
	GLuint mini_coords_buf;

	// create buffers
	glCreateBuffers(1, &quad_data_buf);
	glCreateBuffers(1, &mini_coords_buf);

	// allocate them just enough space
	glNamedBufferStorage(quad_data_buf, sizeof(Quad3D) * 6, quads, NULL);
	glNamedBufferStorage(mini_coords_buf, sizeof(vmath::ivec3), mini_coords, NULL);

	// quad VAO
	glBindVertexArray(glInfo->vao_quad);

	// bind to quads attribute binding point
	glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->quad_data_bidx, quad_data_buf, 0, sizeof(Quad3D));
	glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->q_base_coords_bidx, mini_coords_buf, 0, sizeof(vmath::ivec3));

	// save properties before we overwrite them
	GLint polygon_mode; glGetIntegerv(GL_POLYGON_MODE, &polygon_mode);
	const GLint cull_face = glIsEnabled(GL_CULL_FACE);
	const GLint depth_test = glIsEnabled(GL_DEPTH_TEST);

	// Update projection matrix (increase near distance a bit, to fix z-fighting)
	mat4 proj_matrix = perspective(
		(float)windowInfo->vfov,
		(float)windowInfo->width / (float)windowInfo->height,
		(PLAYER_HEIGHT - CAMERA_HEIGHT) * 1.001f / sqrtf(2.0f), // render outline a tiny bit closer than actual block, to prevent z-fighting
		64 * CHUNK_WIDTH
	);
	glNamedBufferSubData(glInfo->trans_uni_buf, sizeof(vmath::mat4), sizeof(proj_matrix), proj_matrix); // proj matrix

	// DRAW!
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	// glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	glDrawArrays(GL_POINTS, 0, 6);

	// restore original properties
	if (cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
	if (depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, polygon_mode);

	proj_matrix = perspective(
		(float)windowInfo->vfov,
		(float)windowInfo->width / (float)windowInfo->height,
		(PLAYER_HEIGHT - CAMERA_HEIGHT) * 1 / sqrtf(2.0f), // back to normal
		64 * CHUNK_WIDTH
	);
	glNamedBufferSubData(glInfo->trans_uni_buf, sizeof(vmath::mat4), sizeof(proj_matrix), proj_matrix); // proj matrix

	// unbind VAO jic
	glBindVertexArray(0);

	// DELETE
	glDeleteBuffers(1, &quad_data_buf);
	glDeleteBuffers(1, &mini_coords_buf);
}

void WorldRenderPart::highlight_block(const OpenGLInfo* glInfo, const GlfwInfo* windowInfo, const vmath::ivec3& xyz) { return highlight_block(glInfo, windowInfo, xyz[0], xyz[1], xyz[2]); }

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
// stop_check = function that decides if we stop the raycast or not
const void WorldDataPart::raycast(const vec4& origin, const vec4& direction, int radius, vmath::ivec3* result_coords, vmath::ivec3* result_face, const std::function <bool(const vmath::ivec3& coords, const vmath::ivec3& face)>& stop_check) {
	// From "A Fast Voxel Traversal Algorithm for Ray Tracing"
	// by John Amanatides and Andrew Woo, 1987
	// <http://www.cse.yorku.ca/~amana/research/grid.pdf>
	// <http://citeseer.ist.psu.edu/viewdoc/summary?doi=10.1.1.42.3443>
	// Extensions to the described algorithm:
	//   � Imposed a distance limit.
	//   � The face passed through to reach the current cube is provided to
	//     the callback.

	// The foundation of this algorithm is a parameterized representation of
	// the provided ray,
	//                    origin + t * direction,
	// except that t is not actually stored; rather, at any given point in the
	// traversal, we keep track of the *greater* t values which we would have
	// if we took a step sufficient to cross a cube boundary along that axis
	// (i.e. change the integer part of the coordinate) in the variables
	// tMaxX, tMaxY, and tMaxZ

	// Cube containing origin point.
	int x = (int)floorf(origin[0]);
	int y = (int)floorf(origin[1]);
	int z = (int)floorf(origin[2]);
	// Break out direction vector.
	float dx = direction[0];
	float dy = direction[1];
	float dz = direction[2];
	// Direction to increment x,y,z when stepping.
	// TODO: Consider case where sgn = 0.
	int stepX = sgn(dx);
	int stepY = sgn(dy);
	int stepZ = sgn(dz);
	// See description above. The initial values depend on the fractional
	// part of the origin.
	float tMaxX = intbound(origin[0], dx);
	float tMaxY = intbound(origin[1], dy);
	float tMaxZ = intbound(origin[2], dz);
	// The change in t when taking a step (always positive).
	float tDeltaX = stepX / dx;
	float tDeltaY = stepY / dy;
	float tDeltaZ = stepZ / dz;
	// Buffer for reporting faces to the callback.
	vmath::ivec3 face = { 0, 0, 0 };

	// Avoids an infinite loop.
	if (dx == 0 && dy == 0 && dz == 0) {
		throw "Raycast in zero direction!";
	}

	// Rescale from units of 1 cube-edge to units of 'direction' so we can
	// compare with 't'.

	radius /= sqrt(dx * dx + dy * dy + dz * dz);

	//while (/* ray has not gone past bounds of world */
	//	(stepX > 0 ? x < wx : x >= 0) &&
	//	(stepY > 0 ? y < wy : y >= 0) &&
	//	(stepZ > 0 ? z < wz : z >= 0)) {

	while (/* ray has not gone past bounds of radius */
		true) {

		// Invoke the callback, unless we are not *yet* within the bounds of the
		// world.
		//if (!(x < 0 || y < 0 || z < 0 /*|| x >= wx || y >= wy || z >= wz*/)) {

		//}

		// success, set result values and return
		if (stop_check({ x, y, z }, face)) {
			*result_coords = { x, y, z };
			*result_face = face;
			return;
		}

		// tMaxX stores the t-value at which we cross a cube boundary along the
		// X axis, and similarly for Y and Z. Therefore, choosing the least tMax
		// chooses the closest cube boundary. Only the first case of the four
		// has been commented in detail.
		if (tMaxX < tMaxY) {
			if (tMaxX < tMaxZ) {
				if (tMaxX > radius) break;
				// Update which cube we are now in.
				x += stepX;
				// Adjust tMaxX to the next X-oriented boundary crossing.
				tMaxX += tDeltaX;
				// Record the normal vector of the cube face we entered.
				face[0] = -stepX;
				face[1] = 0;
				face[2] = 0;
			}
			else {
				if (tMaxZ > radius) break;
				z += stepZ;
				tMaxZ += tDeltaZ;
				face[0] = 0;
				face[1] = 0;
				face[2] = -stepZ;
			}
		}
		else {
			if (tMaxY < tMaxZ) {
				if (tMaxY > radius) break;
				y += stepY;
				tMaxY += tDeltaY;
				face[0] = 0;
				face[1] = -stepY;
				face[2] = 0;
			}
			else {
				// Identical to the second case, repeated for simplicity in
				// the conditionals.
				if (tMaxZ > radius) break;
				z += stepZ;
				tMaxZ += tDeltaZ;
				face[0] = 0;
				face[1] = 0;
				face[2] = -stepZ;
			}
		}
	}
	// nothing found, set invalid results and return
	*result_coords = { 0, -1, 0 }; // invalid
	*result_face = { 0, 0, 0 }; // invalid
	return;
}

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

MeshGenResult* WorldRenderPart::gen_minichunk_mesh_from_req(std::shared_ptr<MeshGenRequest> req) {
	// update invisibility
	bool invisible = req->data->self->all_air() || check_if_covered(req);

	// if visible, update mesh
	std::unique_ptr<MiniChunkMesh> non_water;
	std::unique_ptr<MiniChunkMesh> water;
	if (!invisible) {
		const std::unique_ptr<MiniChunkMesh> mesh = gen_minichunk_mesh(req);

		non_water = std::make_unique<MiniChunkMesh>();
		water = std::make_unique<MiniChunkMesh>();

		for (auto& quad : mesh->get_quads()) {
			if ((BlockType)quad.block == BlockType::StillWater || (BlockType)quad.block == BlockType::FlowingWater) {
				water->add_quad(quad);
			}
			else {
				non_water->add_quad(quad);
			}
		}

		assert(mesh->size() == non_water->size() + water->size());
	}

	// post result
	MeshGenResult* result = nullptr;
	if (non_water || water)
	{
		result = new MeshGenResult(req->data->self->get_coords(), invisible, std::move(non_water), std::move(water));
	}

	// generated result
	return result;
}

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


std::unique_ptr<MiniChunkMesh> WorldRenderPart::gen_minichunk_mesh(std::shared_ptr<MeshGenRequest> req) {
	// got our mesh
	std::unique_ptr<MiniChunkMesh> mesh = std::make_unique<MiniChunkMesh>();

	// for all 6 sides
	for (int i = 0; i < 6; i++) {
		bool backface = i < 3;
		int layers_idx = i % 3;

		// most efficient to traverse working_idx_1 then working_idx_2;
		int working_idx_1, working_idx_2;
		gen_working_indices(layers_idx, working_idx_1, working_idx_2);

		// generate face variable
		vmath::ivec3 face = { 0, 0, 0 };
		// I don't think it matters whether we start with front or back face, as long as we switch halfway through.
		// BACKFACE => +X/+Y/+Z SIDE. 
		face[layers_idx] = backface ? -1 : 1;

		// for each layer
		for (int i = 0; i < 16; i++) {
			BlockType layer[16][16];
			bool merged[16][16];

			// extract it from the data
			gen_layer(req, layers_idx, i, face, layer);

			// get quads from layer
			vector<Quad2D> quads2d = gen_quads(layer, merged);

			// if -x, -y, or +z, flip triangles around so that we're not drawing them backwards
			if (face[0] < 0 || face[1] < 0 || face[2] > 0) {
				for (auto& quad2d : quads2d) {
					vmath::ivec2 diffs = quad2d.corners[1] - quad2d.corners[0];
					quad2d.corners[0][0] += diffs[0];
					quad2d.corners[1][0] -= diffs[0];
				}
			}

			// TODO: rotate texture sides the correct way. (It's noticeable when placing down diamond block.)
			// -> Or alternatively, can just rotate texture lmao.

			// convert quads back to 3D coordinates
			vector<Quad3D> quads = quads_2d_3d(quads2d, layers_idx, i, face);

			// if not backface (i.e. not facing (0,0,0)), move 1 forwards
			if (face[0] > 0 || face[1] > 0 || face[2] > 0) {
				for (auto& quad : quads) {
					quad.corner1 += face;
					quad.corner2 += face;
				}
			}

			// append quads
			for (auto quad : quads) {
				mesh->add_quad(quad);
			}
		}
	}

	return mesh;
}









/*************************************************************/
/* PLACING TESTS IN HERE UNTIL I LEARN HOW TO DO IT PROPERLY */
/*************************************************************/


#include "minichunk.h"
#include <chrono>

namespace WorldTests {
	void run_all_tests(OpenGLInfo* glInfo) {
		test_gen_quads();
		test_mark_as_merged();
		test_get_max_size();
		//test_gen_layer();
		OutputDebugString("WorldTests completed successfully.\n");
	}

	//void test_gen_layer() {
	//	// gen chunk at 0,0
	//	Chunk* chunk = new Chunk({ 0, 0 });
	//	chunk->generate();

	//	// grab first mini that has stone, grass, and air blocks
	//	MiniChunk* mini;
	//	for (auto &mini2 : chunk->minis) {
	//		bool has_air = false, has_stone = false, has_grass = false;
	//		for (int i = 0; i < MINICHUNK_SIZE; i++) {
	//			has_air |= mini2.blocks[i] == BlockType::Air;
	//			has_stone |= mini2.blocks[i] == BlockType::Stone;
	//			has_grass |= mini2.blocks[i] == BlockType::Grass;
	//		}

	//		if (has_air && has_stone && has_grass) {
	//			mini = &mini2;
	//		}
	//	}

	//	if (mini == nullptr) {
	//		throw "No sufficient minis!";
	//	}

	//	// grab 2nd layer facing us in z direction
	//	BlockType result[16][16];
	//	int z = 1;
	//	vmath::ivec3 face = { 0, 0, -1 };

	//	for (int x = 0; x < 16; x++) {
	//		for (int y = 0; y < 16; y++) {
	//			// set working indices (TODO: move u to outer loop)
	//			vmath::ivec3 coords = { x, y, z };

	//			// get block at these coordinates
	//			BlockType block = mini->get_block(coords);

	//			// dgaf about air blocks
	//			if (block == BlockType::Air) {
	//				continue;
	//			}

	//			// get face block
	//			BlockType face_block = mini->get_block(coords + face);

	//			// if block's face is visible, set it
	//			if (World::is_face_visible(block, face_block)) {
	//				result[x][y] = block;
	//			}
	//		}
	//	}

	//	// Do the same with gen_layer_fast
	//	BlockType expected[16][16];
	//	World::gen_layer_generalized(mini, mini, 2, 1, face, expected);

	//	// Make sure they're the same
	//	for (int x = 0; x < 16; x++) {
	//		for (int y = 0; y < 16; y++) {
	//			if (result[x][y] != expected[x][y]) {
	//				throw "It broke!";
	//			}
	//		}
	//	}

	//	// clear
	//	chunk->clear();
	//}

	void test_gen_quads() {
		// create layer of all air
		BlockType layer[16][16];
		memset(layer, (uint8_t)BlockType::Air, 16 * 16 * sizeof(BlockType));

		// 1. Add rectangle from (1,3) to (3,6)
		for (int i = 1; i <= 3; i++) {
			for (int j = 3; j <= 6; j++) {
				layer[i][j] = BlockType::Stone;
			}
		}
		// expected result
		Quad2D q1;
		q1.block = BlockType::Stone;
		q1.corners[0] = { 1, 3 };
		q1.corners[1] = { 4, 7 };

		// 2. Add plus symbol - line from (7,5)->(7,9), and (5,7)->(9,7)
		for (int i = 7; i <= 7; i++) {
			for (int j = 5; j <= 9; j++) {
				layer[i][j] = BlockType::Stone;
			}
		}
		for (int i = 5; i <= 9; i++) {
			for (int j = 7; j <= 7; j++) {
				layer[i][j] = BlockType::Stone;
			}
		}
		// expected result
		vector<Quad2D> vq2;
		Quad2D q2;
		q2.block = BlockType::Stone;

		q2.corners[0] = { 5, 7 };
		q2.corners[1] = { 10, 8 };
		vq2.push_back(q2);

		q2.corners[0] = { 7, 5 };
		q2.corners[1] = { 8, 7 };
		vq2.push_back(q2);

		q2.corners[0] = { 7, 8 };
		q2.corners[1] = { 8, 10 };
		vq2.push_back(q2);

		// Finally, add line all along bottom
		for (int i = 0; i <= 15; i++) {
			for (int j = 15; j <= 15; j++) {
				layer[i][j] = BlockType::Grass;
			}
		}
		// expected result
		Quad2D q3;
		q3.block = BlockType::Grass;
		q3.corners[0] = { 0, 15 };
		q3.corners[1] = { 16, 16 };

		bool merged[16][16];
		vector<Quad2D> result = World::gen_quads(layer, merged);

		assert(result.size() == 5 && "wrong number of results");
		assert(find(begin(result), end(result), q1) != end(result) && "q1 not in results list");
		for (auto q : vq2) {
			assert(find(begin(result), end(result), q) != end(result) && "q2's element not in results list");
		}
		assert(find(begin(result), end(result), q3) != end(result) && "q3 not in results list");
	}

	void test_mark_as_merged() {
		bool merged[16][16];
		memset(merged, 0, 16 * 16 * sizeof(bool));

		vmath::ivec2 start = { 3, 4 };
		vmath::ivec2 max_size = { 2, 5 };

		World::mark_as_merged(merged, start, max_size);

		for (int i = 0; i < 16; i++) {
			for (int j = 0; j < 16; j++) {
				// if in right x-range and y-range
				if (start[0] <= i && i <= start[0] + max_size[0] - 1 && start[1] <= j && j <= start[1] + max_size[1] - 1) {
					// make sure merged
					if (!merged[i][j]) {
						throw "not merged when should be!";
					}
				}
				// else make sure not merged
				else {
					if (merged[i][j]) {
						throw "merged when shouldn't be!";
					}
				}
			}
		}
	}

	void test_get_max_size() {
		return; // TODO: fix

		// create layer of all air
		BlockType layer[16][16];
		memset(layer, (uint8_t)BlockType::Air, 16 * 16 * sizeof(BlockType));

		// let's say nothing is merged yet
		bool merged[16][16];
		memset(merged, false, 16 * 16 * sizeof(bool));

		// 1. Add rectangle from (1,3) to (3,6)
		for (int i = 1; i <= 3; i++) {
			for (int j = 3; j <= 6; j++) {
				layer[i][j] = BlockType::Stone;
			}
		}

		// 2. Add plus symbol - line from (7,5)->(7,9), and (5,7)->(9,7)
		for (int i = 7; i <= 7; i++) {
			for (int j = 5; j <= 9; j++) {
				layer[i][j] = BlockType::Stone;
			}
		}
		for (int i = 5; i <= 9; i++) {
			for (int j = 7; j <= 7; j++) {
				layer[i][j] = BlockType::Stone;
			}
		}

		// 3. Add line all along bottom
		for (int i = 0; i <= 15; i++) {
			for (int j = 15; j <= 15; j++) {
				layer[i][j] = BlockType::Grass;
			}
		}

		// Get max size for rectangle top-left-corner
		vmath::ivec2 max_size1 = World::get_max_size(layer, merged, { 1, 3 }, BlockType::Stone);
		if (max_size1[0] != 3 || max_size1[1] != 4) {
			throw "wrong max_size1";
		}

		// Get max size for plus center
		vmath::ivec2 max_size2 = World::get_max_size(layer, merged, { 7, 7 }, BlockType::Stone);
		if (max_size2[0] != 3 || max_size2[1] != 1) {
			throw "wrong max_size2";
		}

	}

	// given a layer and start point, find its best dimensions
	vmath::ivec2 get_max_size(BlockType layer[16][16], vmath::ivec2 start_point, BlockType block_type) {
		assert(block_type != BlockType::Air);

		// TODO: Start max size at {1,1}, and for loops at +1.
		// TODO: Search width with find() instead of a for loop.

		// "max width and height"
		vmath::ivec2 max_size = { 0, 0 };

		// maximize width
		for (int i = start_point[0], j = start_point[1]; i < 16; i++) {
			// if extended by 1, add 1 to max width
			if (layer[i][j] == block_type) {
				max_size[0]++;
			}
			// else give up
			else {
				break;
			}
		}

		assert(max_size[0] > 0 && "WTF? Max width is 0? Doesn't make sense.");

		// now that we've maximized width, need to
		// maximize height

		// for each height
		for (int j = start_point[1]; j < 16; j++) {
			// check if entire width is correct
			for (int i = start_point[0]; i < start_point[0] + max_size[0]; i++) {
				// if wrong block type, give up on extending height
				if (layer[i][j] != block_type) {
					break;
				}
			}

			// yep, entire width is correct! Extend max height and keep going
			max_size[1]++;
		}

		assert(max_size[1] > 0 && "WTF? Max height is 0? Doesn't make sense.");
		return max_size;
	}

}

