#include "world_render.h"

#include "contiguous_hashmap.h"
#include "chunk.h"
#include "chunkdata.h"
#include "messaging.h"
#include "minichunkmesh.h"
#include "render.h"
#include "shapes.h"
#include "util.h"
#include "world_utils.h"

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
#ifdef _DEBUG
	bus.out.setsockopt(ZMQ_SUBSCRIBE, "", 0);
#else
	for (const auto& m : msg::render_thread_incoming)
	{
		// TODO: Upgrade zmq and replace this with .set()
		bus.out.setsockopt(ZMQ_SUBSCRIBE, m.c_str(), m.size());
	}
#endif // _DEBUG
}

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
	vmath::mat4 proj_matrix = vmath::perspective(
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

	proj_matrix = vmath::perspective(
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
