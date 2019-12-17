#ifndef __WORLD_H__
#define __WORLD_H__

#include "chunk.h"
#include "chunkdata.h"
#include "minichunkmesh.h"
#include "render.h"
#include "shapes.h"
#include "util.h"

#include "GL/gl3w.h"
#include "GLFW/glfw3.h"

#include <assert.h>
#include <chrono>
#include <functional>
#include <queue>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <vmath.h>
#include <windows.h>

using namespace std;

// MUST MATCH STRUCT IN gen_quads compute shader
struct Quad2DCS {
	ivec2 corners[2] = { ivec2(0, 0), ivec2(0, 0) };
	GLuint block = 0;
	GLuint layer_idx = 0;
};

//// MUST MATCH STRUCT IN gen_quads compute shader
//struct Quad3DCS {
//	uvec3 coords[2];
//	GLuint block;
//  GLuint global_face_idx;
//};

// MUST MATCH STRUCT IN gen_quads compute shader
struct Quad3DCS {
	// DEBUG: set back to uvec4
	ivec4 coords[2];
	GLuint block;
	GLuint global_face_idx;
	GLuint mini_input_idx;
	GLuint empty;
};

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

	return
		(lhs.block == rhs.block) &&
		((lc1 == rc1 && lc2 == rc2) || (lc2 == rc1 && lc1 == rc2));
}

namespace WorldTests {
	void run_all_tests(OpenGLInfo* glInfo);
	void test_gen_quads();
	void test_mark_as_merged();
	void test_get_max_size();
	void test_gen_layer();
	void test_gen_layers_compute_shader(OpenGLInfo *glInfo);
}

// represents an in-game world
class World {
public:
	// map of (chunk coordinate) -> chunk
	unordered_map<vmath::ivec2, Chunk*, vecN_hash> chunk_map;

	int rendered = 0;
	// get_chunk cache
	Chunk* chunk_cache[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
	vmath::ivec2 chunk_cache_ivec2[5] = { ivec2(INT_MAX), ivec2(INT_MAX), ivec2(INT_MAX), ivec2(INT_MAX), ivec2(INT_MAX) };
	int chunk_cache_clock_hand = 0; // clock cache
	OpenGLInfo *glInfo;
	GLsync chunk_sync = NULL;
	//queue<vmath::ivec2> chunk_gen_queue;
	// minis currently having their meshes generated
	queue<MiniChunk*> chunk_gen_mini_queue; // todo: make this ivec3 instead?
	vector<MiniChunk*> chunk_gen_minis; // todo: make this ivec3 instead?
	std::chrono::time_point<std::chrono::high_resolution_clock> last_chunk_sync_check = std::chrono::high_resolution_clock::now();

	World() {

	}

	// add chunk to chunk coords (x, z)
	inline void add_chunk(int x, int z, Chunk* chunk) {
		ivec2 coords = { x, z };
		auto search = chunk_map.find(coords);

		// if element already exists, error
		if (search != chunk_map.end()) {
			throw "Tried to add chunk but it already exists.";
		}

		// insert our chunk
		if (chunk == nullptr) {
			throw "Wew";
		}
		chunk_map[coords] = chunk;

		// invalidate that chunk in cache
		for (int i = 0; i < 5; i++) {
			if (chunk_cache_ivec2[i] == coords) {
				chunk_cache[i] = nullptr;
				chunk_cache_ivec2[i] = ivec2(INT_MAX);
			}
		}

		//char buf[256];
		//sprintf(buf, "Loaded chunks: %d\n", chunk_map.size());
		//OutputDebugString(buf);
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
			result.insert(chunk_map[coords]);
		}

		return result;
	}

	// generate chunks if they don't exist yet
	inline void gen_chunks_if_required(vector<vmath::ivec2> chunk_coords) {
		// don't wanna generate duplicates
		std::unordered_set<ivec2, vecN_hash> to_generate;

		for (auto coords : chunk_coords) {
			auto search = chunk_map.find(coords);

			// if doesn't exist, need to generate it
			if (search == chunk_map.end()) {
				to_generate.insert(coords);
			}
		}

		if (to_generate.size() > 0) {
			gen_chunks(to_generate);
		}
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
		Chunk** chunks = new Chunk*[to_generate.size()];

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

		// figure out all minis to mesh
		vector<MiniChunk*> minis_to_mesh;
		for (auto chunk : to_generate_minis) {
			for (auto &mini : chunk->minis) {
				mini.invisible = mini.invisible || mini.all_air() || check_if_covered(mini);

				if (!mini.invisible) {
					minis_to_mesh.push_back(&mini);
				}
			}
		}

		// DEBUG
		// add them all to queue
		for (auto &mini : minis_to_mesh) {
			chunk_gen_mini_queue.push(mini);
			//char buf[256];
			//sprintf(buf, "Adding mini (%d, %d, %d) to queue...\n", mini->coords[0], mini->coords[1], mini->coords[2]);
			//OutputDebugString(buf);
		}




		//// generate all meshes
		//// TODO: free this
		//vector<MiniChunkMesh*> meshes;
		//for (int i = 0; i < minis_to_mesh.size(); i++) {
		//	meshes.push_back(new MiniChunkMesh);
		//}
		//// DEBUG
		////gen_minichunk_meshes_gpu(glInfo, minis_to_mesh, meshes);
		//gen_minichunk_meshes_gpu_fast(glInfo, minis_to_mesh, meshes);

		//assert(minis_to_mesh.size() == meshes.size());

		//// load assign meshes to minis
		//for (int i = 0; i < minis_to_mesh.size(); i++) {
		//	MiniChunkMesh* mesh = meshes[i];

		//	MiniChunkMesh* non_water = new MiniChunkMesh;
		//	MiniChunkMesh* water = new MiniChunkMesh;

		//	for (auto &quad : mesh->quads3d) {
		//		if ((Block)quad.block == Block::Water) {
		//			water->quads3d.push_back(quad);
		//		}
		//		else {
		//			non_water->quads3d.push_back(quad);
		//		}
		//	}

		//	assert(mesh->size() == non_water->size() + water->size());

		//	minis_to_mesh[i]->mesh = non_water;
		//	minis_to_mesh[i]->water_mesh = water;

		//	minis_to_mesh[i]->update_quads_buf();
		//}



		//for (auto chunk : to_generate_minis) {
		//	for (auto &mini : chunk->minis) {
		//		mini.invisible = mini.invisible || mini.all_air() || check_if_covered(mini);

		//		if (!mini.invisible) {
		//			//MiniChunkMesh* mesh = gen_minichunk_mesh(&mini);
		//			MiniChunkMesh* mesh = gen_minichunk_mesh_gpu(glInfo, &mini);

		//			MiniChunkMesh* non_water = new MiniChunkMesh;
		//			MiniChunkMesh* water = new MiniChunkMesh;

		//			for (auto &quad : mesh->quads3d) {
		//				if ((Block)quad.block == Block::Water) {
		//					water->quads3d.push_back(quad);
		//				}
		//				else {
		//					non_water->quads3d.push_back(quad);
		//				}
		//			}

		//			assert(mesh->size() == non_water->size() + water->size());

		//			mini.mesh = non_water;
		//			mini.water_mesh = water;

		//			mini.update_quads_buf();
		//		}
		//	}
		//}

		// delete malloc'd stuff
		delete[] chunks;
	}

	// get chunk or nullptr (using cache) (TODO: LRU?)
	inline Chunk* get_chunk(int x, int z) {
		ivec2 coords = { x, z };

		// if in cache, return
		for (int i = 0; i < 5; i++) {
			// start at chunk_cache_clock_hand and search backwards
			if (chunk_cache_ivec2[(chunk_cache_clock_hand - i + 5) % 5] == coords) {
				return chunk_cache[(chunk_cache_clock_hand - i + 5) % 5];
			}
		}

		// not in cache, get normally
		Chunk* result = get_chunk_(x, z);

		// save in cache
		chunk_cache_clock_hand = (chunk_cache_clock_hand + 1) % 5;
		chunk_cache[chunk_cache_clock_hand] = result;
		chunk_cache_ivec2[chunk_cache_clock_hand] = coords;

		return result;
	}

	inline Chunk* get_chunk(ivec2 &xz) { return get_chunk(xz[0], xz[1]); }

	// get chunk or nullptr (no cache)
	inline Chunk* get_chunk_(int x, int z) {
		auto search = chunk_map.find({ x, z });

		// if doesn't exist, return null
		if (search == chunk_map.end()) {
			return nullptr;
		}

		return (*search).second;
	}

	// get mini or nullptr
	inline MiniChunk* get_mini(int x, int y, int z) {
		auto search = chunk_map.find({ x, z });

		// if chunk doesn't exist, return null
		if (search == chunk_map.end()) {
			return nullptr;
		}

		Chunk* chunk = (*search).second;
		return chunk->get_mini_with_y_level((y / 16) * 16);
	}

	inline MiniChunk* get_mini(ivec3 xyz) { return get_mini(xyz[0], xyz[1], xyz[2]); }


	// generate chunks near player
	inline void gen_nearby_chunks(vmath::vec4 position, int distance) {
		assert(distance >= 0 && "invalid distance");

		ivec2 chunk_coords = get_chunk_coords((int)floorf(position[0]), (int)floorf(position[2]));

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

	// get minichunk that contains block at (x, y, z)
	inline MiniChunk* get_mini_containing_block(int x, int y, int z) {
		Chunk* chunk = get_chunk_containing_block(x, z);
		return chunk->get_mini_with_y_level((y / 16) * 16);
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
		return vmath::ivec3(((x % CHUNK_WIDTH) + CHUNK_WIDTH) % 16, y, ((z % CHUNK_DEPTH) + CHUNK_DEPTH) % 16);
	}

	// given a block's real-world coordinates, return that block's coordinates relative to its chunk
	inline vmath::ivec3 get_mini_relative_coords(int x, int y, int z) {
		// adjust x and y
		x = x % MINICHUNK_WIDTH;
		y = y % MINICHUNK_HEIGHT;
		z = z % MINICHUNK_DEPTH;

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

		if (!chunk) {
			return Block::Air;
		}

		vmath::ivec3 chunk_coords = get_chunk_relative_coordinates(x, y, z);
		return chunk->get_block(chunk_coords);
	}

	inline Block get_type(vmath::ivec3 xyz) { return get_type(xyz[0], xyz[1], xyz[2]); }
	inline Block get_type(vmath::ivec4 xyz_) { return get_type(xyz_[0], xyz_[1], xyz_[2]); }

	// generate chunk at (x, z) and add it
	inline void gen_chunk(int x, int z) {
		vector<ivec2> coords;
		coords.push_back({ x, z });
		gen_chunks(coords);
	}

	inline bool check_if_covered(MiniChunk &mini) {
		// if contains any translucent blocks, don't know how to handle that yet
		if (mini.any_translucent()) {
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
						if (get_type(clamp_coords_to_world(coords + IEAST_0)).is_translucent()) return false;
					}
					// if along west wall, check west
					if (miniX == 0) {
						if (get_type(clamp_coords_to_world(coords + IWEST_0)).is_translucent()) return false;
					}

					// if along north wall, check north
					if (miniZ == 0) {
						if (get_type(clamp_coords_to_world(coords + INORTH_0)).is_translucent()) return false;
					}
					// if along south wall, check south
					if (miniZ == CHUNK_DEPTH - 1) {
						if (get_type(clamp_coords_to_world(coords + ISOUTH_0)).is_translucent()) return false;
					}

					// if along bottom wall, check bottom
					if (miniY == 0) {
						if (get_type(clamp_coords_to_world(coords + IDOWN_0)).is_translucent()) return false;
					}
					// if along top wall, check top
					if (miniY == MINICHUNK_HEIGHT - 1) {
						if (get_type(clamp_coords_to_world(coords + IUP_0)).is_translucent()) return false;
					}
				}
			}
		}

		return true;
	}

	//float(&pleft)[4], float(&pright)[4], float(&ptop)[4], float(&pbottom)[4], float(&pnear)[4], float(&pfar)[4]
	inline void render(OpenGLInfo* glInfo, const vmath::vec4(&planes)[6]) {
		char buf[256];

		auto start_of_fn = std::chrono::high_resolution_clock::now();


		// update meshes
		//update_enqueued_chunks_meshes(glInfo);
		update_enqueued_chunks_meshes_fast(glInfo);

		//sprintf(buf, "[%d] Rendering all chunks\n", rendered);
		//OutputDebugString(buf);

		//for (auto &[coords_p, chunk] : chunk_map) {
		//	chunk->render(glInfo);
		//}

		// collect all the minis we're gonna draw
		vector<MiniChunk> minis_to_draw;
		for (auto &[coords_p, chunk] : chunk_map) {
			for (auto &mini : chunk->minis) {
				if (!mini.invisible) {
					if (mini_in_frustum(&mini, planes)) {
						minis_to_draw.push_back(mini);
					}
				}
			}
		}



		//sprintf(buf, "Drawing %d/%d\tvisible minis.\n", minis_to_draw.size(), num_visible);
		//OutputDebugString(buf);

		glUseProgram(glInfo->rendering_program);

		for (auto &mini : minis_to_draw) {
			mini.render_meshes(glInfo);
		}
		for (auto mini : minis_to_draw) {
			mini.render_water_meshes(glInfo);
		}



		rendered++;
	}

	// check if a mini is visible in a frustum
	static inline bool mini_in_frustum(MiniChunk* mini, const vmath::vec4(&planes)[6]) {
		float radius = 28.0f;
		return sphere_in_frustrum(mini->center_coords_v3(), radius, planes);
	}

	static constexpr inline void gen_working_indices(int &layers_idx, int &working_idx_1, int &working_idx_2) {
		// working indices are always gonna be xy, xz, or yz.
		working_idx_1 = layers_idx == 0 ? 1 : 0;
		working_idx_2 = layers_idx == 2 ? 1 : 2;
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

			// set face
			quad3d.face = face;

			result.push_back(quad3d);
		}

		return result;
	}

	// generate layer by grabbing face blocks directly from the minichunk
	static inline void gen_layer_fast(MiniChunk* mini, int layers_idx, int layer_no, const ivec3 &face, Block(&result)[16][16]) {
		// working indices are always gonna be xy, xz, or yz.
		int working_idx_1, working_idx_2;
		gen_working_indices(layers_idx, working_idx_1, working_idx_2);

		// get coordinates of a random block
		ivec3 coords = { 0, 0, 0 };
		coords[layers_idx] = layer_no;
		ivec3 face_coords = coords + face;

		// make sure face not out of bounds
		assert(in_range(face_coords, ivec3(0, 0, 0), ivec3(15, 15, 15)) && "Face outside minichunk.");

		// reset all to air
		memset(result, (uint8_t)Block::Air, sizeof(result));

		// for each coordinate

		// if face is y, iterate on x then z (best speed)
		// if face is z, iterate on x then y (best speed)
		if (face[1] != 0 || face[2] != 0) {
			// y or z
			for (int v = 0; v < 16; v++) {
				// x
				for (int u = 0; u < 16; u++) {
					// set working indices (TODO: move u to outer loop)
					coords[working_idx_1] = u;
					coords[working_idx_2] = v;

					// get block at these coordinates
					Block block = mini->get_block(coords);

					// dgaf about air blocks
					if (block == Block::Air) {
						continue;
					}

					// get face block
					face_coords = coords + face;
					Block face_block = mini->get_block(face_coords);

					// if block's face is visible, set it
					if (is_face_visible(block, face_block)) {
						result[u][v] = block;
					}
				}
			}
		}

		// if face is x, iterate on z then y (best speed)
		else if (face[0] != 0) {
			// y
			for (int u = 0; u < 16; u++) {
				// z
				for (int v = 0; v < 16; v++) {
					// set working indices (TODO: move u to outer loop)
					coords[working_idx_1] = u;
					coords[working_idx_2] = v;

					// get block at these coordinates
					Block block = mini->get_block(coords);

					// dgaf about air blocks
					if (block == Block::Air) {
						continue;
					}

					// get face block
					face_coords = coords + face;
					Block face_block = mini->get_block(face_coords);

					// if block's face is visible, set it
					if (is_face_visible(block, face_block)) {
						result[u][v] = block;
					}
				}
			}
		}
	}

	// generate layer by grabbing face blocks using get_type()
	inline void gen_layer_slow(MiniChunk* mini, int layers_idx, int layer_no, const ivec3 &face, Block(&result)[16][16]) {
		// working indices are always gonna be xy, xz, or yz.
		int working_idx_1, working_idx_2;
		gen_working_indices(layers_idx, working_idx_1, working_idx_2);

		// coordinates of current block
		ivec3 coords = { 0, 0, 0 };
		coords[layers_idx] = layer_no;

		// minichunk's coordinates
		ivec3 minichunk_offset = mini->real_coords();

		// reset all to air
		memset(result, (uint8_t)Block::Air, sizeof(result));

		// for each coordinate

		// if face is y, iterate on x then z (best speed)
		// if face is z, iterate on x then y (best speed)
		if (face[1] != 0 || face[2] != 0) {
			// y or z
			for (int v = 0; v < 16; v++) {
				// x
				for (int u = 0; u < 16; u++) {
					// set working indices (TODO: move u to outer loop)
					coords[working_idx_1] = u;
					coords[working_idx_2] = v;

					// get block at these coordinates
					Block block = mini->get_block(coords);

					// dgaf about air blocks
					if (block == Block::Air) {
						continue;
					}

					// get face block
					Block face_block = get_type(minichunk_offset + coords + face);

					// if block's face is visible, set it
					if (is_face_visible(block, face_block)) {
						result[u][v] = block;
					}
				}
			}
		}

		// if face is x, iterate on z then y (best speed)
		else if (face[0] != 0) {
			// y
			for (int u = 0; u < 16; u++) {
				// z
				for (int v = 0; v < 16; v++) {
					// set working indices (TODO: move u to outer loop)
					coords[working_idx_1] = u;
					coords[working_idx_2] = v;

					// get block at these coordinates
					Block block = mini->get_block(coords);

					// dgaf about air blocks
					if (block == Block::Air) {
						continue;
					}

					// get face block
					Block face_block = get_type(minichunk_offset + coords + face);

					// if block's face is visible, set it
					if (is_face_visible(block, face_block)) {
						result[u][v] = block;
					}
				}
			}
		}
	}

	static inline bool is_face_visible(Block &block, Block &face_block) {
		return face_block.is_transparent() || (block != Block::Water && face_block.is_translucent()) || (face_block.is_translucent() && !block.is_translucent());
	}

	inline void gen_layer(MiniChunk* mini, int layers_idx, int layer_no, const ivec3 &face, Block(&result)[16][16]) {
		// working indices are always gonna be xy, xz, or yz.
		int working_idx_1, working_idx_2;
		gen_working_indices(layers_idx, working_idx_1, working_idx_2);

		// get coordinates of a random block
		ivec3 coords = { 0, 0, 0 };
		coords[layers_idx] = layer_no;
		ivec3 face_coords = coords + face;

		// choose function based on whether we can gather face data from inside the minichunk
		if (in_range(face_coords, ivec3(0, 0, 0), ivec3(15, 15, 15))) {
			return gen_layer_fast(mini, layers_idx, layer_no, face, result);
		}
		else {
			return gen_layer_slow(mini, layers_idx, layer_no, face, result);
		}
	}

	// given 2D array of block numbers, generate optimal quads
	static inline vector<Quad2D> gen_quads(const Block(&layer)[16][16]) {
		bool merged[16][16];
		memset(merged, false, sizeof(merged));

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
	static inline ivec2 get_max_size(const Block(&layer)[16][16], const bool(&merged)[16][16], ivec2 start_point, Block block_type) {
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

	inline void highlight_block(OpenGLInfo* glInfo, GlfwInfo* windowInfo, int x, int y, int z) {
		// Figure out corners / block types
		Block blocks[6];
		ivec3 corner1s[6];
		ivec3 corner2s[6];
		ivec3 faces[6];

		ivec3 mini_coords = get_mini_coords(x, y, z);

		ivec3 relative_coords = get_chunk_relative_coordinates(x, y, z);
		ivec3 block_coords = { relative_coords[0], y % 16, relative_coords[2] };

		for (int i = 0; i < 6; i++) {
			blocks[i] = Block::Outline; // outline
		}

		// SOUTH
		//	bottom-left corner
		corner1s[0] = block_coords + ivec3(1, 0, 1);
		//	top-right corner
		corner2s[0] = block_coords + ivec3(0, 1, 1);
		faces[0] = ivec3(0, 0, 1);

		// NORTH
		//	bottom-left corner
		corner1s[1] = block_coords + ivec3(0, 0, 0);
		//	top-right corner
		corner2s[1] = block_coords + ivec3(1, 1, 0);
		faces[1] = ivec3(0, 0, -1);

		// EAST
		//	bottom-left corner
		corner1s[2] = block_coords + ivec3(1, 1, 1);
		//	top-right corner
		corner2s[2] = block_coords + ivec3(1, 0, 0);
		faces[2] = ivec3(1, 0, 0);

		// WEST
		//	bottom-left corner
		corner1s[3] = block_coords + ivec3(0, 1, 0);
		//	top-right corner
		corner2s[3] = block_coords + ivec3(0, 0, 1);
		faces[3] = ivec3(-1, 0, 0);

		// UP
		//	bottom-left corner
		corner1s[4] = block_coords + ivec3(0, 1, 0);
		//	top-right corner
		corner2s[4] = block_coords + ivec3(1, 1, 1);
		faces[4] = ivec3(0, 1, 0);

		// DOWN
		//	bottom-left corner
		corner1s[5] = block_coords + ivec3(0, 0, 1);
		//	top-right corner
		corner2s[5] = block_coords + ivec3(1, 0, 0);
		faces[5] = ivec3(0, -1, 0);

		GLuint quad_block_type_buf, quad_corner1_buf, quad_corner2_buf, quad_face_buf;

		// create buffers with just the right sizes
		glCreateBuffers(1, &quad_block_type_buf);
		glCreateBuffers(1, &quad_corner1_buf);
		glCreateBuffers(1, &quad_corner2_buf);
		glCreateBuffers(1, &quad_face_buf);

		// allocate them just enough space
		glNamedBufferStorage(quad_block_type_buf, sizeof(Block) * 6, NULL, GL_DYNAMIC_STORAGE_BIT); // allocate 2 matrices of space for transforms, and allow editing
		glNamedBufferStorage(quad_corner1_buf, sizeof(ivec3) * 6, NULL, GL_DYNAMIC_STORAGE_BIT); // allocate 2 matrices of space for transforms, and allow editing
		glNamedBufferStorage(quad_corner2_buf, sizeof(ivec3) * 6, NULL, GL_DYNAMIC_STORAGE_BIT); // allocate 2 matrices of space for transforms, and allow editing
		glNamedBufferStorage(quad_face_buf, sizeof(ivec3) * 6, NULL, GL_DYNAMIC_STORAGE_BIT); // allocate 2 matrices of space for transforms, and allow editing

		// fill 'em up!
		glNamedBufferSubData(quad_block_type_buf, 0, sizeof(Block) * 6, blocks);
		glNamedBufferSubData(quad_corner1_buf, 0, sizeof(ivec3) * 6, corner1s);
		glNamedBufferSubData(quad_corner2_buf, 0, sizeof(ivec3) * 6, corner2s);
		glNamedBufferSubData(quad_face_buf, 0, sizeof(ivec3) * 6, faces);

		// DRAW!

		// quad VAO
		glBindVertexArray(glInfo->vao_quad);

		// bind to quads attribute binding point
		glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->quad_block_type_bidx, quad_block_type_buf, 0, sizeof(Block));
		glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->q_corner1_bidx, quad_corner1_buf, 0, sizeof(ivec3));
		glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->q_corner2_bidx, quad_corner2_buf, 0, sizeof(ivec3));
		glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->q_face_bidx, quad_face_buf, 0, sizeof(ivec3));

		// write this chunk's coordinate to coordinates buffer
		glNamedBufferSubData(glInfo->trans_buf, TRANSFORM_BUFFER_COORDS_OFFSET, sizeof(ivec3), mini_coords);

		// save properties before we overwrite them
		GLint polygon_mode; glGetIntegerv(GL_POLYGON_MODE, &polygon_mode);
		GLint cull_face = glIsEnabled(GL_CULL_FACE);
		GLint depth_test = glIsEnabled(GL_DEPTH_TEST);

		// Update projection matrix (increase near distance a bit, to fix z-fighting)
		mat4 proj_matrix = perspective(
			(float)windowInfo->vfov, // virtual fov
			(float)windowInfo->width / (float)windowInfo->height, // aspect ratio
			(PLAYER_HEIGHT - CAMERA_HEIGHT) * 1.001f / sqrtf(2.0f), // see blocks no matter how close they are
			64 * CHUNK_WIDTH // only support 64 chunks for now
		);
		glNamedBufferSubData(glInfo->trans_buf, sizeof(vmath::mat4), sizeof(proj_matrix), proj_matrix); // proj matrix

		// DRAW!
		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

		glDrawArraysInstanced(GL_TRIANGLES, 0, 6, 6);

		// restore original properties
		if (cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
		if (depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
		glPolygonMode(GL_FRONT_AND_BACK, polygon_mode);

		proj_matrix = perspective(
			(float)windowInfo->vfov, // virtual fov
			(float)windowInfo->width / (float)windowInfo->height, // aspect ratio
			(PLAYER_HEIGHT - CAMERA_HEIGHT) * 1 / sqrtf(2.0f), // see blocks no matter how close they are
			64 * CHUNK_WIDTH // only support 64 chunks for now
		);
		glNamedBufferSubData(glInfo->trans_buf, sizeof(vmath::mat4), sizeof(proj_matrix), proj_matrix); // proj matrix


		// unbind VAO jic
		glBindVertexArray(0);

		// DELETE
		glDeleteBuffers(1, &quad_block_type_buf);
		glDeleteBuffers(1, &quad_corner1_buf);
		glDeleteBuffers(1, &quad_corner2_buf);
	}

	inline void highlight_block(OpenGLInfo* glInfo, GlfwInfo* windowInfo, ivec3 xyz) { return highlight_block(glInfo, windowInfo, xyz[0], xyz[1], xyz[2]); }


	// make sure a block isn't air
	inline bool not_air(ivec3 block_coords, ivec3 face) {
		return get_type(block_coords) != Block::Air;
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
	// stop_check = function that decides if we stop the raycast or not
	inline void raycast(vec4 origin, vec4 direction, int radius, ivec3 *result_coords, ivec3 *result_face, const std::function <bool(ivec3 coords, ivec3 face)>& stop_check) {
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

			// success, set result values and return
			if (stop_check({ x, y, z }, face)) {
				*result_coords = { x, y, z };
				*result_face = face;
				return;
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
		// nothing found, set invalid results and return
		*result_coords = { 0, -1, 0 }; // invalid
		*result_face = { 0, 0, 0 }; // invalid
		return;
	}

	// when a mini updates, update its and its neighbors' meshes, if required.
	// mini: the mini that changed
	// block: the mini-coordinates of the block that was added/deleted
	// TODO: Use block.
	void on_mini_update(MiniChunk* mini, vmath::ivec3 block) {
		auto &coords = mini->coords;

		// need to regenerate self and neighbors
		vector<MiniChunk*> minis_to_regenerate = { mini };

		// get all neighbors
		auto neighbor_coords = mini->neighbors();
		for (auto coord : neighbor_coords) {
			MiniChunk* m = get_mini(coord);
			if (m != nullptr) {
				minis_to_regenerate.push_back(m);
			}
		}

		// update self and neighbors
		for (auto mini : minis_to_regenerate) {
			mini->invisible = mini->all_air() || check_if_covered(*mini);
			if (!mini->invisible) {
				//MiniChunkMesh* mesh = gen_minichunk_mesh(mini);
				MiniChunkMesh* mesh = gen_minichunk_mesh_gpu(glInfo, mini);

				MiniChunkMesh* non_water = new MiniChunkMesh;
				MiniChunkMesh* water = new MiniChunkMesh;

				for (auto &quad : mesh->quads3d) {
					if ((Block)quad.block == Block::Water) {
						water->quads3d.push_back(quad);
					}
					else {
						non_water->quads3d.push_back(quad);
					}
				}

				assert(mesh->size() == non_water->size() + water->size());

				mini->mesh = non_water;
				mini->water_mesh = water;

				mini->update_quads_buf();
			}
		}
	}

	void destroy_block(int x, int y, int z) {
		// update data
		MiniChunk* mini = get_mini_containing_block(x, y, z);
		ivec3 mini_coords = get_mini_relative_coords(x, y, z);
		mini->set_block(mini_coords, Block::Air);

		// regenerate textures for all neighboring minis (TODO: This should be a maximum of 3 neighbors, since the block always has at least 3 sides inside its mini.)
		on_mini_update(mini, { x, y, z });
	}

	void destroy_block(ivec3 xyz) { return destroy_block(xyz[0], xyz[1], xyz[2]); };

	void add_block(int x, int y, int z, Block block) {
		// update data
		MiniChunk* mini = get_mini_containing_block(x, y, z);
		ivec3 mini_coords = get_mini_relative_coords(x, y, z);
		mini->set_block(mini_coords, block);

		// regenerate textures for all neighboring minis (TODO: This should be a maximum of 3 neighbors, since the block always has at least 3 sides inside its mini.)
		on_mini_update(mini, { x, y, z });
	}

	void add_block(ivec3 xyz, Block block) { return add_block(xyz[0], xyz[1], xyz[2], block); };

	// extract layer from output
	// TODO: memcpy?
	static void extract_layer(unsigned *output, unsigned layer_idx, unsigned global_face_idx, unsigned global_minichunk_idx, Block(&results)[16][16]) {
		for (int v = 0; v < 16; v++) {
			for (int u = 0; u < 16; u++) {
				results[u][v] = (uint8_t)output[u + v * 16 + layer_idx * 16 * 16 + global_face_idx * 16 * 16 * 16 + global_minichunk_idx * 6 * 16 * 16 * 16];
			}
		}
	}

	// fill layer in output
	// TODO: memcpy?
	static void fill_layer(unsigned *output, unsigned layer_idx, unsigned global_face_idx, unsigned global_minichunk_idx, Block(&layer)[16][16]) {
		for (int v = 0; v < 16; v++) {
			for (int u = 0; u < 16; u++) {
				output[u + v * 16 + layer_idx * 16 * 16 + global_face_idx * 16 * 16 * 16 + global_minichunk_idx * 6 * 16 * 16 * 16] = (uint8_t)layer[u][v];
			}
		}
	}

	// fill in missed layers for a minichunk's layers
	void fill_missed_layers(unsigned *output, MiniChunk* mini, unsigned global_minichunk_idx) {
		Block layer[16][16];

		// for each face
		for (int global_face_idx = 0; global_face_idx < 6; global_face_idx++) {
			int local_face_idx = global_face_idx % 3;
			bool backface = global_face_idx < 3;

			ivec3 face = ivec3(0, 0, 0);
			face[local_face_idx] = backface ? -1 : 1;

			// working indices are always gonna be xy, xz, or yz.
			int working_idx_1 = local_face_idx == 0 ? 1 : 0;
			int working_idx_2 = local_face_idx == 2 ? 1 : 2;

			// index of layer to fill
			// if backface, fill first layer (0), else fill last layer (15)
			int layer_idx = backface ? 0 : 15;

			// fill layer
			gen_layer(mini, local_face_idx, layer_idx, face, layer);
			fill_layer(output, layer_idx, global_face_idx, global_minichunk_idx, layer);
		}

		OutputDebugString("");
	}

	// generate mesh for a single minichunk on GPU
	MiniChunkMesh* gen_minichunk_mesh_gpu(OpenGLInfo *glInfo, MiniChunk* mini) {
		char buf[1024];

		// got our mesh
		MiniChunkMesh* mesh = new MiniChunkMesh();

		// generate layers that we can't generate ourselves (yet)
		unsigned *layers = new unsigned[16 * 16 * 96];
		fill_missed_layers(layers, mini, 0);

		// load them into GPU
		// (it's okay that some are uninitialized, since they'll get initialized by running gen_layers)
		glNamedBufferSubData(glInfo->gen_layer_layers_buf, 0, 16 * 16 * 96 * sizeof(unsigned), layers);
		delete[] layers;

		// switch to gen_layers program
		glUseProgram(glInfo->gen_layer_program);

		// convert mini data to unsigned ints, since that's what GPU wants
		unsigned data[MINICHUNK_SIZE];
		for (int k = 0; k < MINICHUNK_SIZE; k++) {
			data[k] = (uint8_t)mini->data[k];
		}

		// load data into GPU
		glNamedBufferSubData(glInfo->gen_layer_mini_buf, 0, MINICHUNK_SIZE * sizeof(unsigned), data);

		// gen layers!
		glDispatchCompute(
			1, // 1 minichunk
			6, // 6 faces
			15 // 15 layers per face
		);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		// switch to gen_quads program
		glUseProgram(glInfo->gen_quads_program);

		// initialize atomic counter
		GLuint num_quads = 0;
		glNamedBufferSubData(glInfo->gen_quads_atomic_buf, 0, sizeof(GLuint), &num_quads);

		// gen quads!
		glDispatchCompute(
			96, // 96 layers ('cause 1 minichunk)
			1,
			1
		);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);

		// get number of generated quads
		glGetNamedBufferSubData(glInfo->gen_quads_atomic_buf, 0, sizeof(GLuint), &num_quads);
		sprintf(buf, "gen_minichunk_mesh_gpu num generated quads: %u\n", num_quads);
		OutputDebugString(buf);

		// read back quad3ds
		Quad3DCS *quad3ds = new Quad3DCS[num_quads];
		glGetNamedBufferSubData(glInfo->gen_quads_quads3d_buf, 0, num_quads * sizeof(Quad3DCS), quad3ds);

		// fill them into mesh
		for (int i = 0; i < num_quads; i++) {
			int local_face_idx = quad3ds[i].global_face_idx % 3;
			bool backface = quad3ds[i].global_face_idx < 3;
			ivec3 face = { 0, 0, 0 };
			face[local_face_idx] = backface ? -1 : 1;

			Quad3D q;
			q.block = quad3ds[i].block;
			for (int j = 0; j < 3; j++) {
				q.corners[0][j] = quad3ds[i].coords[0][j];
				q.corners[1][j] = quad3ds[i].coords[1][j];
			}
			q.face = face;

			mesh->quads3d.push_back(q);
		}

		return mesh;
	}

	// generate meshes for multiple minichunks on GPU
	// TODO: assert that there's no duplicates
	// TODO: assert no more than we can handle (256 minis?)
	void gen_minichunk_meshes_gpu(OpenGLInfo *glInfo, vector<MiniChunk*> &minis, vector<MiniChunkMesh*> &results) {
		if (minis.size() == 0) return;

		char buf[1024];

		// all the resulting layers
		unsigned *layers = new unsigned[16 * 16 * 96 * minis.size()];
		unsigned *data = new unsigned[MINICHUNK_SIZE * minis.size()];

		auto start_init_local_data = std::chrono::high_resolution_clock::now();

		// for each mini
		for (int i = 0; i < minis.size(); i++) {
			auto &mini = minis[i];

			// generate layers that we can't generate ourselves (yet)
			fill_missed_layers(layers, mini, i); // I hope this works!

			// convert mini data to unsigned ints, since that's what GPU wants
			for (int k = 0; k < MINICHUNK_SIZE; k++) {
				data[i*MINICHUNK_SIZE + k] = (uint8_t)mini->data[k];
			}
		}

		auto end_init_local_data = std::chrono::high_resolution_clock::now();
		long result_init_local_data = std::chrono::duration_cast<std::chrono::microseconds>(end_init_local_data - start_init_local_data).count();

		auto start_load_data_gpu = std::chrono::high_resolution_clock::now();

		// load layers into GPU
		// (it's okay that some are uninitialized, since they'll get initialized by running gen_layers)
		glNamedBufferSubData(glInfo->gen_layer_layers_buf, 0, 16 * 16 * 96 * sizeof(unsigned) * minis.size(), layers);
		delete[] layers;

		// load mini data into GPU
		glNamedBufferSubData(glInfo->gen_layer_mini_buf, 0, MINICHUNK_SIZE * sizeof(unsigned) * minis.size(), data);
		delete[] data;

		auto end_load_data_gpu = std::chrono::high_resolution_clock::now();
		long result_load_data_gpu = std::chrono::duration_cast<std::chrono::microseconds>(end_load_data_gpu - start_load_data_gpu).count();

		auto start_gen_layers_quads = std::chrono::high_resolution_clock::now();

		// switch to gen_layers program
		glUseProgram(glInfo->gen_layer_program);

		// gen layers!
		glDispatchCompute(
			minis.size(), // minis.size() minis
			6, // 6 faces
			15 // 15 layers per face
		);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		// switch to gen_quads program
		glUseProgram(glInfo->gen_quads_program);

		// initialize atomic counter
		GLuint num_quads = 0;
		glNamedBufferSubData(glInfo->gen_quads_atomic_buf, 0, sizeof(GLuint), &num_quads);

		// gen quads!
		glDispatchCompute(
			96 * minis.size(), // 96 layers per mini
			1,
			1
		);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);

		auto end_gen_layers_quads = std::chrono::high_resolution_clock::now();
		long result_gen_layers_quads = std::chrono::duration_cast<std::chrono::microseconds>(end_gen_layers_quads - start_gen_layers_quads).count();

		// WAIT FOR OPEARTIONS TO COMPLETE (up to 1 second)
		GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		GLenum result = glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 1 * 1000000000);
		glDeleteSync(sync);

		// MAKE SURE WAITED SUCCESSFULLY
		switch (result) {
		case GL_TIMEOUT_EXPIRED:
			OutputDebugString("~ TIMEOUT EXPIRED WHILE WAITING\n");
			break;
		case GL_ALREADY_SIGNALED:
		case GL_CONDITION_SATISFIED:
			OutputDebugString("~ SUCCESSFUL WAITING\n");
			break;
		case GL_WAIT_FAILED:
			OutputDebugString("~ WAIT ERROR!?\n");
			break;
		default:
			throw "Huh?";
			break;
		}

		auto start_read_num_quads = std::chrono::high_resolution_clock::now();

		//glCopyNamedBufferSubData(glInfo->gen_quads_atomic_buf, glInfo->gen_quads_atomic_buf_tmp, 0, 0, sizeof(GLuint));
		//glGetNamedBufferSubData(glInfo->gen_quads_atomic_buf_tmp, 0, sizeof(GLuint), &num_quads);

		// get number of generated quads
		glGetNamedBufferSubData(glInfo->gen_quads_atomic_buf, 0, sizeof(GLuint), &num_quads);

		auto end_read_num_quads = std::chrono::high_resolution_clock::now();
		long result_read_num_quads = std::chrono::duration_cast<std::chrono::microseconds>(end_read_num_quads - start_read_num_quads).count();

		//sprintf(buf, "gen_minichunk_mesh_gpu num generated quads: %u\n", num_quads);
		//OutputDebugString(buf);

		auto start_read_quads = std::chrono::high_resolution_clock::now();

		// read back quad3ds
		Quad3DCS *quad3ds = new Quad3DCS[num_quads];
		glGetNamedBufferSubData(glInfo->gen_quads_quads3d_buf, 0, num_quads * sizeof(Quad3DCS), quad3ds);

		auto end_read_quads = std::chrono::high_resolution_clock::now();
		long result_read_quads = std::chrono::duration_cast<std::chrono::microseconds>(end_read_quads - start_read_quads).count();

		auto start_fill_mesh = std::chrono::high_resolution_clock::now();

		// fill them into mesh
		// TODO: do this efficiently, not like fuckin this.
		for (int i = 0; i < num_quads; i++) {
			int local_face_idx = quad3ds[i].global_face_idx % 3;
			bool backface = quad3ds[i].global_face_idx < 3;
			ivec3 face = { 0, 0, 0 };
			face[local_face_idx] = backface ? -1 : 1;

			Quad3D q;
			q.block = quad3ds[i].block;
			for (int j = 0; j < 3; j++) {
				q.corners[0][j] = quad3ds[i].coords[0][j];
				q.corners[1][j] = quad3ds[i].coords[1][j];
			}
			q.face = face;

			results[quad3ds[i].mini_input_idx]->quads3d.push_back(q);
		}

		delete[] quad3ds;

		auto end_fill_mesh = std::chrono::high_resolution_clock::now();
		long result_fill_mesh = std::chrono::duration_cast<std::chrono::microseconds>(end_fill_mesh - start_fill_mesh).count();

		// done
		sprintf(buf, "num quads generated: %d\n", num_quads);
		OutputDebugString(buf);
		sprintf(buf, "\ninit local data: %.2f ms\nload data gpu: %.2f ms\ngen layers, quads: %.2f ms\nread num quads: %.2f\nread quads: %.2f ms\nfill mesh: %.2f ms\n\n", result_init_local_data / 1000.0f, result_load_data_gpu / 1000.0f, result_gen_layers_quads / 1000.0f, result_read_num_quads / 1000.0f, result_read_quads / 1000.0f, result_fill_mesh / 1000.0f);
		OutputDebugString(buf);
		OutputDebugString("");
	}

	// generate meshes for multiple minichunks on GPU
	// TODO: assert that there's no duplicates
	// TODO: assert no more than we can handle (256 minis?)
	void gen_minichunk_meshes_gpu_fast(OpenGLInfo *glInfo, vector<MiniChunk*> &minis, vector<MiniChunkMesh*> &results) {
		if (minis.size() == 0) return;

		auto start_of_fn = std::chrono::high_resolution_clock::now();

		char buf[1024];

		// create a mini with all 0 data to use for invisible minis, -y minis, non-loaded-in minis, etc.
		MiniChunk zero_mini;
		zero_mini.data = new Block[MINICHUNK_SIZE];
		memset(zero_mini.data, 0, sizeof(Block) * MINICHUNK_SIZE);

		// find all the neighboring minis (EXCLUDING existing minis), and add them to our chunks vector
		unordered_set<ivec3, vecN_hash> neighboring_mini_coords_set;
		for (auto &mini : minis) {
			// add all neighbors, even invalid ones (with y == -16, y == 256)
			for (auto &neighbor : mini->all_neighbors()) {
				neighboring_mini_coords_set.insert(neighbor);
			}
		}

		// remove all the ones that are already in minis
		for (auto &mini : minis) {
			neighboring_mini_coords_set.erase(mini->coords);
		}

		// convert to vec
		// TODO: just combine both using hashtable
		vector<ivec3> neighboring_mini_coords;
		for (auto mini_coord : neighboring_mini_coords_set) {
			neighboring_mini_coords.push_back(mini_coord);
		}

		// convert coords into actual minis
		vector<MiniChunk*> neighboring_minis;
		for (auto mini_coord : neighboring_mini_coords) {
			MiniChunk* mini = nullptr;

			// if invalid coords, zero mini
			if (mini_coord[1] < 0 || mini_coord[1] > (CHUNK_HEIGHT - MINICHUNK_HEIGHT)) {
				mini = &zero_mini;
			}
			// if valid coords,
			else {
				// get mini at coords
				MiniChunk* mini2 = get_mini(mini_coord);

				// if chunk not loaded in, use zero mini
				if (mini2 == nullptr) {
					mini = &zero_mini;
				}
				// if invisible, use zero-mini
				else if (mini2->invisible) {
					mini = &zero_mini;
				}
				// chunk loaded in and visible, use that mini
				else {
					mini = mini2;
				}
			}

			// add it to our list
			neighboring_minis.push_back(mini);
		}

		// total num minis, to add to uniform later
		int total_num_minis = minis.size() + neighboring_minis.size();

		// DEBUG
		sprintf(buf, "\ngen meshes for %d minis (%d using total mini datas)\n", minis.size(), total_num_minis);
		OutputDebugString(buf);

		// prepare mini data in same format as GPU takes
		// TODO: maybe don't alloc every time, or instead, just MAP TO GPU and generate right there and then.

		auto start_init_local_data = std::chrono::high_resolution_clock::now();

		unsigned *data = new unsigned[MINICHUNK_SIZE * total_num_minis];
		ivec4 *coords = new ivec4[total_num_minis];

		// prepare all data for minis that we're meshing for
		// TODO: Use std::copy
		for (int i = 0; i < minis.size(); i++) {
			auto &mini = *minis[i];
			for (int k = 0; k < MINICHUNK_SIZE; k++) {
				data[i*MINICHUNK_SIZE + k] = (uint8_t)mini.data[k];
			}
			coords[i] = ivec4(mini.coords[0], mini.coords[1], mini.coords[2], 0);
		}

		// prepare all data for neighboring minis
		// TODO: Use std::copy
		for (int i = 0; i < neighboring_minis.size(); i++) {
			int total_mini_idx = minis.size() + i;
			auto &mini = *neighboring_minis[i];
			auto &mini_coords = neighboring_mini_coords[i];
			for (int k = 0; k < MINICHUNK_SIZE; k++) {
				data[total_mini_idx*MINICHUNK_SIZE + k] = (uint8_t)mini.data[k];
			}
			coords[total_mini_idx] = ivec4(mini_coords[0], mini_coords[1], mini_coords[2], 0);
		}

		auto end_init_local_data = std::chrono::high_resolution_clock::now();
		long result_init_local_data = std::chrono::duration_cast<std::chrono::microseconds>(end_init_local_data - start_init_local_data).count();

		auto start_load_data_gpu = std::chrono::high_resolution_clock::now();

		// load mini data into GPU
		glNamedBufferSubData(glInfo->gen_layer_mini_buf, 0, MINICHUNK_SIZE * sizeof(unsigned) * total_num_minis, data);
		delete[] data;

		// load coords into GPU
		glNamedBufferSubData(glInfo->gen_layer_mini_coords_buf, 0, sizeof(ivec4) * total_num_minis, coords);

		auto end_load_data_gpu = std::chrono::high_resolution_clock::now();
		long result_load_data_gpu = std::chrono::duration_cast<std::chrono::microseconds>(end_load_data_gpu - start_load_data_gpu).count();

		auto start_gen_layers_quads = std::chrono::high_resolution_clock::now();

		// switch to gen_layers_and_quads program
		glUseProgram(glInfo->gen_layers_and_quads_program);

		// initialize num minis
		GLuint num_minis = total_num_minis;
		glUniform1ui(0, num_minis);

		// initialize atomic counter
		GLuint num_quads = 0;
		glNamedBufferSubData(glInfo->gen_quads_atomic_buf, 0, sizeof(GLuint), &num_quads);

		// gen layers!
		// TODO: 1 workgroup per mini?
		glDispatchCompute(
			96 * minis.size(), // 96 layers per mini
			1,
			1
		);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);

		auto end_gen_layers_quads = std::chrono::high_resolution_clock::now();
		long result_gen_layers_quads = std::chrono::duration_cast<std::chrono::microseconds>(end_gen_layers_quads - start_gen_layers_quads).count();

		auto start_wait_sync = std::chrono::high_resolution_clock::now();

		// WAIT FOR OPEARTIONS TO COMPLETE (up to 1 second)
		GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		GLenum result = glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 1 * 1000000000);
		glDeleteSync(sync);

		// MAKE SURE WAITED SUCCESSFULLY
		switch (result) {
		case GL_TIMEOUT_EXPIRED:
			OutputDebugString("~ TIMEOUT EXPIRED WHILE WAITING\n");
			break;
		case GL_ALREADY_SIGNALED:
		case GL_CONDITION_SATISFIED:
			OutputDebugString("~ SUCCESSFUL WAITING\n");
			break;
		case GL_WAIT_FAILED:
			OutputDebugString("~ WAIT ERROR!?\n");
			break;
		default:
			throw "Huh?";
			break;
		}

		auto end_wait_sync = std::chrono::high_resolution_clock::now();
		long result_wait_sync = std::chrono::duration_cast<std::chrono::microseconds>(end_wait_sync - start_wait_sync).count();

		auto start_read_num_quads = std::chrono::high_resolution_clock::now();

		// get number of generated quads
		glGetNamedBufferSubData(glInfo->gen_quads_atomic_buf, 0, sizeof(GLuint), &num_quads);

		auto end_read_num_quads = std::chrono::high_resolution_clock::now();
		long result_read_num_quads = std::chrono::duration_cast<std::chrono::microseconds>(end_read_num_quads - start_read_num_quads).count();

		auto start_read_quads = std::chrono::high_resolution_clock::now();

		// read back quad3ds
		Quad3DCS *quad3ds = new Quad3DCS[num_quads];
		glGetNamedBufferSubData(glInfo->gen_quads_quads3d_buf, 0, num_quads * sizeof(Quad3DCS), quad3ds);

		auto end_read_quads = std::chrono::high_resolution_clock::now();
		long result_read_quads = std::chrono::duration_cast<std::chrono::microseconds>(end_read_quads - start_read_quads).count();

		if (quad3ds[0].empty != 12345) {
			// if error == (cannot find face coords in list)
			if (quad3ds[0].empty == 1) {
				Quad3DCS error_q = quad3ds[0];
				ivec4 own_coords = error_q.coords[0];
				ivec4 face_coords = error_q.coords[1];

				int own_idx = -1;
				int face_idx = -1;

				for (int i = 0; i < total_num_minis; i++) {
					if (coords[i] == own_coords) {
						if (own_idx != -1) {
							throw "duplicate";
						}
						own_idx = i;
					}
					if (coords[i] == face_coords) {
						if (face_idx != -1) {
							throw "duplicate";
						}
						face_idx = i;
					}
				}
			}


			// a-ha! Missing coords!

			throw "error";
		}
		delete[] coords;

		auto start_fill_mesh = std::chrono::high_resolution_clock::now();

		// fill them into mesh
		// TODO: do this efficiently, not like fuckin this.
		for (int i = 0; i < num_quads; i++) {
			int local_face_idx = quad3ds[i].global_face_idx % 3;
			bool backface = quad3ds[i].global_face_idx < 3;
			ivec3 face = { 0, 0, 0 };
			face[local_face_idx] = backface ? -1 : 1;

			Quad3D q;
			q.block = quad3ds[i].block;
			for (int j = 0; j < 3; j++) {
				q.corners[0][j] = quad3ds[i].coords[0][j];
				q.corners[1][j] = quad3ds[i].coords[1][j];
			}
			q.face = face;

			results[quad3ds[i].mini_input_idx]->quads3d.push_back(q);
		}

		zero_mini.free_data();
		delete[] quad3ds;

		auto end_fill_mesh = std::chrono::high_resolution_clock::now();
		long result_fill_mesh = std::chrono::duration_cast<std::chrono::microseconds>(end_fill_mesh - start_fill_mesh).count();

		// done
		auto end_of_fn = std::chrono::high_resolution_clock::now();
		long result_total = std::chrono::duration_cast<std::chrono::microseconds>(end_of_fn - start_of_fn).count();

		sprintf(buf, "num quads generated: %d\n", num_quads);
		OutputDebugString(buf);
		sprintf(buf, "\ninit local data: %.2f ms\nload data gpu: %.2f ms\ngen layers, quads: %.2f ms\nwait sync: %.2f\nread num quads: %.2f\nread quads: %.2f ms\nfill mesh: %.2f ms\nTOTAL: %.2fms\n\n", result_init_local_data / 1000.0f, result_load_data_gpu / 1000.0f, result_gen_layers_quads / 1000.0f, result_wait_sync / 1000.0f, result_read_num_quads / 1000.0f, result_read_quads / 1000.0f, result_fill_mesh / 1000.0f, result_total / 1000.0f);
		OutputDebugString(buf);
		OutputDebugString("");
	}

	// generate meshes for multiple minichunks on GPU
	// TODO: assert that there's no duplicates
	// TODO: assert no more than we can handle (256 minis?)
	void update_enqueued_chunks_meshes(OpenGLInfo *glInfo) {
		// TODO: define.
		//int chunks_at_a_time = 16;
		int minis_at_a_time = 64 * 16;

		// if not waiting on any chunks
		if (chunk_sync == NULL) {
			// if no minis to enqueue, return
			if (chunk_gen_mini_queue.size() == 0) {
				return;
			}

			// we have chunks to start generating
			// ah fuck it... do it mini-style
			auto &minis = chunk_gen_minis;
			minis.clear();

			// up to a certain mini limit
			for (int i = 0; i < minis_at_a_time && chunk_gen_mini_queue.size() > 0; i++) {
				// move mini from queue to current processing minis
				MiniChunk* mini = chunk_gen_mini_queue.front();
				chunk_gen_mini_queue.pop();

				//char buf[256];
				//sprintf(buf, "Popping mini (%d, %d, %d) to queue...\n", mini->coords[0], mini->coords[1], mini->coords[2]);
				//OutputDebugString(buf);

				minis.push_back(mini);
			}

			// LOAD IN LAYERS
			auto start_initload_layers = std::chrono::high_resolution_clock::now();

			unsigned *layers = new unsigned[16 * 16 * 96 * minis.size()];

			// for each mini
			for (int i = 0; i < minis.size(); i++) {
				auto &mini = minis[i];

				// generate layers that we can't generate ourselves (yet)
				fill_missed_layers(layers, mini, i);
			}

			// load layers into GPU
			// (it's okay that some are uninitialized, since they'll get initialized by running gen_layers)
			glNamedBufferSubData(glInfo->gen_layer_layers_buf, 0, 16 * 16 * 96 * sizeof(unsigned) * minis.size(), layers);
			delete[] layers;

			auto end_initload_layers = std::chrono::high_resolution_clock::now();
			long result_initload_layers = std::chrono::duration_cast<std::chrono::microseconds>(end_initload_layers - start_initload_layers).count();


			auto start_initload_minidata = std::chrono::high_resolution_clock::now();

			// LOAD IN MINIS
			unsigned *data = new unsigned[MINICHUNK_SIZE * minis.size()];

			// for each mini
			for (int i = 0; i < minis.size(); i++) {
				auto &mini = minis[i];

				// convert mini data to unsigned ints, since that's what GPU wants
				for (int k = 0; k < MINICHUNK_SIZE; k++) {
					data[i*MINICHUNK_SIZE + k] = (uint8_t)mini->data[k];
				}
			}



			// load mini data into GPU
			glNamedBufferSubData(glInfo->gen_layer_mini_buf, 0, MINICHUNK_SIZE * sizeof(unsigned) * minis.size(), data);
			delete[] data;

			auto end_initload_minidata = std::chrono::high_resolution_clock::now();
			long result_initload_minidata = std::chrono::duration_cast<std::chrono::microseconds>(end_initload_minidata - start_initload_minidata).count();

			// initialize atomic counter
			GLuint num_quads = 0;
			glNamedBufferSubData(glInfo->gen_quads_atomic_buf, 0, sizeof(GLuint), &num_quads);

			auto start_gen_layers_quads = std::chrono::high_resolution_clock::now();

			// switch to gen_layers program
			glUseProgram(glInfo->gen_layer_program);

			// gen layers!
			glDispatchCompute(
				minis.size(), // minis.size() minis
				6, // 6 faces
				15 // 15 layers per face
			);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

			// switch to gen_quads program
			glUseProgram(glInfo->gen_quads_program);

			// gen quads!
			glDispatchCompute(
				96 * minis.size(), // 96 layers per mini
				1,
				1
			);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);

			auto end_gen_layers_quads = std::chrono::high_resolution_clock::now();
			long result_gen_layers_quads = std::chrono::duration_cast<std::chrono::microseconds>(end_gen_layers_quads - start_gen_layers_quads).count();

			// at this point, just need to wait for quads to complete
			// WAIT FOR OPEARTIONS TO COMPLETE (up to 1 second)
			chunk_sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

			OutputDebugString("START QUAD GENERATION RESULT:\n");
			char buf[1024];
			sprintf(buf, "\ninit/load layers: %.2f ms\ninit/load minis: %.2f ms\ngen layers, quads: %.2f ms\n\n", result_initload_layers / 1000.0f, result_initload_minidata / 1000.0f, result_gen_layers_quads / 1000.0f);
			OutputDebugString(buf);
			OutputDebugString("");

		}

		// if ARE waiting on chunks
		else {
			// check if it's done
			GLint result;
			glGetSynciv(chunk_sync, GL_SYNC_STATUS, sizeof(GLint), NULL, &result);

			// if it's not done, quit
			if (result != GL_SIGNALED) {
				return;
			}

			// done! Let's read back num_quads and get that shit on!
			glDeleteSync(chunk_sync);
			chunk_sync = 0;

			auto start_read_num_quads = std::chrono::high_resolution_clock::now();

			// get number of generated quads
			GLuint num_quads;
			glGetNamedBufferSubData(glInfo->gen_quads_atomic_buf, 0, sizeof(GLuint), &num_quads);

			auto end_read_num_quads = std::chrono::high_resolution_clock::now();
			long result_read_num_quads = std::chrono::duration_cast<std::chrono::microseconds>(end_read_num_quads - start_read_num_quads).count();

			//sprintf(buf, "gen_minichunk_mesh_gpu num generated quads: %u\n", num_quads);
			//OutputDebugString(buf);

			auto start_read_quads = std::chrono::high_resolution_clock::now();

			// read back quad3ds
			Quad3DCS *quad3ds = new Quad3DCS[num_quads];
			glGetNamedBufferSubData(glInfo->gen_quads_quads3d_buf, 0, num_quads * sizeof(Quad3DCS), quad3ds);

			auto end_read_quads = std::chrono::high_resolution_clock::now();
			long result_read_quads = std::chrono::duration_cast<std::chrono::microseconds>(end_read_quads - start_read_quads).count();

			auto start_fill_mesh = std::chrono::high_resolution_clock::now();

			vector<MiniChunkMesh*> results;
			// TODO: free?
			for (int i = 0; i < chunk_gen_minis.size(); i++) {
				results.push_back(new MiniChunkMesh);
			}

			// fill them into mesh
			// TODO: do this efficiently, not like fuckin this.
			for (int i = 0; i < num_quads; i++) {
				int local_face_idx = quad3ds[i].global_face_idx % 3;
				bool backface = quad3ds[i].global_face_idx < 3;
				ivec3 face = { 0, 0, 0 };
				face[local_face_idx] = backface ? -1 : 1;

				Quad3D q;
				q.block = quad3ds[i].block;
				for (int j = 0; j < 3; j++) {
					q.corners[0][j] = quad3ds[i].coords[0][j];
					q.corners[1][j] = quad3ds[i].coords[1][j];
				}
				q.face = face;

				results[quad3ds[i].mini_input_idx]->quads3d.push_back(q);
			}

			delete[] quad3ds;

			auto end_fill_mesh = std::chrono::high_resolution_clock::now();
			long result_fill_mesh = std::chrono::duration_cast<std::chrono::microseconds>(end_fill_mesh - start_fill_mesh).count();

			// done
			for (int i = 0; i < chunk_gen_minis.size(); i++) {
				auto &mini = chunk_gen_minis[i];
				auto &mesh = results[i];

				MiniChunkMesh* non_water = new MiniChunkMesh;
				MiniChunkMesh* water = new MiniChunkMesh;

				for (auto &quad : mesh->quads3d) {
					if ((Block)quad.block == Block::Water) {
						water->quads3d.push_back(quad);
					}
					else {
						non_water->quads3d.push_back(quad);
					}
				}

				mini->mesh = non_water;
				mini->water_mesh = water;
				mini->update_quads_buf();
			}

			OutputDebugString("READ QUADS RESULT:\n");
			char buf[1024];
			//sprintf(buf, "\ninit local data: %.2f ms\nload data gpu: %.2f ms\ngen layers, quads: %.2f ms\nread num quads: %.2f\nread quads: %.2f ms\nfill mesh: %.2f ms\n\n", result_init_local_data / 1000.0f, result_load_data_gpu / 1000.0f, result_gen_layers_quads / 1000.0f, result_read_num_quads / 1000.0f, result_read_quads / 1000.0f, result_fill_mesh / 1000.0f);
			sprintf(buf, "\nread num quads: %.2f\nread quads: %.2f ms\nfill mesh: %.2f ms\n\n", result_read_num_quads / 1000.0f, result_read_quads / 1000.0f, result_fill_mesh / 1000.0f);
			OutputDebugString(buf);
			OutputDebugString("");
		}
	}

	void update_enqueued_chunks_meshes_fast(OpenGLInfo *glInfo) {
		// TODO: define.
		//int chunks_at_a_time = 16;
		int minis_at_a_time = 64 * 16;
		char buf[1024];

		auto start_of_fn = std::chrono::high_resolution_clock::now();

		// if not waiting on any chunks
		if (chunk_sync == NULL) {
			// if no minis to enqueue, return
			if (chunk_gen_mini_queue.size() == 0) {
				return;
			}

			OutputDebugString("1 start...\n");

			auto start_init_local_data = std::chrono::high_resolution_clock::now();

			// we have chunks to start generating
			// ah fuck it... do it mini-style
			auto &minis = chunk_gen_minis;
			minis.clear();

			// up to a certain mini limit
			for (int i = 0; i < minis_at_a_time && chunk_gen_mini_queue.size() > 0; i++) {
				// move mini from queue to current processing minis
				MiniChunk* mini = chunk_gen_mini_queue.front();
				chunk_gen_mini_queue.pop();

				//char buf[256];
				//sprintf(buf, "Popping mini (%d, %d, %d) to queue...\n", mini->coords[0], mini->coords[1], mini->coords[2]);
				//OutputDebugString(buf);

				minis.push_back(mini);
			}


			// create a mini with all 0 data to use for invisible minis, -y minis, non-loaded-in minis, etc.
			MiniChunk zero_mini;
			zero_mini.data = new Block[MINICHUNK_SIZE];
			memset(zero_mini.data, 0, sizeof(Block) * MINICHUNK_SIZE);

			// find all the neighboring minis (EXCLUDING existing minis), and add them to our chunks vector
			unordered_set<ivec3, vecN_hash> neighboring_mini_coords_set;
			for (auto &mini : minis) {
				// add all neighbors, even invalid ones (with y == -16, y == 256)
				for (auto &neighbor : mini->all_neighbors()) {
					neighboring_mini_coords_set.insert(neighbor);
				}
			}

			// remove all the ones that are already in minis
			for (auto &mini : minis) {
				neighboring_mini_coords_set.erase(mini->coords);
			}

			// convert to vec
			// TODO: just combine both using hashtable
			vector<ivec3> neighboring_mini_coords;
			for (auto mini_coord : neighboring_mini_coords_set) {
				neighboring_mini_coords.push_back(mini_coord);
			}

			// convert coords into actual minis
			vector<MiniChunk*> neighboring_minis;
			for (auto mini_coord : neighboring_mini_coords) {
				MiniChunk* mini = nullptr;

				// if invalid coords, zero mini
				if (mini_coord[1] < 0 || mini_coord[1] > (CHUNK_HEIGHT - MINICHUNK_HEIGHT)) {
					mini = &zero_mini;
				}
				// if valid coords,
				else {
					// get mini at coords
					MiniChunk* mini2 = get_mini(mini_coord);

					// if chunk not loaded in, use zero mini
					if (mini2 == nullptr) {
						mini = &zero_mini;
					}
					// if invisible, use zero-mini
					else if (mini2->invisible) {
						mini = &zero_mini;
					}
					// chunk loaded in and visible, use that mini
					else {
						mini = mini2;
					}
				}

				// add it to our list
				neighboring_minis.push_back(mini);
			}

			auto end_init_local_data = std::chrono::high_resolution_clock::now();
			long result_init_local_data = std::chrono::duration_cast<std::chrono::microseconds>(end_init_local_data - start_init_local_data).count();


			// total num minis, to add to uniform later
			int total_num_minis = minis.size() + neighboring_minis.size();


			// DEBUG
			sprintf(buf, "\ngen meshes for %d minis (%d using total mini datas)\n", minis.size(), total_num_minis);
			OutputDebugString(buf);

			// prepare mini data in same format as GPU takes
			// TODO: maybe don't alloc every time, or instead, just MAP TO GPU and generate right there and then.

			auto start_load_data_gpu = std::chrono::high_resolution_clock::now();

			// map buffers
			// TODO: Use map to create my minichunk meshes! And shit.
			// TODO: Check out other bits.
			// TODO: Try invalidate range bit.
			unsigned* data = (unsigned*)glMapNamedBufferRange(glInfo->gen_layer_mini_buf, 0, MINICHUNK_SIZE * sizeof(unsigned) * total_num_minis, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
			ivec4* coords = (ivec4*)glMapNamedBufferRange(glInfo->gen_layer_mini_coords_buf, 0, sizeof(ivec4) * total_num_minis, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);

			// write all data for minis that we're meshing for
			for (int i = 0; i < minis.size(); i++) {
				auto &mini = *minis[i];
				std::copy(mini.data, mini.data + MINICHUNK_SIZE, data + i * MINICHUNK_SIZE);
				coords[i] = ivec4(mini.coords[0], mini.coords[1], mini.coords[2], 0);
			}

			// write all data for neighboring minis
			for (int i = 0; i < neighboring_minis.size(); i++) {
				int total_mini_idx = minis.size() + i;
				auto &mini = *neighboring_minis[i];
				auto &mini_coords = neighboring_mini_coords[i];
				std::copy(mini.data, mini.data + MINICHUNK_SIZE, data + total_mini_idx * MINICHUNK_SIZE);
				coords[total_mini_idx] = ivec4(mini_coords[0], mini_coords[1], mini_coords[2], 0);
			}

			// flush
			glFlushMappedNamedBufferRange(glInfo->gen_layer_mini_buf, 0, MINICHUNK_SIZE * sizeof(unsigned) * total_num_minis);
			glFlushMappedNamedBufferRange(glInfo->gen_layer_mini_coords_buf, 0, sizeof(ivec4) * total_num_minis);

			// unmap buffers
			glUnmapNamedBuffer(glInfo->gen_layer_mini_buf);
			glUnmapNamedBuffer(glInfo->gen_layer_mini_coords_buf);

			zero_mini.free_data();

			auto end_load_data_gpu = std::chrono::high_resolution_clock::now();
			long result_load_data_gpu = std::chrono::duration_cast<std::chrono::microseconds>(end_load_data_gpu - start_load_data_gpu).count();

			auto start_gen_layers_quads = std::chrono::high_resolution_clock::now();

			// switch to gen_layers_and_quads program
			glUseProgram(glInfo->gen_layers_and_quads_program);

			// initialize num minis
			GLuint num_minis = total_num_minis;
			glUniform1ui(0, num_minis);

			// initialize atomic counter
			GLuint num_quads = 0;
			glNamedBufferSubData(glInfo->gen_quads_atomic_buf, 0, sizeof(GLuint), &num_quads);

			// gen layers!
			// TODO: 1 workgroup per mini?
			glDispatchCompute(
				96 * minis.size(), // 96 layers per mini
				1,
				1
			);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);

			auto end_gen_layers_quads = std::chrono::high_resolution_clock::now();
			long result_gen_layers_quads = std::chrono::duration_cast<std::chrono::microseconds>(end_gen_layers_quads - start_gen_layers_quads).count();

			// done
			// done
			auto end_of_fn = std::chrono::high_resolution_clock::now();
			long result_total = std::chrono::duration_cast<std::chrono::microseconds>(end_of_fn - start_of_fn).count();
			sprintf(buf, "num quads generated: %d\n", num_quads);
			OutputDebugString(buf);
			sprintf(buf, "\ninit local data: %.2f ms\nload data gpu: %.2f ms\ngen layers, quads: %.2f ms\nTOTAL (1): %.2fms\n\n", result_init_local_data / 1000.0f, result_load_data_gpu / 1000.0f, result_gen_layers_quads / 1000.0f, result_total / 1000.0f);
			OutputDebugString(buf);
			OutputDebugString("");





			// at this point, just need to wait for quads to complete
			// WAIT FOR OPEARTIONS TO COMPLETE (up to 1 second)
			chunk_sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		}

		// if ARE waiting on chunks
		else {
			// make sure we're not checking too often
			auto now = std::chrono::high_resolution_clock::now();
			auto ms_since_last_check = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_chunk_sync_check).count();

			// no more than 10 times a second
			if (ms_since_last_check < 100) {
				return;
			}
			last_chunk_sync_check = std::chrono::high_resolution_clock::now();

			// check if it's done
			// TODO: timer
			GLint result;
			glGetSynciv(chunk_sync, GL_SYNC_STATUS, sizeof(GLint), NULL, &result);

			// if it's not done, quit
			if (result != GL_SIGNALED) {
				return;
			}

			OutputDebugString("2 start...\n");

			// done! Let's read back num_quads and get that shit on!
			glDeleteSync(chunk_sync);
			chunk_sync = 0;


			auto start_read_num_quads = std::chrono::high_resolution_clock::now();

			// get number of generated quads
			GLuint num_quads;
			glGetNamedBufferSubData(glInfo->gen_quads_atomic_buf, 0, sizeof(GLuint), &num_quads);

			auto end_read_num_quads = std::chrono::high_resolution_clock::now();
			long result_read_num_quads = std::chrono::duration_cast<std::chrono::microseconds>(end_read_num_quads - start_read_num_quads).count();

			auto start_read_quads = std::chrono::high_resolution_clock::now();
			auto end_read_quads = std::chrono::high_resolution_clock::now();
			long result_read_quads = std::chrono::duration_cast<std::chrono::microseconds>(end_read_quads - start_read_quads).count();

			//if (quad3ds[0].empty != 12345) {
			//	// if error == (cannot find face coords in list)
			//	if (quad3ds[0].empty == 1) {
			//		Quad3DCS error_q = quad3ds[0];
			//		ivec4 own_coords = error_q.coords[0];
			//		ivec4 face_coords = error_q.coords[1];

			//		int own_idx = -1;
			//		int face_idx = -1;

			//		for (int i = 0; i < total_num_minis; i++) {
			//			if (coords[i] == own_coords) {
			//				if (own_idx != -1) {
			//					throw "duplicate";
			//				}
			//				own_idx = i;
			//			}
			//			if (coords[i] == face_coords) {
			//				if (face_idx != -1) {
			//					throw "duplicate";
			//				}
			//				face_idx = i;
			//			}
			//		}
			//	}
			//
			//	// a-ha!
			//	throw "error";
			//}

			auto start_fill_mesh = std::chrono::high_resolution_clock::now();

			Quad3DCS *quad3ds = (Quad3DCS*)glMapNamedBufferRange(glInfo->gen_quads_quads3d_buf, 0, num_quads * sizeof(Quad3DCS), GL_MAP_READ_BIT);

			// fill them into mesh
			// TODO: do this efficiently, not like fuckin this.
			vector<MiniChunkMesh*> results;
			for (int i = 0; i < chunk_gen_minis.size(); i++) {
				results.push_back(new MiniChunkMesh);
			}
			for (int i = 0; i < num_quads; i++) {
				int local_face_idx = quad3ds[i].global_face_idx % 3;
				bool backface = quad3ds[i].global_face_idx < 3;
				ivec3 face = { 0, 0, 0 };
				face[local_face_idx] = backface ? -1 : 1;

				Quad3D q;
				q.block = quad3ds[i].block;
				for (int j = 0; j < 3; j++) {
					q.corners[0][j] = quad3ds[i].coords[0][j];
					q.corners[1][j] = quad3ds[i].coords[1][j];
				}
				q.face = face;

				results[quad3ds[i].mini_input_idx]->quads3d.push_back(q);
			}

			glUnmapNamedBuffer(glInfo->gen_quads_quads3d_buf);

			auto end_fill_mesh = std::chrono::high_resolution_clock::now();
			long result_fill_mesh = std::chrono::duration_cast<std::chrono::microseconds>(end_fill_mesh - start_fill_mesh).count();

			// done
			for (int i = 0; i < chunk_gen_minis.size(); i++) {
				auto &mini = chunk_gen_minis[i];
				auto &mesh = results[i];

				MiniChunkMesh* non_water = new MiniChunkMesh;
				MiniChunkMesh* water = new MiniChunkMesh;

				for (auto &quad : mesh->quads3d) {
					if ((Block)quad.block == Block::Water) {
						water->quads3d.push_back(quad);
					}
					else {
						non_water->quads3d.push_back(quad);
					}
				}

				mini->mesh = non_water;
				mini->water_mesh = water;
				mini->update_quads_buf();
			}

			// done
			auto end_of_fn = std::chrono::high_resolution_clock::now();
			long result_total = std::chrono::duration_cast<std::chrono::microseconds>(end_of_fn - start_of_fn).count();
			sprintf(buf, "num quads generated: %d\n", num_quads);
			OutputDebugString(buf);
			sprintf(buf, "\nread num quads: %.2f\nread quads: %.2f ms\nfill mesh: %.2f ms\nTOTAL (2): %.2fms\n\n", result_read_num_quads / 1000.0f, result_read_quads / 1000.0f, result_fill_mesh / 1000.0f, result_total / 1000.0f);
			OutputDebugString(buf);
			OutputDebugString("");
		}

		auto end_of_fn = std::chrono::high_resolution_clock::now();
		long result_total = std::chrono::duration_cast<std::chrono::microseconds>(end_of_fn - start_of_fn).count();

		sprintf(buf, "TOTAL FN TIME: %.2f\n", result_total / 1000.0f);
		OutputDebugString(buf);
	}

	MiniChunkMesh* gen_minichunk_mesh(MiniChunk* mini);
};

#endif /* __WORLD_H__ */
