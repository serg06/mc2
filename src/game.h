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

#include <cassert>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>

constexpr int NUM_MESH_GEN_THREADS = 1;

void run_game(std::shared_ptr<zmq::context_t> ctx);

class Game {
public:
	// zmq
	std::shared_ptr<zmq::context_t> ctx;
	BusNode bus;

	Game(std::shared_ptr<zmq::context_t> ctx_, std::shared_ptr<GLFWwindow> window_, std::shared_ptr<GlfwInfo> windowInfo_, std::shared_ptr<OpenGLInfo> glInfo_);
	~Game();

	/* INPUTS */

	// mouse inputs
	double last_mouse_x = 0;
	double last_mouse_y = 0;

	// key inputs
	std::array<bool, GLFW_KEY_LAST + 1> held_keys;

	// redirected GLFW/GL callbacks
	void onKey(GLFWwindow* window, int key, int scancode, int action, int mods);
	void onMouseMove(GLFWwindow* window, double x, double y);
	void onResize(GLFWwindow* window, int width, int height);
	void onMouseButton(int button, int action);
	void onMouseWheel(double scroll_direction);

	/* RENDER PART */
	std::shared_ptr<GLFWwindow> window;

	// settings
	std::shared_ptr<GlfwInfo> windowInfo;
	std::shared_ptr<OpenGLInfo> glInfo;

	// misc
	float last_render_time = 0;

	// funcs
	void render_frame(bool& quit);
	void render(float time);
	void render_debug_info(float dt);
	void render_esc_menu(bool& quit);

	bool show_debug_info = false;
	bool should_fix_tjunctions = true;
	double fps = 0;
	bool capture_mouse = true;

	// misc
	std::unique_ptr<WorldRenderPart> world_render;
	enum class GameState
	{
		InGame,
		InEscMenu
	};
	GameState state = GameState::InGame;

	/* WORLD PART */

	// misc
	std::unique_ptr<World> world;

	// funcs
	void update_player_actions();
	Player& get_player();

	/* GAME STATE PART */

	// funcs
	bool running = false;
	void startup();
	void shutdown();

	inline void set_min_render_distance(int min_render_distance) {
		if (min_render_distance > world->player.render_distance) {
			world->player.should_check_for_nearby_chunks = true;
		}
		world->player.render_distance = min_render_distance;
	}
};
