#ifndef __GAME_H__
#define __GAME_H__

#include "chunks.h"
#include "util.h"

#include "GL/gl3w.h"
#include "GLFW/glfw3.h"

#include <unordered_map>
#include <string>
#include <vmath.h>
#include <tuple>
#include <utility>

using namespace std;
using namespace vmath;

struct AppInfo {
	string title = "OpenGL";
	bool debug = GL_TRUE;
	bool msaa = GL_FALSE;
	int width = 800;
	int height = 600;
	float vfov = 59.0f; // vertical fov -- 59.0 vfov = 90.0 hfov
	float mouseX_Sensitivity = 0.25f;
	float mouseY_Sensitivity = 0.25f;
};

struct pair_hash
{
	template <class T1, class T2>
	std::size_t operator() (const std::pair<T1, T2> &pair) const
	{
		return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
	}
};

class App {
protected:
public:
	// the one and only copy of this app
	AppInfo info;
	GLFWwindow *window;

	GLuint rendering_program;
	GLuint vao_cube, vao2;

	// buffers
	GLuint trans_buf; // transformations buffer - currently stores view and projection transformations.
	GLuint vert_buf; // vertices buffer - currently stores vertices for a single 3D cube
	GLuint chunk_types_buf; // stores the block type for every block in the chunk

	double last_mouse_x;
	double last_mouse_y;
	double last_render_time;
	bool held_keys[GLFW_KEY_LAST + 1];

	vec4 char_position = { 8.0f, 66.0f, 8.0f, 1.0f };
	vec4 char_velocity = { 0.0f };
	float char_pitch = 0.0f; //   up/down  angle;    capped to [-90.0, 90.0]
	float char_yaw = 0.0f;   // left/right angle; un-capped (TODO: Reset it if it gets too high?)

	// store our chunk info in here for now
	Block* chunks[1024];
	ivec2 chunk_coords[1024];

	unordered_map<pair<int, int>, Chunk*, pair_hash> chunk_map;

	App() {

	}

	void shutdown() {
		// TODO: Maybe some day.
	}

	void run();
	void startup();
	void render(double);
	void update_player_movement(const double);
	void velocity_prevent_collisions(const double);
	void velocity_prevent_collisions2(const double);
	void velocity_prevent_collisions3(const double);
	//Block get_type(int x, int y, int z);
	bool is_dir_clear(vec4);


	inline void add_chunk(int x, int z, Chunk* chunk) {
		chunk_map[{x, z}] = chunk;
	}

	inline Chunk* get_chunk(int x, int z) {
		return chunk_map[{x, z}];
	}

	// get chunk that contains block w/ these coordinates
	inline Chunk* get_chunk_real_coords(int x, int z) {
		return chunk_map[{x / 16, z / 16}];
	}

	// coordinates in real world to coordinates in its chunk
	inline ivec3 real_coords_to_chunk_coords(int x, int y, int z) {
		return ivec3(x % 16, y, z % 16);
	}

	// get type of this block
	inline Block get_type(int x, int y, int z) {
		Chunk* chunk = get_chunk_real_coords(x, z);
		ivec3 chunk_coords = real_coords_to_chunk_coords(x, y, z);
		return chunk->get_block(chunk_coords[0], chunk_coords[1], chunk_coords[2]);
	}

	void onKey(GLFWwindow* window, int key, int scancode, int action, int mods);
	void onMouseMove(GLFWwindow* window, double x, double y);
	void onResize(GLFWwindow* window, int width, int height);

	// glfw static functions which passthrough to real handle functions
	static void glfw_onKey(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void glfw_onMouseMove(GLFWwindow* window, double x, double y);
	static void glfw_onResize(GLFWwindow* window, int width, int height);
	//static void gl_onDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, GLvoid* userParam);

	static App* app;
};
// define/declare/de-whatever App's static variables
App* App::app;

#endif /* __GAME_H__ */