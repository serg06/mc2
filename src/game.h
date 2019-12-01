#ifndef __GAME_H__
#define __GAME_H__

#define MIN_RENDER_DISTANCE 8
#define GPU_MAX_CHUNKS 256

#include "chunk.h"
#include "render.h"
#include "util.h"

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

	// movement
	vec4 char_position = { 8.0f, 66.0f, 8.0f, 1.0f };
	vec4 char_velocity = { 0.0f };

	// rotation
	float char_pitch = 0.0f; //   up/down  angle;    capped to [-90.0, 90.0]
	float char_yaw = 0.0f;   // left/right angle; un-capped (TODO: Reset it if it gets too high?)

	// misc
	unordered_map<pair<int, int>, Chunk*, pair_hash> chunk_map;
	float last_render_time;
	int num_gpu_chunks = 0;
	unordered_map<pair<int, int>, int, pair_hash> chunk_indices_map;

	App() {}
	void run();
	void startup();
	void shutdown() { /* TODO: Maybe some day. */ }
	void render(float time);
	void update_player_movement(const float dt);
	vec4 prevent_collisions(const vec4 position_change);
	std::vector<vmath::ivec4> get_intersecting_blocks(vec4 player_position);

	// generate chunks near player
	inline void gen_nearby_chunks() {
		ivec2 chunk_coords = get_chunk_coords((int)floorf(char_position[0]), (int)floorf(char_position[2]));

		for (int i = -MIN_RENDER_DISTANCE; i <= MIN_RENDER_DISTANCE; i++) {
			for (int j = -(MIN_RENDER_DISTANCE - abs(i)); j <= MIN_RENDER_DISTANCE - abs(i); j++) {
				get_chunk_generate_if_required(chunk_coords[0] + i, chunk_coords[1] + j);
			}
		}
	}

	// add chunk to chunk coords (x, z)
	// AND TO GPU!
	inline void add_chunk(int x, int z, Chunk* chunk) {
		auto search = chunk_map.find({ x, z });

		// if element already exists, error
		if (search != chunk_map.end()) {
			throw "Tried to add chunk but it already exists.";
		}

		// insert our chunk
		if (chunk == nullptr || chunk->data == nullptr) {
			throw "Wew";
		}
		chunk_map[{x, z}] = chunk;

		/* ADD TO GPU */

		if (num_gpu_chunks >= GPU_MAX_CHUNKS) {
			throw "ERROR: Cannot add new chunk, we've fully filled up the chunk buffer.";
		}

		ivec2 coord_data[CHUNK_SIZE];
		for (int i = 0; i < CHUNK_SIZE; i++) {
			coord_data[i] = ivec2(x, z);
		}

		//glNamedBufferSubData(chunk_types_buf_yuge, num_gpu_chunks * CHUNK_SIZE * sizeof(Block), CHUNK_SIZE * sizeof(Block), chunk->data);
		//glNamedBufferSubData(coords_buf_yuge, num_gpu_chunks * CHUNK_SIZE * sizeof(ivec2), CHUNK_SIZE * sizeof(ivec2), coord_data);
		chunk_indices_map[{x, z}] = num_gpu_chunks;

		num_gpu_chunks++;
	}

	// get chunk (generate it if required)
	inline Chunk* get_chunk_generate_if_required(int x, int z) {
		auto search = chunk_map.find({ x, z });

		// if doesn't exist, generate it
		if (search == chunk_map.end()) {
			gen_chunk_at(x, z);
		}

		return chunk_map[{x, z}];
	}

	// get chunk or nullptr
	inline Chunk* get_chunk(int x, int z) {
		auto search = chunk_map.find({ x, z });

		// if doesn't exist, return null
		if (search == chunk_map.end()) {
			return nullptr;
		}

		return (*search).second;
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

	// get a block's type
	inline Block get_type(int x, int y, int z) {
		Chunk* chunk = get_chunk_containing_block(x, z);
		ivec3 chunk_coords = get_chunk_relative_coordinates(x, y, z);

		if (!chunk) {
			return Block::Air;
		}

		return chunk->get_block(chunk_coords[0], chunk_coords[1], chunk_coords[2]);
	}

	inline bool App::check_if_covered(MiniChunk* mini) {
		auto directions = {
			INORTH_0,
			ISOUTH_0,
			IEAST_0,
			IWEST_0,
			IUP_0,
			IDOWN_0,
		};

		// simple way: just check every side of every cube
		for (int miniX = 0; miniX < CHUNK_WIDTH; miniX++) {
			for (int miniY = 0; miniY < MINICHUNK_HEIGHT; miniY++) {
				for (int miniZ = 0; miniZ < CHUNK_DEPTH; miniZ++) {
					for (auto direction : directions) {
						int x = mini->coords[0] * CHUNK_WIDTH + miniX;
						int y = mini->coords[1] + miniY;
						int z = mini->coords[2] * CHUNK_DEPTH + miniZ;

						ivec4 coords = ivec4(x, y, z, 0) + direction;

						if (get_type(clamp_coords_to_world(coords)) == Block::Air) {
							return false;
						}
					}
				}
			}
		}

		return true;
	}

	// generate chunk at (x, z) and add it
	inline void gen_chunk_at(int x, int z) {
		// generate it
		Chunk* c = gen_chunk(x, z);

		// set up its MiniChunks
		for (auto mini : c->minis) {
			mini->invisible = mini->invisible || mini->all_air() || check_if_covered(mini);
		}

		// set up nearby MiniChunks
		for (auto coords : c->surrounding_chunks()) {
			Chunk* c2 = get_chunk(coords[0], coords[1]);
			if (c2 == nullptr) continue;

			for (auto mini : c2->minis) {
				mini->invisible = mini->invisible || mini->all_air() || check_if_covered(mini);
			}
		}

		add_chunk(x, z, c);
	}

	inline Block get_type(ivec3 xyz) { return get_type(xyz[0], xyz[1], xyz[2]); }
	inline Block get_type(ivec4 xyz_) { return get_type(xyz_[0], xyz_[1], xyz_[2]); }

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
