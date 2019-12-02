#ifndef __WORLD_H__
#define __WORLD_H__

#include "chunk.h"
#include "chunkdata.h"
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

// represents an in-game world
class World {
public:
	// map of (chunk coordinate) -> chunk
	unordered_map<pair<int, int>, Chunk*, pair_hash> chunk_map;

	World() {

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
		if (chunk == nullptr) {
			throw "Wew";
		}
		chunk_map[{x, z}] = chunk;

		char buf[256];
		sprintf(buf, "Loaded chunks: %d\n", chunk_map.size());
		OutputDebugString(buf);
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

	// generate chunks near player
	inline void gen_nearby_chunks(vmath::vec4 position, int distance) {
		assert(distance > 0 && "invalid distance");

		ivec2 chunk_coords = get_chunk_coords((int)floorf(position[0]), (int)floorf(position[2]));

		for (int i = -distance; i <= distance; i++) {
			for (int j = -(distance - abs(i)); j <= distance - abs(i); j++) {
				get_chunk_generate_if_required(chunk_coords[0] + i, chunk_coords[1] + j);
			}
		}
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
	inline vmath::ivec3 get_chunk_relative_coordinates(int x, int y, int z) {
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

		return vmath::ivec3(x, y, z);
	}

	// get a block's type
	inline Block get_type(int x, int y, int z) {
		Chunk* chunk = get_chunk_containing_block(x, z);
		vmath::ivec3 chunk_coords = get_chunk_relative_coordinates(x, y, z);

		if (!chunk) {
			return Block::Air;
		}

		return chunk->get_block(chunk_coords[0], chunk_coords[1], chunk_coords[2]);
	}

	inline Block get_type(vmath::ivec3 xyz) { return get_type(xyz[0], xyz[1], xyz[2]); }
	inline Block get_type(vmath::ivec4 xyz_) { return get_type(xyz_[0], xyz_[1], xyz_[2]); }


	// generate chunk at (x, z) and add it
	inline void gen_chunk_at(int x, int z) {
		// generate it
		Chunk* c = gen_chunk(x, z);

		// set up its MiniChunks
		for (auto &mini : c->minis) {
			mini.invisible = mini.invisible || mini.all_air() || check_if_covered(mini);
		}

		// set up nearby MiniChunks
		for (auto coords : c->surrounding_chunks()) {
			Chunk* c2 = get_chunk(coords[0], coords[1]);
			if (c2 == nullptr) continue;

			for (auto &mini : c2->minis) {
				mini.invisible = mini.invisible || mini.all_air() || check_if_covered(mini);
			}
		}

		add_chunk(x, z, c);
	}


	inline bool check_if_covered(MiniChunk mini) {
		// if contains any air blocks, don't know how to handle that yet
		if (mini.any_air()) {
			return false;
		}

		// none are air, so only check outside blocks
		for (int miniX = 0; miniX < CHUNK_WIDTH; miniX++) {
			for (int miniY = 0; miniY < MINICHUNK_HEIGHT; miniY++) {
				for (int miniZ = 0; miniZ < CHUNK_DEPTH; miniZ++) {
					int x = mini.coords[0] * CHUNK_WIDTH + miniX;
					int y = mini.coords[1] + miniY;
					int z = mini.coords[2] * CHUNK_DEPTH + miniZ;

					vmath::ivec4 coords = vmath::ivec4(x, y, z, 0);

					// if along east wall, check east
					if (miniX == CHUNK_WIDTH - 1) {
						if (get_type(clamp_coords_to_world(coords + IEAST_0)) == Block::Air) return false;
					}
					// if along west wall, check west
					if (miniX == 0) {
						if (get_type(clamp_coords_to_world(coords + IWEST_0)) == Block::Air) return false;
					}

					// if along north wall, check north
					if (miniZ == 0) {
						if (get_type(clamp_coords_to_world(coords + INORTH_0)) == Block::Air) return false;
					}
					// if along south wall, check south
					if (miniZ == CHUNK_DEPTH - 1) {
						if (get_type(clamp_coords_to_world(coords + ISOUTH_0)) == Block::Air) return false;
					}

					// if along bottom wall, check bottom
					if (miniY == 0) {
						if (get_type(clamp_coords_to_world(coords + IDOWN_0)) == Block::Air) return false;
					}
					// if along top wall, check top
					if (miniY == MINICHUNK_HEIGHT - 1) {
						if (get_type(clamp_coords_to_world(coords + IUP_0)) == Block::Air) return false;
					}
				}
			}
		}

		return true;
	}

	inline void render(OpenGLInfo* glInfo) {
		for (auto &[coords_p, chunk] : chunk_map) {
			chunk->render(glInfo);
		}
	}
};

#endif /* __WORLD_H__ */
