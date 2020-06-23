#pragma once

#include "chunk.h"
#include "chunkdata.h"
#include "messaging.h"
#include "player.h"
#include "render.h"
#include "util.h"
#include "world.h"
#include "world_meshing.h"
#include "world_render.h"

#include "GL/gl3w.h"
#include "GLFW/glfw3.h"
#include "vmath.h"
#include "zmq.hpp"

#include <assert.h>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>

constexpr int NUM_MESH_GEN_THREADS = 1;

void run_game(zmq::context_t* const ctx);
void MeshingThread(zmq::context_t* const ctx, msg::on_ready_fn on_ready);
void ChunkGenThread(zmq::context_t* const ctx, msg::on_ready_fn on_ready);

class App {
public:
	// zmq
	zmq::context_t* const ctx;
	BusNode bus;

	App(zmq::context_t* const ctx_) : ctx(ctx_), bus(ctx_)
	{
		std::fill(held_keys, held_keys + (sizeof(held_keys) / sizeof(held_keys[0])), false);
		bus.out.setsockopt(ZMQ_SUBSCRIBE, "", 0);
	}

	~App()
	{
		// Send exit message
		std::vector<zmq::const_buffer> message({
			zmq::buffer(msg::EXIT)
			});

		auto ret = zmq::send_multipart(bus.in, message, zmq::send_flags::dontwait);
		assert(ret);
	}

	/* INPUTS */

	// mouse inputs
	double last_mouse_x = 0;
	double last_mouse_y = 0;

	// key inputs
	bool held_keys[GLFW_KEY_LAST + 1];

	// redirected GLFW/GL callbacks
	void onKey(GLFWwindow* window, int key, int scancode, int action, int mods);
	void onMouseMove(GLFWwindow* window, double x, double y);
	void onResize(GLFWwindow* window, int width, int height);
	void onMouseButton(int button, int action);
	void onMouseWheel(double scroll_direction);

	void onDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message);

	/* RENDER PART */

	GLFWwindow* window = nullptr;

	// settings
	GlfwInfo windowInfo;
	OpenGLInfo glInfo;

	// misc
	float last_render_time = 0;

	// funcs
	void render(float time);
	void render_debug_info(float dt);
	void render_main_menu();

	bool show_debug_info = false;
	bool should_fix_tjunctions = true;
	unsigned min_render_distance = 0;
	double fps = 0;
	bool capture_mouse = true;

	// misc
	std::unique_ptr<WorldRenderPart> world_render;

	/* WORLD PART */

	// player
	Player player;

	// misc
	std::unique_ptr<WorldDataPart> world_data;

	BlockType held_block = BlockType::StillWater; // TODO: Instead, remembering which inventory slot
	vmath::ivec2 last_chunk_coords = { std::numeric_limits<int>::max(), std::numeric_limits<int>::max() };
	bool should_check_for_nearby_chunks = true;
	bool noclip = false;

	// funcs
	void updateWorld(float time);
	void update_player_movement(const float dt);
	vmath::vec4 prevent_collisions(const vmath::vec4 position_change);

	const vmath::ivec2& get_last_chunk_coords() const;
	void set_last_chunk_coords(const vmath::ivec2& last_chunk_coords);

	/* GAME STATE PART */

	// funcs
	void run();
	void startup();
	void shutdown();

	inline void set_min_render_distance(int min_render_distance) {
		if (min_render_distance > this->min_render_distance) {
			should_check_for_nearby_chunks = true;
		}
		this->min_render_distance = min_render_distance;
	}
};

namespace {
	// global GLFW/GL callback functions
	void glfw_onError(int error, const char* description);
	void glfw_onKey(GLFWwindow* window, int key, int scancode, int action, int mods);
	void glfw_onMouseMove(GLFWwindow* window, double x, double y);
	void glfw_onResize(GLFWwindow* window, int width, int height);
	void glfw_onMouseButton(GLFWwindow* window, int button, int action, int mods);
	void glfw_onMouseWheel(GLFWwindow* window, double xoffset, double yoffset);

	void APIENTRY gl_onDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, GLvoid* userParam);
}
