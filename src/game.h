#pragma once

constexpr int NUM_MESH_GEN_THREADS = 1;

#include "chunk.h"
#include "chunkdata.h"
#include "GL/gl3w.h"
#include "GLFW/glfw3.h"
#include "render.h"
#include "util.h"
#include "vmath.h"
#include "world.h"
#include "zmq.hpp"

#include <assert.h>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>

using namespace std;
using namespace vmath;

void run_game();

class App {
public:
	/* ETC */
	static std::unique_ptr<App> app;

	// zmq
	zmq::context_t ctx;

	App() : ctx(0) {
		std::fill(held_keys, held_keys + (sizeof(held_keys) / sizeof(held_keys[0])), false);
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

	bool show_debug_info = false;
	bool should_fix_tjunctions = true;
	unsigned min_render_distance = 0;
	double fps = 0;

	/* WORLD PART */

	// movement
	vec4 char_position = { 8.0f, 73.0f, 8.0f, 1.0f };
	vec4 char_velocity = { 0.0f };

	// rotation
	float char_pitch = 0.0f; //   up/down  angle;    capped to [-90.0, 90.0]
	float char_yaw = 0.0f;   // left/right angle; un-capped (TODO: Reset it if it gets too high?)

	// misc
	World* world = nullptr;
	ivec3 staring_at = { 0, -1, 0 }; // the block you're staring at (invalid by default)
	ivec3 staring_at_face; // the face you're staring at on the block you're staring at
	BlockType held_block = BlockType::StillWater;
	ivec2 last_chunk_coords = { std::numeric_limits<int>::max(), std::numeric_limits<int>::max() };
	bool should_check_for_nearby_chunks = true;
	bool noclip = false;

	// funcs
	void updateWorld(float time);
	void update_player_movement(const float dt);
	vec4 prevent_collisions(const vec4 position_change);

	// get direction player's staring at
	inline auto staring_direction() {
		return rotate_pitch_yaw(char_pitch, char_yaw) * NORTH_0;
	}

	// get direction straight up from player
	inline auto up_direction() {
		return rotate_pitch_yaw(char_pitch, char_yaw) * UP_0;
	}

	// get direction straight right from player
	inline auto right_direction() {
		return rotate_pitch_yaw(char_pitch, char_yaw) * EAST_0;
	}

	const inline auto& get_last_chunk_coords() const {
		return last_chunk_coords;
	}

	inline void set_last_chunk_coords(const ivec2& last_chunk_coords) {
		if (last_chunk_coords != this->last_chunk_coords) {
			this->last_chunk_coords = last_chunk_coords;
			should_check_for_nearby_chunks = true;
		}
	}

	/* GAME STATE PART */

	// funcs
	void run();
	void startup();
	void shutdown() { /* TODO: Maybe some day. */ }

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
