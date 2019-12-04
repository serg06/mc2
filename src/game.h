#ifndef __GAME_H__
#define __GAME_H__

#define GPU_MAX_CHUNKS 256

#include "chunk.h"
#include "chunkdata.h"
#include "render.h"
#include "util.h"
#include "world.h"

#include "GL/gl3w.h"
#include "GLFW/glfw3.h"

#include <assert.h>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>
#include <vmath.h>
#include <windows.h>

using namespace std;
using namespace vmath;

class App {
public:
	static App* app;
	GLFWwindow* window;

	// settings
	GlfwInfo windowInfo;
	OpenGLInfo glInfo;

	// mouse inputs
	double last_mouse_x;
	double last_mouse_y;

	// key inputs
	bool held_keys[GLFW_KEY_LAST + 1];
	bool noclip = false;
	unsigned min_render_distance = 3;

	// movement
	vec4 char_position = { 8.0f, 66.0f, 8.0f, 1.0f }; // DEBUG: lower player
	vec4 char_velocity = { 0.0f };

	// rotation
	float char_pitch = 0.0f; //   up/down  angle;    capped to [-90.0, 90.0]
	float char_yaw = 0.0f;   // left/right angle; un-capped (TODO: Reset it if it gets too high?)

	// misc
	World* world;
	float last_render_time;
	int num_chunks = 0;

	App() {}
	void run();
	void startup();
	void shutdown() { /* TODO: Maybe some day. */ }
	void render(float time);
	void update_player_movement(const float dt);
	vec4 prevent_collisions(const vec4 position_change);
	std::vector<vmath::ivec4> get_intersecting_blocks(vec4 player_position);

	// redirected GLFW/GL callbacks
	void onKey(GLFWwindow* window, int key, int scancode, int action, int mods);
	void onMouseMove(GLFWwindow* window, double x, double y);
	void onResize(GLFWwindow* window, int width, int height);
	void onMouseButton(int button, int action);
	void onMouseWheel(int pos);

	void onDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message);
};
App* App::app;

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

#endif /* __GAME_H__ */
