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

WorldRenderPart::WorldRenderPart(zmq::context_t* const ctx_) : bus(ctx_)
{
	bus.out.setsockopt(ZMQ_SUBSCRIBE, "", 0);
}
WorldDataPart::WorldDataPart(zmq::context_t* const ctx_) : bus(ctx_)
{
	bus.out.setsockopt(ZMQ_SUBSCRIBE, "", 0);
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

void WorldRenderPart::update_meshes()
{
	// Receive all mesh-gen results
	std::vector<zmq::message_t> message;
	auto ret = zmq::recv_multipart(bus.out, std::back_inserter(message), zmq::recv_flags::dontwait);
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
		ret = zmq::recv_multipart(bus.out, std::back_inserter(message), zmq::recv_flags::dontwait);
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








/*************************************************************/
/* PLACING TESTS IN HERE UNTIL I LEARN HOW TO DO IT PROPERLY */
/*************************************************************/

//
//#include "minichunk.h"
//#include <chrono>
//
//namespace WorldTests {
//	void run_all_tests(OpenGLInfo* glInfo) {
//		test_gen_quads();
//		test_mark_as_merged();
//		test_get_max_size();
//		//test_gen_layer();
//		OutputDebugString("WorldTests completed successfully.\n");
//	}
//
//	//void test_gen_layer() {
//	//	// gen chunk at 0,0
//	//	Chunk* chunk = new Chunk({ 0, 0 });
//	//	chunk->generate();
//
//	//	// grab first mini that has stone, grass, and air blocks
//	//	MiniChunk* mini;
//	//	for (auto &mini2 : chunk->minis) {
//	//		bool has_air = false, has_stone = false, has_grass = false;
//	//		for (int i = 0; i < MINICHUNK_SIZE; i++) {
//	//			has_air |= mini2.blocks[i] == BlockType::Air;
//	//			has_stone |= mini2.blocks[i] == BlockType::Stone;
//	//			has_grass |= mini2.blocks[i] == BlockType::Grass;
//	//		}
//
//	//		if (has_air && has_stone && has_grass) {
//	//			mini = &mini2;
//	//		}
//	//	}
//
//	//	if (mini == nullptr) {
//	//		throw "No sufficient minis!";
//	//	}
//
//	//	// grab 2nd layer facing us in z direction
//	//	BlockType result[16][16];
//	//	int z = 1;
//	//	vmath::ivec3 face = { 0, 0, -1 };
//
//	//	for (int x = 0; x < 16; x++) {
//	//		for (int y = 0; y < 16; y++) {
//	//			// set working indices (TODO: move u to outer loop)
//	//			vmath::ivec3 coords = { x, y, z };
//
//	//			// get block at these coordinates
//	//			BlockType block = mini->get_block(coords);
//
//	//			// dgaf about air blocks
//	//			if (block == BlockType::Air) {
//	//				continue;
//	//			}
//
//	//			// get face block
//	//			BlockType face_block = mini->get_block(coords + face);
//
//	//			// if block's face is visible, set it
//	//			if (World::is_face_visible(block, face_block)) {
//	//				result[x][y] = block;
//	//			}
//	//		}
//	//	}
//
//	//	// Do the same with gen_layer_fast
//	//	BlockType expected[16][16];
//	//	World::gen_layer_generalized(mini, mini, 2, 1, face, expected);
//
//	//	// Make sure they're the same
//	//	for (int x = 0; x < 16; x++) {
//	//		for (int y = 0; y < 16; y++) {
//	//			if (result[x][y] != expected[x][y]) {
//	//				throw "It broke!";
//	//			}
//	//		}
//	//	}
//
//	//	// clear
//	//	chunk->clear();
//	//}
//
//	void test_gen_quads() {
//		// create layer of all air
//		BlockType layer[16][16];
//		memset(layer, (uint8_t)BlockType::Air, 16 * 16 * sizeof(BlockType));
//
//		// 1. Add rectangle from (1,3) to (3,6)
//		for (int i = 1; i <= 3; i++) {
//			for (int j = 3; j <= 6; j++) {
//				layer[i][j] = BlockType::Stone;
//			}
//		}
//		// expected result
//		Quad2D q1;
//		q1.block = BlockType::Stone;
//		q1.corners[0] = { 1, 3 };
//		q1.corners[1] = { 4, 7 };
//
//		// 2. Add plus symbol - line from (7,5)->(7,9), and (5,7)->(9,7)
//		for (int i = 7; i <= 7; i++) {
//			for (int j = 5; j <= 9; j++) {
//				layer[i][j] = BlockType::Stone;
//			}
//		}
//		for (int i = 5; i <= 9; i++) {
//			for (int j = 7; j <= 7; j++) {
//				layer[i][j] = BlockType::Stone;
//			}
//		}
//		// expected result
//		vector<Quad2D> vq2;
//		Quad2D q2;
//		q2.block = BlockType::Stone;
//
//		q2.corners[0] = { 5, 7 };
//		q2.corners[1] = { 10, 8 };
//		vq2.push_back(q2);
//
//		q2.corners[0] = { 7, 5 };
//		q2.corners[1] = { 8, 7 };
//		vq2.push_back(q2);
//
//		q2.corners[0] = { 7, 8 };
//		q2.corners[1] = { 8, 10 };
//		vq2.push_back(q2);
//
//		// Finally, add line all along bottom
//		for (int i = 0; i <= 15; i++) {
//			for (int j = 15; j <= 15; j++) {
//				layer[i][j] = BlockType::Grass;
//			}
//		}
//		// expected result
//		Quad2D q3;
//		q3.block = BlockType::Grass;
//		q3.corners[0] = { 0, 15 };
//		q3.corners[1] = { 16, 16 };
//
//		bool merged[16][16];
//		vector<Quad2D> result = World::gen_quads(layer, merged);
//
//		assert(result.size() == 5 && "wrong number of results");
//		assert(find(begin(result), end(result), q1) != end(result) && "q1 not in results list");
//		for (auto q : vq2) {
//			assert(find(begin(result), end(result), q) != end(result) && "q2's element not in results list");
//		}
//		assert(find(begin(result), end(result), q3) != end(result) && "q3 not in results list");
//	}
//
//	void test_mark_as_merged() {
//		bool merged[16][16];
//		memset(merged, 0, 16 * 16 * sizeof(bool));
//
//		vmath::ivec2 start = { 3, 4 };
//		vmath::ivec2 max_size = { 2, 5 };
//
//		World::mark_as_merged(merged, start, max_size);
//
//		for (int i = 0; i < 16; i++) {
//			for (int j = 0; j < 16; j++) {
//				// if in right x-range and y-range
//				if (start[0] <= i && i <= start[0] + max_size[0] - 1 && start[1] <= j && j <= start[1] + max_size[1] - 1) {
//					// make sure merged
//					if (!merged[i][j]) {
//						throw "not merged when should be!";
//					}
//				}
//				// else make sure not merged
//				else {
//					if (merged[i][j]) {
//						throw "merged when shouldn't be!";
//					}
//				}
//			}
//		}
//	}
//
//	void test_get_max_size() {
//		return; // TODO: fix
//
//		// create layer of all air
//		BlockType layer[16][16];
//		memset(layer, (uint8_t)BlockType::Air, 16 * 16 * sizeof(BlockType));
//
//		// let's say nothing is merged yet
//		bool merged[16][16];
//		memset(merged, false, 16 * 16 * sizeof(bool));
//
//		// 1. Add rectangle from (1,3) to (3,6)
//		for (int i = 1; i <= 3; i++) {
//			for (int j = 3; j <= 6; j++) {
//				layer[i][j] = BlockType::Stone;
//			}
//		}
//
//		// 2. Add plus symbol - line from (7,5)->(7,9), and (5,7)->(9,7)
//		for (int i = 7; i <= 7; i++) {
//			for (int j = 5; j <= 9; j++) {
//				layer[i][j] = BlockType::Stone;
//			}
//		}
//		for (int i = 5; i <= 9; i++) {
//			for (int j = 7; j <= 7; j++) {
//				layer[i][j] = BlockType::Stone;
//			}
//		}
//
//		// 3. Add line all along bottom
//		for (int i = 0; i <= 15; i++) {
//			for (int j = 15; j <= 15; j++) {
//				layer[i][j] = BlockType::Grass;
//			}
//		}
//
//		// Get max size for rectangle top-left-corner
//		vmath::ivec2 max_size1 = World::get_max_size(layer, merged, { 1, 3 }, BlockType::Stone);
//		if (max_size1[0] != 3 || max_size1[1] != 4) {
//			throw "wrong max_size1";
//		}
//
//		// Get max size for plus center
//		vmath::ivec2 max_size2 = World::get_max_size(layer, merged, { 7, 7 }, BlockType::Stone);
//		if (max_size2[0] != 3 || max_size2[1] != 1) {
//			throw "wrong max_size2";
//		}
//
//	}
//
//	// given a layer and start point, find its best dimensions
//	vmath::ivec2 get_max_size(BlockType layer[16][16], vmath::ivec2 start_point, BlockType block_type) {
//		assert(block_type != BlockType::Air);
//
//		// TODO: Start max size at {1,1}, and for loops at +1.
//		// TODO: Search width with find() instead of a for loop.
//
//		// "max width and height"
//		vmath::ivec2 max_size = { 0, 0 };
//
//		// maximize width
//		for (int i = start_point[0], j = start_point[1]; i < 16; i++) {
//			// if extended by 1, add 1 to max width
//			if (layer[i][j] == block_type) {
//				max_size[0]++;
//			}
//			// else give up
//			else {
//				break;
//			}
//		}
//
//		assert(max_size[0] > 0 && "WTF? Max width is 0? Doesn't make sense.");
//
//		// now that we've maximized width, need to
//		// maximize height
//
//		// for each height
//		for (int j = start_point[1]; j < 16; j++) {
//			// check if entire width is correct
//			for (int i = start_point[0]; i < start_point[0] + max_size[0]; i++) {
//				// if wrong block type, give up on extending height
//				if (layer[i][j] != block_type) {
//					break;
//				}
//			}
//
//			// yep, entire width is correct! Extend max height and keep going
//			max_size[1]++;
//		}
//
//		assert(max_size[1] > 0 && "WTF? Max height is 0? Doesn't make sense.");
//		return max_size;
//	}
//
//}
//
