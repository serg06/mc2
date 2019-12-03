#ifndef __WORLD_H__
#define __WORLD_H__

#include "chunk.h"
#include "chunkdata.h"
#include "minichunkmesh.h"
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

struct Quad2D {
	Block block;
	ivec2 corners[2];
};

namespace WorldTests {
	void test_gen_quads();
	void run_all_tests();
	void test_mark_as_merged();
	void test_get_max_size();
}

// represents an in-game world
class World {
public:
	// map of (chunk coordinate) -> chunk
	unordered_map<pair<int, int>, Chunk*, pair_hash> chunk_map;
	int rendered = 0;

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
		assert(distance >= 0 && "invalid distance");

		ivec2 chunk_coords = get_chunk_coords((int)floorf(position[0]), (int)floorf(position[2]));

		// DEBUG: don't generate anything but (0,_,0)
		if (chunk_coords[0] != 0 || chunk_coords[1] != 0) {
			return;
		}

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
			if (!mini.invisible) {
				mini.mesh = gen_minichunk_mesh(&mini);
				mini.update_quads_buf();
			}
		}

		// set up nearby MiniChunks
		for (auto coords : c->surrounding_chunks()) {
			Chunk* c2 = get_chunk(coords[0], coords[1]);
			if (c2 == nullptr) continue;

			for (auto &mini : c2->minis) {
				mini.invisible = mini.invisible || mini.all_air() || check_if_covered(mini);
				if (!mini.invisible) {
					mini.mesh = gen_minichunk_mesh(&mini);
					mini.update_quads_buf();
				}
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
		char buf[256];
		sprintf(buf, "[%d] Rendering all chunks\n", rendered);
		//OutputDebugString(buf);

		for (auto &[coords_p, chunk] : chunk_map) {
			chunk->render(glInfo);
		}

		rendered++;
	}

	inline MiniChunkMesh* gen_minichunk_mesh(MiniChunk* mini) {
		// DEBUG: Check why mesh at (0,0,0) is fucking awful
		if (mini->coords[0] == 0 && mini->coords[1] == 0 && mini->coords[2] == 0) {
			OutputDebugString("");
		}
		MiniChunkMesh* mesh = new MiniChunkMesh();

		bool merged[16][16];

		vmath::ivec3 startPos, currPos, quadSize, m, n, offsetPos, vertices[4];

		Block startBlock;
		int direction, workAxis1, workAxis2;

		// Iterate over each face of the blocks.
		for (int face = 0; face < 6; face++) {
			bool isBackFace = face > 2;
			direction = face % 3;
			workAxis1 = (direction + 1) % 3;
			workAxis2 = (direction + 2) % 3;

			startPos = { 0, 0, 0 };
			currPos = { 0, 0, 0 };

			// Iterate over the chunk layer by layer.
			for (startPos[direction] = 0; startPos[direction] < 16; startPos[direction]++) {
				memset(merged, 0, 16 * 16 * sizeof(bool));

				// Build the slices of the mesh.
				for (startPos[workAxis1] = 0; startPos[workAxis1] < 16; startPos[workAxis1]++) {
					for (startPos[workAxis2] = 0; startPos[workAxis2] < 16; startPos[workAxis2]++) {
						startBlock = mini->get_block(startPos);

						if (startPos[0] == 1 && startPos[1] == 0 && startPos[2] == 0) {
							OutputDebugString("");
						}

						bool already_merged = merged[startPos[workAxis1]][startPos[workAxis2]];
						bool air = startBlock == Block::Air;
						bool not_visible = !IsBlockFaceVisible(mini, startPos, direction, isBackFace);

						// If this block has already been merged, is air, or not visible skip it.
						if (merged[startPos[workAxis1]][startPos[workAxis2]] || startBlock == Block::Air || !IsBlockFaceVisible(mini, startPos, direction, isBackFace)) {
							continue;
						}

						// Reset the work var
						quadSize = { 0, 0, 0 };

						// Figure out the width, then save it
						for (currPos = startPos, currPos[workAxis2]++; currPos[workAxis2] < 16 && CompareStep(mini, startPos, currPos, direction, isBackFace) && !merged[currPos[workAxis1]][currPos[workAxis2]]; currPos[workAxis2]++) {}
						quadSize[workAxis2] = currPos[workAxis2] - startPos[workAxis2];

						// Figure out the height, then save it
						for (currPos = startPos, currPos[workAxis1]++; currPos[workAxis1] < 16 && CompareStep(mini, startPos, currPos, direction, isBackFace) && !merged[currPos[workAxis1]][currPos[workAxis2]]; currPos[workAxis1]++) {
							for (currPos[workAxis2] = startPos[workAxis2]; currPos[workAxis2] < 16 && CompareStep(mini, startPos, currPos, direction, isBackFace) && !merged[currPos[workAxis1]][currPos[workAxis2]]; currPos[workAxis2]++) {}

							// If we didn't reach the end then its not a good add.
							if (currPos[workAxis2] - startPos[workAxis2] < quadSize[workAxis2]) {
								break;
							}
							else {
								currPos[workAxis2] = startPos[workAxis2];
							}
						}
						quadSize[workAxis1] = currPos[workAxis1] - startPos[workAxis1];

						// Now we add the quad to the mesh
						m = { 0, 0, 0 };
						m[workAxis1] = quadSize[workAxis1];

						n = { 0, 0, 0 };
						n[workAxis2] = quadSize[workAxis2];

						// We need to add a slight offset when working with front faces.
						offsetPos = startPos;
						offsetPos[direction] += isBackFace ? 0 : 1;

						//Draw the face to the mesh
						vertices[0] = offsetPos;
						vertices[1] = offsetPos + m;
						vertices[2] = offsetPos + m + n;
						vertices[3] = offsetPos + n;

						Quad q;
						q.is_back_face = isBackFace;
						for (int i = 0; i < sizeof(vertices) / sizeof(vertices[0]); i++) {
							q.corners[i] = vertices[i];
						}
						q.block = (uint8_t)startBlock;

						q.size = 1;
						for (int i = 0; i < 3; i++) {
							if (vertices[0][i] == vertices[2][i]) continue;
							q.size *= abs(vertices[0][i] - vertices[2][i]);
							if (q.size < 0) {
								OutputDebugString("");
							}
						}

						mesh->quads.push_back(q);

						// Mark it merged
						for (int f = 0; f < quadSize[workAxis1]; f++) {
							for (int g = 0; g < quadSize[workAxis2]; g++) {
								merged[startPos[workAxis1] + f][startPos[workAxis2] + g] = true;
							}
						}
					}
				}
			}
		}

		OutputDebugString("Printing mesh:\n");
		char buf[256];

		for (int i = 0; i < mesh->quads.size(); i++) {
			sprintf(buf, "\t(%d, %d, %d) -> (%d, %d, %d) [%d]\n", mesh->quads[i].corners[0][0], mesh->quads[i].corners[0][1], mesh->quads[i].corners[0][2], mesh->quads[i].corners[2][0], mesh->quads[i].corners[2][1], mesh->quads[i].corners[2][2], mesh->quads[i].block);
			OutputDebugString(buf);
		}

		return mesh;
	}

	inline bool IsBlockFaceVisible(MiniChunk* mini, vmath::ivec3 startPos, int direction, bool isBackFace) {
		vmath::ivec3 dir_vec = { 0, 0, 0 };
		dir_vec[direction] = isBackFace ? -1 : 1;

		return get_type(mini->real_coords() + startPos + dir_vec) == Block::Air;
	}

	inline bool CompareStep(MiniChunk* mini, vmath::ivec3 a, vmath::ivec3 b, int direction, bool backFace) {
		Block blockA = mini->get_block(a);
		Block blockB = mini->get_block(b);

		return blockA == blockB && blockB != Block::Air && IsBlockFaceVisible(mini, b, direction, backFace);
	}

	inline MiniChunkMesh* gen_minichunk_mesh2(MiniChunk* mini) {
		// DEBUG: Check why mesh at (0,0,0) is fucking awful
		if (mini->coords[0] == 0 && mini->coords[1] == 0 && mini->coords[2] == 0) {
			OutputDebugString("");
		}
		MiniChunkMesh* mesh = new MiniChunkMesh();


		return mesh;
	}

	// given 2D array of block numbers, generate optimal quads
	static inline vector<Quad2D> gen_quads(Block(&layer)[16][16]) {
		bool merged[16][16];
		memset(merged, false, 16 * 16 * sizeof(bool));

		vector<Quad2D> result;

		for (int i = 0; i < 16; i++) {
			for (int j = 0; j < 16; j++) {
				// skip merged blocks
				if (merged[i][j]) continue;

				Block block = layer[i][j];

				// skip air
				if (block == Block::Air) continue;

				// get max size of this quad
				ivec2 max_size = get_max_size(layer, merged, { i, j }, block);

				// add it to results
				ivec2 start = { i, j };
				Quad2D q;
				q.block = block;
				q.corners[0] = start;
				q.corners[1] = start + max_size;

				// mark all as merged
				mark_as_merged(merged, start, max_size);

				// wew
				result.push_back(q);
			}
		}

		return result;
	}

	static inline void mark_as_merged(bool(&merged)[16][16], ivec2 &start, ivec2 &max_size) {
		for (int i = start[0]; i < start[0] + max_size[0]; i++) {
			for (int j = start[1]; j < start[1] + max_size[1]; j++) {
				merged[i][j] = true;
			}
		}
	}

	// given a layer and start point, find its best dimensions
	static inline ivec2 get_max_size(Block layer[16][16], bool merged[16][16], ivec2 start_point, Block block_type) {
		assert(block_type != Block::Air);

		// TODO: Start max size at {1,1}, and for loops at +1.
		// TODO: Search width with find() instead of a for loop.

		// "max width and height"
		ivec2 max_size = { 0, 0 };

		// maximize width
		for (int i = start_point[0], j = start_point[1]; i < 16; i++) {
			// if extended by 1, add 1 to max width
			if (layer[i][j] == block_type && !merged[i][j]) {
				max_size[0]++;
			}
			// else give up
			else {
				break;
			}
		}

		assert(max_size[0] > 0 && "WTF? Max width is 0? Doesn't make sense.");

		// now that we've maximized width, need to
		// maximize height

		// for each height
		for (int j = start_point[1]; j < 16; j++) {
			// check if entire width is correct
			for (int i = start_point[0]; i < start_point[0] + max_size[0]; i++) {
				// if wrong block type, give up on extending height
				if (layer[i][j] != block_type || merged[i][j]) {
					break;
				}
			}

			// yep, entire width is correct! Extend max height and keep going
			max_size[1]++;
		}

		assert(max_size[1] > 0 && "WTF? Max height is 0? Doesn't make sense.");

		return max_size;
	}

	// check if a block's face is visible
	inline bool is_face_visible(vmath::ivec4 block_coords, int face) {
		return get_type(block_coords + face_to_direction(face)) == Block::Air;
	}

	// check if a block's face is visible
	inline bool is_face_visible(int x, int y, int z, int face) { return is_face_visible(vmath::ivec4(x, y, z, 0), face); }

	//// check if a block's face is visible
	//inline bool is_face_visible(vmath::ivec4 block_coords, vmath::ivec4 axis, int backface) {
	//	return get_type(block_coords + face_to_direction(face)) == Block::Air;
	//}
};

#endif /* __WORLD_H__ */
