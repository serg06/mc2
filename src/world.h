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
#include <unordered_set>
#include <utility>
#include <vector>
#include <vmath.h>
#include <windows.h>

using namespace std;

class Quad2D {
public:
	Block block;
	ivec2 corners[2];
};
inline bool operator==(const Quad2D& lhs, const Quad2D& rhs) {
	auto &lc1 = lhs.corners[0];
	auto &lc2 = lhs.corners[1];

	auto &rc1 = rhs.corners[0];
	auto &rc2 = rhs.corners[1];

	return lhs.block == rhs.block &&
		((lc1[0] == rc1[0] && lc1[1] == rc1[1] && lc2[0] == rc2[0] && lc2[1] == rc2[1]) ||
		(lc2[0] == rc1[0] && lc2[1] == rc1[1] && lc1[0] == rc2[0] && lc1[1] == rc2[1]));
}

namespace WorldTests {
	void test_gen_quads();
	void run_all_tests();
	void test_mark_as_merged();
	void test_get_max_size();
	void test_gen_layer();
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
			gen_chunk(x, z);
		}

		return chunk_map[{x, z}];
	}

	// get multiple chunks -- much faster than get_chunk_generate_if_required when n > 1
	inline std::unordered_set<Chunk*, chunk_hash> get_chunks_generate_if_required(vector<vmath::ivec2> chunk_coords) {
		// don't wanna get duplicates
		std::unordered_set<Chunk*, chunk_hash> result;

		// gen if required
		gen_chunks_if_required(chunk_coords);

		// fetch
		for (auto coords : chunk_coords) {
			result.insert(chunk_map[{coords[0], coords[1]}]);
		}

		return result;
	}

	// generate chunks if they don't exist yet
	inline void gen_chunks_if_required(vector<vmath::ivec2> chunk_coords) {
		// don't wanna generate duplicates
		std::unordered_set<ivec2, vecN_hash> to_generate;

		for (auto coords : chunk_coords) {
			auto search = chunk_map.find({ coords[0], coords[1] });

			// if doesn't exist, need to generate it
			if (search == chunk_map.end()) {
				to_generate.insert(coords);
			}
		}

		gen_chunks(to_generate);
	}

	inline void gen_chunks(vector<ivec2> to_generate) {
		std::unordered_set<ivec2, vecN_hash> set;
		for (auto coords : to_generate) {
			set.insert(coords);
		}
		return gen_chunks(set);
	}

	// generate chunk at (x, z) and add it
	inline void gen_chunks(std::unordered_set<ivec2, vecN_hash> to_generate) {
		// get pointers ready
		Chunk** chunks = (Chunk**)malloc(sizeof(Chunk*) * to_generate.size());

		// generate chunks and set pointers
		int i = 0;
		for (auto coords : to_generate) {
			// generate it
			Chunk* c = gen_chunk_data(coords);

			// add it to world so that we can use get_type in following code
			add_chunk(coords[0], coords[1], c);

			// add it to our pointers
			chunks[i] = c;

			i++;
		}

		// chunks we need to generate minis for
		std::unordered_set<Chunk*, chunk_hash> to_generate_minis;

		// for every chunk we just generated
		for (int i = 0; i < to_generate.size(); i++) {
			// get chunk
			Chunk* chunk = chunks[i];

			// need to generate minis for it
			to_generate_minis.insert(chunk);

			// and need to regenerate minis for its neighbors
			for (auto coords : chunk->surrounding_chunks()) {
				// get the neighbor
				Chunk* neighbor = get_chunk(coords[0], coords[1]);

				// if neighbor exists, add it to lists of chunks we need to regenerate minis in
				if (neighbor != nullptr) {
					to_generate_minis.insert(neighbor);
				}
			}
		}

		// now that we know everyone who needs new minis, let's do it
		for (auto chunk : to_generate_minis) {
			for (auto &mini : chunk->minis) {
				mini.invisible = mini.invisible || mini.all_air() || check_if_covered(mini);

				if (!mini.invisible) {
					mini.mesh = gen_minichunk_mesh(&mini);
					mini.update_quads_buf();
				}
			}
		}
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
		//if (chunk_coords[0] != 0 || chunk_coords[1] != 0) {
		//	return;
		//}

		vector<ivec2> coords;

		for (int i = -distance; i <= distance; i++) {
			for (int j = -(distance - abs(i)); j <= distance - abs(i); j++) {
				coords.push_back({ chunk_coords[0] + i, chunk_coords[1] + j });
			}
		}

		gen_chunks_if_required(coords);
	}

	// get chunk that contains block at (x, _, z)
	inline Chunk* get_chunk_containing_block(int x, int z) {
		return get_chunk((int)floorf((float)x / 16.0f), (int)floorf((float)z / 16.0f));
	}

	// get chunk-coordinates of chunk containing the block at (x, _, z)
	inline ivec2 get_chunk_coords(int x, int z) {
		return { (int)floorf((float)x / 16.0f), (int)floorf((float)z / 16.0f) };
	}

	// get minichunk-coordinates of minichunk containing the block at (x, y, z)
	inline ivec3 get_mini_coords(int x, int y, int z) {
		return { (int)floorf((float)x / 16.0f), (y / 16) * 16, (int)floorf((float)z / 16.0f) };
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
	inline void gen_chunk(int x, int z) {
		vector<ivec2> coords;
		coords.push_back({ x, z });
		gen_chunks(coords);
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
		//sprintf(buf, "[%d] Rendering all chunks\n", rendered);
		//OutputDebugString(buf);

		for (auto &[coords_p, chunk] : chunk_map) {
			chunk->render(glInfo);
		}

		rendered++;
	}

	static inline void gen_working_indices(int &layers_idx, int &working_idx_1, int &working_idx_2) {
		// working indices are always gonna be xy, xz, or yz.
		working_idx_1 = layers_idx == 0 ? 1 : 0;
		working_idx_2 = layers_idx == 2 ? 1 : 2;
	}

	inline MiniChunkMesh* gen_minichunk_mesh(MiniChunk* mini) {
		// got our mesh
		MiniChunkMesh* mesh = new MiniChunkMesh();

		// for all 6 sides
		for (int i = 0; i < 6; i++) {
			bool backface = i < 3;
			int layers_idx = i % 3;

			// working indices are always gonna be xy, xz, or yz.
			int working_idx_1, working_idx_2;
			gen_working_indices(layers_idx, working_idx_1, working_idx_2);

			// generate face variable
			ivec3 face = { 0, 0, 0 };
			// I don't think it matters whether we start with front or back face, as long as we switch halfway through.
			// BACKFACE => +X/+Y/+Z SIDE. 
			face[layers_idx] = backface ? -1 : 1;

			// for each layer
			for (int i = 0; i < 16; i++) {
				Block layer[16][16];

				// extract it from the data
				gen_layer(mini, layers_idx, i, face, layer);

				int num_air = 0;
				for (int i = 0; i < 16; i++) {
					for (int j = 0; j < 16; j++) {
						if (layer[i][j] == Block::Air) {
							num_air++;
						}
					}
				}
				int num_not_air = 16 * 16 - num_air;

				// get quads from layer
				vector<Quad2D> quads2d = gen_quads(layer);

				// if +x, -y, or +z, flip triangles around so that we're not drawing them backwards
				if (face[0] > 0 || face[1] < 0 || face[2] > 0) {
					for (int i = 0; i < quads2d.size(); i++) {
						ivec2 diffs = quads2d[i].corners[1] - quads2d[i].corners[0];
						quads2d[i].corners[1] = quads2d[i].corners[0] + ivec2(0, diffs[1]);
						quads2d[i].corners[0] = quads2d[i].corners[0] + ivec2(diffs[0], 0);
					}
				}

				// convert quads back to 3D coordinates
				vector<Quad3D> quads = quads_2d_3d(quads2d, layers_idx, i, face);

				// if -x, -y, or -z, move 1 forwards
				if (face[0] > 0 || face[1] > 0 || face[2] > 0) {
					for (int i = 0; i < quads.size(); i++) {
						quads[i].corners[0] += face;
						quads[i].corners[1] += face;
					}
				}

				// append quads
				for (auto quad : quads) {
					mesh->quads3d.push_back(quad);
				}
			}
		}

		return mesh;
	}

	// convert 2D quads to 3D quads
	// face: for offset
	static inline vector<Quad3D> quads_2d_3d(vector<Quad2D> &quads2d, int layers_idx, int layer_no, ivec3 face) {
		vector<Quad3D> result;

		// working variable
		Quad3D quad3d;

		// working indices are always gonna be xy, xz, or yz.
		int working_idx_1, working_idx_2;
		gen_working_indices(layers_idx, working_idx_1, working_idx_2);

		// for each quad
		for (auto quad2d : quads2d) {
			// set block
			quad3d.block = (uint8_t)quad2d.block;

			// for each corner in the quad
			for (int i = 0; i < 2; i++) {
				// set 3D coordinates
				quad3d.corners[i][layers_idx] = layer_no;
				quad3d.corners[i][working_idx_1] = quad2d.corners[i][0];
				quad3d.corners[i][working_idx_2] = quad2d.corners[i][1];
			}

			result.push_back(quad3d);
		}

		return result;
	}

	// extract a layer from data, replacing covered faces with air, and checking covered faces via `face` variable
	// layers_idx: The index of the coordinate in {x, y, z} that we're currently traversing, layer by layer.
	inline void gen_layer(MiniChunk* mini, int layers_idx, int layer_no, ivec3 face, Block(&result)[16][16]) {
		// NOTE: Regardless of what order we're doing layers,
		//       `face` = face that we wanna check. So e.g. if you wanna check face on -x side, need to pass (-1, 0, 0);
		assert(length(face) == 1);

		// error check
		assert(face[layers_idx] && "wrong face, fool");

		// working indices are always gonna be xy, xz, or yz.
		int working_idx_1, working_idx_2;
		gen_working_indices(layers_idx, working_idx_1, working_idx_2);

		assert(layers_idx != working_idx_1 && working_idx_1 != working_idx_2 && working_idx_2 != layers_idx);
		assert(layers_idx + working_idx_1 + working_idx_2 == 3);

		// TODO: Use mini.get_block() whenever possible, it's way faster then world.get_type()!
		// TODO: Maybe make this method static by passing in surrounding minis.

		ivec3 minichunk_offset = { mini->coords[0] * 16, mini->coords[1], mini->coords[2] * 16 };

		for (int u = 0; u < 16; u++) {
			for (int v = 0; v < 16; v++) {
				ivec3 coords;
				coords[layers_idx] = layer_no;
				coords[working_idx_1] = u;
				coords[working_idx_2] = v;

				ivec3 face_coords = coords + face;

				Block block = mini->get_block(coords);
				Block face_block;

				// if in range of mini, get face block via mini's data
				if (in_range(face_coords, ivec3(0, 0, 0), ivec3(15, 15, 15))) {
					face_block = mini->get_block(face_coords);
				}
				else {
					ivec3 world_coords = minichunk_offset + coords;
					face_block = get_type(world_coords + face);
				}


				// if block invisible or face not visible, set to air
				if (block == Block::Air || face_block != Block::Air) {
					result[u][v] = Block::Air;
				}
				else {
					result[u][v] = block;
				}
			}
		}
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
		assert(!merged[start_point[0]][start_point[1]] && "bruh");

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
		bool stop = false;
		for (int j = start_point[1]; j < 16; j++) {
			// check if entire width is correct
			for (int i = start_point[0]; i < start_point[0] + max_size[0]; i++) {
				// if wrong block type, give up on extending height
				if (layer[i][j] != block_type || merged[i][j]) {
					stop = true;
					break;
				}
			}

			if (stop) {
				break;
			}

			// yep, entire width is correct! Extend max height and keep going
			max_size[1]++;
		}

		assert(max_size[1] > 0 && "WTF? Max height is 0? Doesn't make sense.");

		return max_size;
	}

	//// check if a block's face is visible
	//inline bool is_face_visible(vmath::ivec4 block_coords, vmath::ivec4 axis, int backface) {
	//	return get_type(block_coords + face_to_direction(face)) == Block::Air;
	//}

	// position you're at
	// direction you're looking at
	inline void render_outline_of_forwards_block(vec4 position, vec4 direction) {

	}

	//// todo: understand
	//inline float intBound(s, ds) {
	//	return ds > 0 ? (Math.ceil(s) - s) / ds : (s - Math.floor(s)) / -ds;
	//}

	static inline float intbound(float s, float ds)
	{
		// Some kind of edge case, see:
		// http://gamedev.stackexchange.com/questions/47362/cast-ray-to-select-block-in-voxel-game#comment160436_49423
		bool sIsInteger = round(s) == s;
		if (ds < 0 && sIsInteger) {
			return 0;
		}

		return (ds > 0 ? ceil(s) - s : s - floor(s)) / abs(ds);
	}

	inline void highlight_block(OpenGLInfo* glInfo, int x, int y, int z) {
		// Figure out corners / block types
		Block blocks[6];
		ivec3 corner1s[6];
		ivec3 corner2s[6];

		ivec3 mini_coords = get_mini_coords(x, y, z);

		ivec3 relative_coords = get_chunk_relative_coordinates(x, y, z);
		ivec3 block_coords = { relative_coords[0], y % 16, relative_coords[2] };

		for (int i = 0; i < 6; i++) {
			blocks[i] = (Block)100; // outline
		}

		// SOUTH
		//	bottom-left corner
		corner1s[0] = block_coords + ivec3(0, 0, 1);
		//	top-right corner
		corner2s[0] = block_coords + ivec3(1, 1, 1);

		// NORTH
		//	bottom-left corner
		corner1s[1] = block_coords + ivec3(1, 0, 0);
		//	top-right corner
		corner2s[1] = block_coords + ivec3(0, 1, 0);

		// EAST
		//	bottom-left corner
		corner1s[2] = block_coords + ivec3(1, 0, 1);
		//	top-right corner
		corner2s[2] = block_coords + ivec3(1, 1, 0);

		// WEST
		//	bottom-left corner
		corner1s[3] = block_coords + ivec3(0, 0, 0);
		//	top-right corner
		corner2s[3] = block_coords + ivec3(0, 1, 1);

		// UP
		//	bottom-left corner
		corner1s[4] = block_coords + ivec3(0, 1, 0);
		//	top-right corner
		corner2s[4] = block_coords + ivec3(1, 1, 1);

		// DOWN
		//	bottom-left corner
		corner1s[5] = block_coords + ivec3(1, 0, 1);
		//	top-right corner
		corner2s[5] = block_coords + ivec3(0, 0, 0);

		char buf[256];
		char *str1 = vec2str(relative_coords), *str2 = vec2str(block_coords), *str3 = vec2str(mini_coords);
		sprintf(buf, "Gonna highlight block %s [i.e. block %s in minichunk %s].\n", str1, str2, str3);
		OutputDebugString(buf);


		GLuint quad_block_type_buf, quad_corner1_buf, quad_corner2_buf;

		// create buffers with just the right sizes
		glCreateBuffers(1, &quad_block_type_buf);
		glCreateBuffers(1, &quad_corner1_buf);
		glCreateBuffers(1, &quad_corner2_buf);

		// allocate them just enough space
		glNamedBufferStorage(quad_block_type_buf, sizeof(Block) * 6, NULL, GL_DYNAMIC_STORAGE_BIT); // allocate 2 matrices of space for transforms, and allow editing
		glNamedBufferStorage(quad_corner1_buf, sizeof(ivec3) * 6, NULL, GL_DYNAMIC_STORAGE_BIT); // allocate 2 matrices of space for transforms, and allow editing
		glNamedBufferStorage(quad_corner2_buf, sizeof(ivec3) * 6, NULL, GL_DYNAMIC_STORAGE_BIT); // allocate 2 matrices of space for transforms, and allow editing

		// fill 'em up!
		glNamedBufferSubData(quad_block_type_buf, 0, sizeof(Block) * 6, blocks);
		glNamedBufferSubData(quad_corner1_buf, 0, sizeof(ivec3) * 6, corner1s);
		glNamedBufferSubData(quad_corner2_buf, 0, sizeof(ivec3) * 6, corner2s);

		// DRAW!

		// quad VAO
		glBindVertexArray(glInfo->vao_quad);

		// bind to quads attribute binding point
		glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->quad_block_type_bidx, quad_block_type_buf, 0, sizeof(Block));
		glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->q_corner1_bidx, quad_corner1_buf, 0, sizeof(ivec3));
		glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->q_corner2_bidx, quad_corner2_buf, 0, sizeof(ivec3));

		// write this chunk's coordinate to coordinates buffer
		glNamedBufferSubData(glInfo->trans_buf, TRANSFORM_BUFFER_COORDS_OFFSET, sizeof(ivec3), mini_coords);

		// DRAW!
		glDisable(GL_CULL_FACE);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 6, 6);
		glEnable(GL_CULL_FACE);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		// unbind VAO jic
		glBindVertexArray(0);

		// DELETE
		glDeleteBuffers(1, &quad_block_type_buf);
		glDeleteBuffers(1, &quad_corner1_buf);
		glDeleteBuffers(1, &quad_corner2_buf);
	}

	inline bool raycast_callback(OpenGLInfo* glInfo, int x, int y, int z, ivec3 face) {
		char buf[256];
		if (get_type(x, y, z) != Block::Air) {
			sprintf(buf, "Found Block at location (%d, %d, %d)!\n", x, y, z);
			OutputDebugString(buf);

			highlight_block(glInfo, x, y, z);

			return true;
		}
		return false;
	}



	/**
	* Call the callback with (x,y,z,value,face) of all blocks along the line
	* segment from point 'origin' in vector direction 'direction' of length
	* 'radius'. 'radius' may be infinite.
	*
	* 'face' is the normal vector of the face of that block that was entered.
	* It should not be used after the callback returns.
	*
	* If the callback returns a true value, the traversal will be stopped.
	*/
	inline void raycast(OpenGLInfo* glInfo, vec4 origin, vec4 direction, int radius) {
		// From "A Fast Voxel Traversal Algorithm for Ray Tracing"
		// by John Amanatides and Andrew Woo, 1987
		// <http://www.cse.yorku.ca/~amana/research/grid.pdf>
		// <http://citeseer.ist.psu.edu/viewdoc/summary?doi=10.1.1.42.3443>
		// Extensions to the described algorithm:
		//   • Imposed a distance limit.
		//   • The face passed through to reach the current cube is provided to
		//     the callback.

		// The foundation of this algorithm is a parameterized representation of
		// the provided ray,
		//                    origin + t * direction,
		// except that t is not actually stored; rather, at any given point in the
		// traversal, we keep track of the *greater* t values which we would have
		// if we took a step sufficient to cross a cube boundary along that axis
		// (i.e. change the integer part of the coordinate) in the variables
		// tMaxX, tMaxY, and tMaxZ

		// Cube containing origin point.
		int x = (int)floorf(origin[0]);
		int y = (int)floorf(origin[1]);
		int z = (int)floorf(origin[2]);
		// Break out direction vector.
		float dx = direction[0];
		float dy = direction[1];
		float dz = direction[2];
		// Direction to increment x,y,z when stepping.
		// TODO: Consider case where sgn = 0.
		int stepX = sgn(dx);
		int stepY = sgn(dy);
		int stepZ = sgn(dz);
		// See description above. The initial values depend on the fractional
		// part of the origin.
		float tMaxX = intbound(origin[0], dx);
		float tMaxY = intbound(origin[1], dy);
		float tMaxZ = intbound(origin[2], dz);
		// The change in t when taking a step (always positive).
		float tDeltaX = stepX / dx;
		float tDeltaY = stepY / dy;
		float tDeltaZ = stepZ / dz;
		// Buffer for reporting faces to the callback.
		ivec3 face = { 0, 0, 0 };

		// Avoids an infinite loop.
		if (dx == 0 && dy == 0 && dz == 0) {
			throw "Raycast in zero direction!";
		}

		// Rescale from units of 1 cube-edge to units of 'direction' so we can
		// compare with 't'.

		radius /= sqrt(dx*dx + dy * dy + dz * dz);

		//while (/* ray has not gone past bounds of world */
		//	(stepX > 0 ? x < wx : x >= 0) &&
		//	(stepY > 0 ? y < wy : y >= 0) &&
		//	(stepZ > 0 ? z < wz : z >= 0)) {

		while (/* ray has not gone past bounds of radius */
			true) {

			// Invoke the callback, unless we are not *yet* within the bounds of the
			// world.
			//if (!(x < 0 || y < 0 || z < 0 /*|| x >= wx || y >= wy || z >= wz*/)) {

			//}

			// callback
			if (raycast_callback(glInfo, x-2, y, z-2, face)) {
				break;
			}


			// tMaxX stores the t-value at which we cross a cube boundary along the
			// X axis, and similarly for Y and Z. Therefore, choosing the least tMax
			// chooses the closest cube boundary. Only the first case of the four
			// has been commented in detail.
			if (tMaxX < tMaxY) {
				if (tMaxX < tMaxZ) {
					if (tMaxX > radius) break;
					// Update which cube we are now in.
					x += stepX;
					// Adjust tMaxX to the next X-oriented boundary crossing.
					tMaxX += tDeltaX;
					// Record the normal vector of the cube face we entered.
					face[0] = -stepX;
					face[1] = 0;
					face[2] = 0;
				}
				else {
					if (tMaxZ > radius) break;
					z += stepZ;
					tMaxZ += tDeltaZ;
					face[0] = 0;
					face[1] = 0;
					face[2] = -stepZ;
				}
			}
			else {
				if (tMaxY < tMaxZ) {
					if (tMaxY > radius) break;
					y += stepY;
					tMaxY += tDeltaY;
					face[0] = 0;
					face[1] = -stepY;
					face[2] = 0;
				}
				else {
					// Identical to the second case, repeated for simplicity in
					// the conditionals.
					if (tMaxZ > radius) break;
					z += stepZ;
					tMaxZ += tDeltaZ;
					face[0] = 0;
					face[1] = 0;
					face[2] = -stepZ;
				}
			}
		}
	}
};

#endif /* __WORLD_H__ */
