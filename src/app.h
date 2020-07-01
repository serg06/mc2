#pragma once

#include "chunk.h"
#include "chunkdata.h"
#include "game.h"
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

void run_game(std::shared_ptr<zmq::context_t> ctx);

class App {
public:
	// zmq
	std::shared_ptr<zmq::context_t> ctx;
	BusNode bus;

	App(std::shared_ptr<zmq::context_t> ctx_);
	~App();

	/* INPUTS */

	// redirected GLFW/GL callbacks
	void onKey(GLFWwindow* window, int key, int scancode, int action, int mods);
	void onMouseMove(GLFWwindow* window, double x, double y);
	void onResize(GLFWwindow* window, int width, int height);
	void onMouseButton(int button, int action);
	void onMouseWheel(double scroll_direction);
	void onDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message);

	/* RENDER PART */
	std::shared_ptr<GLFWwindow> window;
	
	void draw_game();
	void on_start_game();
	void on_quit_game();
	void on_enter_main_menu();
	void setup_imgui();

	enum class AppState
	{
		InMainMenu,
		InGame,
		Quitting
	};
	AppState state = AppState::InMainMenu;
	bool draw_main_menu();
	void draw_frame();

	// settings
	std::shared_ptr<GlfwInfo> windowInfo;
	std::shared_ptr<OpenGLInfo> glInfo;

	bool capture_mouse = true;

	/* GAME STATE PART */

	// funcs
	void run();
	void startup();
	void shutdown();

	// World
	std::shared_ptr<Game> game;
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
