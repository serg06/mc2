#ifndef __GAME_H__
#define __GAME_H__

#define MIN_RENDER_DISTANCE 2

#include "chunks.h"
#include "util.h"

#include "GL/gl3w.h"
#include "GLFW/glfw3.h"

#include <assert.h>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vmath.h>
#include <windows.h>

using namespace std;
using namespace vmath;

struct AppInfo {
	std::string title = "OpenGL";
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
	double time;

	GLuint rendering_program;
	GLuint vao_cube, vao2;

	bool noclip = false;

	// buffers
	GLuint trans_buf; // transformations buffer - currently stores view and projection transformations.
	GLuint vert_buf; // vertices buffer - currently stores vertices for a single 3D cube
	GLuint chunk_types_buf; // stores the block type for every block in the chunk

	double last_mouse_x;
	double last_mouse_y;
	float last_render_time;
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
	void render(float time);
	void update_player_movement(const float dt);
	vec4 App::velocity_prevent_collisions(const float dt, const vec4 position_change);
	bool is_dir_clear(vec4);
	vector<ivec4> App::get_intersecting_blocks(vec4 velocity, vec4 direction = { 0 });


	inline void gen_nearby_chunks() {
		// get chunk coords
		ivec2 chunk_coords = get_chunk_coords((int)floorf(char_position[0]), (int)floorf(char_position[2]));

		for (int i = -MIN_RENDER_DISTANCE; i <= MIN_RENDER_DISTANCE; i++) {
			for (int j = -(MIN_RENDER_DISTANCE - abs(i)); j <= MIN_RENDER_DISTANCE - abs(i); j++) {
				get_chunk(chunk_coords[0] + i, chunk_coords[1] + j);
			}
		}
	}

	inline void add_chunk(int x, int z, Chunk* chunk) {
		auto search = chunk_map.find({ x, z });

		// if element already exists, error
		if (search != chunk_map.end()) {
			throw "Tried to add chunk but it already exists.";
		}
		
		// insert our chunk
		chunk_map[{x, z}] = chunk;
	}

	// get chunk (generate it if required)
	inline Chunk* get_chunk(int x, int z) {
		auto search = chunk_map.find({ x, z });

		// if doesn't exist, generate it
		if (search == chunk_map.end()) {
			add_chunk(x, z, gen_chunk(x, z));
		}

		return chunk_map[{x, z}];
	}

	// get chunk that contains block at (x, _, z)
	inline Chunk* get_chunk_containing_block(int x, int z) {
		return get_chunk((int)floorf((float)x / 16.0f), (int)floorf((float)z / 16.0f));
	}

	// get chunk-coordinates of chunk containing the block at (x, _, z)
	inline ivec2 get_chunk_coords(int x, int z) {
		return { (int)floorf((float)x / 16.0f), (int)floorf((float)z / 16.0f) };
	}

	// given a block's real-world coordinates, return that block's coordinates relative to its chunk
	inline ivec3 get_chunk_relative_coordinates(int x, int y, int z) {
		// adjust x and y
		x = x % CHUNK_WIDTH;
		z = z % CHUNK_DEPTH;

		// make sure modulo didn't leave them negative
		if (x < 0) {
			x += CHUNK_WIDTH;
		}
		if (z < 0) {
			z += CHUNK_WIDTH;
		}

		return ivec3(x, y, z);
	}

	// get type of this block
	inline Block get_type(int x, int y, int z) {
		Chunk* chunk = get_chunk_containing_block(x, z);
		ivec3 chunk_coords = get_chunk_relative_coordinates(x, y, z);
		auto result = chunk->get_block(chunk_coords[0], chunk_coords[1], chunk_coords[2]);

		return result;
	}

	inline Block get_type(ivec3 xyz) { return get_type(xyz[0], xyz[1], xyz[2]); }
	inline Block get_type(ivec4 xyz_) { return get_type(xyz_[0], xyz_[1], xyz_[2]); }

	// redirected GLFW/GL callbacks
	void onKey(GLFWwindow* window, int key, int scancode, int action, int mods);
	void onMouseMove(GLFWwindow* window, double x, double y);
	void onResize(GLFWwindow* window, int width, int height);
	void onDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message);

	static App* app;
};
App* App::app;

namespace {
	// global GLFW/GL callback functions
	void glfw_onError(int error, const char* description);
	void glfw_onKey(GLFWwindow* window, int key, int scancode, int action, int mods);
	void glfw_onMouseMove(GLFWwindow* window, double x, double y);
	void glfw_onResize(GLFWwindow* window, int width, int height);
	void APIENTRY gl_onDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, GLvoid* userParam);
}

#endif /* __GAME_H__ */
