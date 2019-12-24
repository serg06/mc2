#ifndef __WORLD_H__
#define __WORLD_H__

#include "chunk.h"
#include "chunkdata.h"
#include "minichunkmesh.h"
#include "render.h"
#include "shapes.h"
#include "util.h"

#include "cmake_pch.hxx"

#include <assert.h>
#include <chrono>
#include <functional>
#include <mutex>          // std::mutex
#include <queue>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// radius from center of minichunk that must be included in view frustum
#define FRUSTUM_MINI_RADIUS_ALLOWANCE 28.0f

using namespace std;

class Quad2D {
public:
	BlockType block;
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
}

// represents an in-game world
class World {
public:
	// map of (chunk coordinate) -> chunk
	unordered_map<vmath::ivec2, Chunk*, vecN_hash> chunk_map;

	// get_chunk cache
	Chunk* chunk_cache[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
	vmath::ivec2 chunk_cache_ivec2[5] = { ivec2(INT_MAX), ivec2(INT_MAX), ivec2(INT_MAX), ivec2(INT_MAX), ivec2(INT_MAX) };
	int chunk_cache_clock_hand = 0; // clock cache

	// multi-thread-access minis who need their mesh generated
	queue<MiniChunk*> mesh_gen_queue; // storage
	unordered_set<MiniChunk*> mesh_gen_set; // uniqueness
	std::mutex mesh_gen_mutex; // thread-safety

	// count how many times render() has been called
	int rendered = 0;

	World() {

	}

	// enqueue mesh generation of this mini
	// expects mesh lock
	inline void enqueue_mesh_gen(MiniChunk* mini) {
		assert(mesh_gen_set.size() == mesh_gen_queue.size() && "wew");
		assert(mini != nullptr && "seriously?");

		// check if mini in set
		auto search = mesh_gen_set.find(mini);

		// already in set, so don't add, just quit
		if (search != mesh_gen_set.end()) {
			return;
		}

		// not in set yet, add.
		mesh_gen_set.insert(mini);
		mesh_gen_queue.push(mini);
	}

	inline MiniChunk* dequeue_mesh_gen() {
		assert(mesh_gen_set.size() == mesh_gen_queue.size() && "wew");

		// no meshes to generate!
		if (mesh_gen_set.size() == 0) {
			return nullptr;
		}

		// get mini
		MiniChunk* mini = mesh_gen_queue.front();
		assert(mini != nullptr && "seriously?");

		// remove it from set and queue
		mesh_gen_queue.pop();
		mesh_gen_set.erase(mini);

		// done
		return mini;
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

	// generate all chunks (much faster than gen_chunk)
	inline void gen_chunks(vector<ivec2> to_generate) {
		std::unordered_set<ivec2, vecN_hash> set;
		for (auto coords : to_generate) {
			set.insert(coords);
		}
		return gen_chunks(set);
	}

	// generate all chunks (much faster than gen_chunk)
	inline void gen_chunks(std::unordered_set<ivec2, vecN_hash> to_generate) {
		// get pointers ready
		vector<Chunk*> chunks(to_generate.size());

		// generate chunks and set pointers
		int i = 0;
		for (auto coords : to_generate) {
			// generate it
			Chunk* c = new Chunk(coords);
			c->generate();

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

		// add them all to queue
		mesh_gen_mutex.lock();
		for (auto &mini : minis_to_mesh) {
			enqueue_mesh_gen(mini);
		}
		mesh_gen_mutex.unlock();
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
		vector<ivec2> &coords = gen_circle(distance, chunk_coords);
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
	// inefficient when called repeatedly - if you need multiple blocks from one mini/chunk, use get_mini (or get_chunk) and mini.get_block.
	inline BlockType get_type(int x, int y, int z) {
		Chunk* chunk = get_chunk_containing_block(x, z);

		if (!chunk) {
			return BlockType::Air;
		}

		vmath::ivec3 chunk_coords = get_chunk_relative_coordinates(x, y, z);
		return chunk->get_block(chunk_coords);
	}

	inline BlockType get_type(vmath::ivec3 xyz) { return get_type(xyz[0], xyz[1], xyz[2]); }
	inline BlockType get_type(vmath::ivec4 xyz_) { return get_type(xyz_[0], xyz_[1], xyz_[2]); }

	inline bool check_if_covered(MiniChunk &mini) {
		// if contains any translucent blocks, don't know how to handle that yet
		if (mini.any_translucent()) {
			return false;
		}

		// none are air, so only check outside blocks
		for (int miniY = 0; miniY < MINICHUNK_HEIGHT; miniY++) {
			for (int miniZ = 0; miniZ < CHUNK_DEPTH; miniZ++) {
				for (int miniX = 0; miniX < CHUNK_WIDTH; miniX++) {
					auto &mini_coords = mini.get_coords();
					vmath::ivec3 coords = { mini_coords[0] * CHUNK_WIDTH + miniX, mini_coords[1] + miniY,  mini_coords[2] * CHUNK_DEPTH + miniZ };

					// if along east wall, check east
					if (miniX == CHUNK_WIDTH - 1) {
						if (get_type(clamp_coords_to_world(coords + IEAST)).is_translucent()) return false;
					}
					// if along west wall, check west
					if (miniX == 0) {
						if (get_type(clamp_coords_to_world(coords + IWEST)).is_translucent()) return false;
					}

					// if along north wall, check north
					if (miniZ == 0) {
						if (get_type(clamp_coords_to_world(coords + INORTH)).is_translucent()) return false;
					}
					// if along south wall, check south
					if (miniZ == CHUNK_DEPTH - 1) {
						if (get_type(clamp_coords_to_world(coords + ISOUTH)).is_translucent()) return false;
					}

					// if along bottom wall, check bottom
					if (miniY == 0) {
						if (get_type(clamp_coords_to_world(coords + IDOWN)).is_translucent()) return false;
					}
					// if along top wall, check top
					if (miniY == MINICHUNK_HEIGHT - 1) {
						if (get_type(clamp_coords_to_world(coords + IUP)).is_translucent()) return false;
					}
				}
			}
		}

		return true;
	}

	inline void render(OpenGLInfo* glInfo, const vmath::vec4(&planes)[6]) {
		// collect all the minis we're gonna draw
		vector<MiniChunk*> minis_to_draw;
		for (auto &[coords_p, chunk] : chunk_map) {
			for (auto &mini : chunk->minis) {
				if (!mini.invisible) {
					if (mini_in_frustum(&mini, planes)) {
						minis_to_draw.push_back(&mini);
					}
				}
			}
		}

		if (minis_to_draw.size() == 0) return;

		// draw them
		glUseProgram(glInfo->game_rendering_program);

		for (auto &mini : minis_to_draw) {
			mini->render_meshes(glInfo);
		}
		
		for (auto &mini : minis_to_draw) {
			mini->render_water_meshes(glInfo);
		}

		rendered++;
	}

	// check if a mini is visible in a frustum
	static inline bool mini_in_frustum(MiniChunk* mini, const vmath::vec4(&planes)[6]) {
		return sphere_in_frustrum(mini->center_coords_v3(), FRUSTUM_MINI_RADIUS_ALLOWANCE, planes);
	}

	static constexpr inline void gen_working_indices(const int &layers_idx, int &working_idx_1, int &working_idx_2) {
		// working indices are always gonna be xy, xz, or yz.
		working_idx_1 = layers_idx == 0 ? 1 : 0;
		working_idx_2 = layers_idx == 2 ? 1 : 2;
	}

	// convert 2D quads to 3D quads
	// face: for offset
	static inline vector<Quad3D> quads_2d_3d(const vector<Quad2D> &quads2d, const int layers_idx, const int layer_no, const ivec3 &face) {
		vector<Quad3D> result(quads2d.size());

		// working variable

		// working indices are always gonna be xy, xz, or yz.
		int working_idx_1, working_idx_2;
		gen_working_indices(layers_idx, working_idx_1, working_idx_2);

		// for each quad
		for (int i = 0; i < quads2d.size(); i++) {
			auto &quad3d = result[i];
			auto &quad2d = quads2d[i];

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
		}

		return result;
	}

	// generate layer by grabbing face blocks directly from the minichunk
	static inline void gen_layer_generalized(MiniChunk* mini, MiniChunk* face_mini, int layers_idx, int layer_no, const ivec3 face, BlockType(&result)[16][16]) {
		// working indices are always gonna be xy, xz, or yz.
		int working_idx_1, working_idx_2;
		gen_working_indices(layers_idx, working_idx_1, working_idx_2);

		// get coordinates of a random block
		ivec3 coords = { 0, 0, 0 };
		coords[layers_idx] = layer_no;
		ivec3 face_coords = coords + face;
		face_coords[layers_idx] = (face_coords[layers_idx] + 16) % 16;

		// make sure face not out of bounds
		assert(in_range(face_coords, ivec3(0, 0, 0), ivec3(15, 15, 15)) && "Face outside minichunk.");

		// reset all to air
		memset(result, (uint8_t)BlockType::Air, sizeof(result));

		// for each coordinate
		// if face is y, iterate on x then z (best speed)
		// if face is z, iterate on x then y (best speed)
		if (face[1] != 0 || face[2] != 0) {
			// y or z
			for (int v = 0; v < 16; v++) {
				// x
				for (int u = 0; u < 16; u++) {
					coords[working_idx_1] = u;
					coords[working_idx_2] = v;

					// get block at these coordinates
					BlockType block = mini->get_block(coords);

					// dgaf about air blocks and about invalid minis
					if (block == BlockType::Air || face_mini == nullptr) {
						continue;
					}

					// get face block
					face_coords = coords + face;
					face_coords[layers_idx] = (face_coords[layers_idx] + 16) % 16;
					BlockType face_block = face_mini->get_block(face_coords);

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
					coords[working_idx_1] = u;
					coords[working_idx_2] = v;

					// get block at these coordinates
					BlockType block = mini->get_block(coords);

					// dgaf about air blocks and about invalid minis
					if (block == BlockType::Air || face_mini == nullptr) {
						continue;
					}

					// get face block
					face_coords = coords + face;
					face_coords[layers_idx] = (face_coords[layers_idx] + 16) % 16;
					BlockType face_block = face_mini->get_block(face_coords);

					// if block's face is visible, set it
					if (is_face_visible(block, face_block)) {
						result[u][v] = block;
					}
				}
			}
		}
	}

	static inline bool is_face_visible(BlockType &block, BlockType &face_block) {
		return face_block.is_transparent() || (block != BlockType::StillWater && face_block.is_translucent()) || (face_block.is_translucent() && !block.is_translucent());
	}

	inline void gen_layer(MiniChunk* mini, int layers_idx, int layer_no, const ivec3 &face, BlockType(&result)[16][16]) {
		// working indices are always gonna be xy, xz, or yz.
		int working_idx_1, working_idx_2;
		gen_working_indices(layers_idx, working_idx_1, working_idx_2);

		// get coordinates of a random block
		ivec3 coords = { 0, 0, 0 };
		coords[layers_idx] = layer_no;
		ivec3 face_coords = coords + face;

		// figure out which mini has our face layer (usually ours)
		MiniChunk* face_mini = mini;
		if (!in_range(face_coords, ivec3(0, 0, 0), ivec3(15, 15, 15))) {
			//gen_layer_slow(mini, layers_idx, layer_no, face, result);
			auto face_mini_coords = mini->get_coords() + (layers_idx == 1 ? face * 16 : face);
			face_mini = (face_mini_coords[1] < 0 || face_mini_coords[1] > BLOCK_MAX_HEIGHT - MINICHUNK_HEIGHT) ? nullptr : get_mini(face_mini_coords);
		}

		// generate layer
		gen_layer_generalized(mini, face_mini, layers_idx, layer_no, face, result);
	}

	// given 2D array of block numbers, generate optimal quads
	static inline vector<Quad2D> gen_quads(const BlockType(&layer)[16][16]) {
		bool merged[16][16];
		memset(merged, false, sizeof(merged));

		vector<Quad2D> result;

		for (int i = 0; i < 16; i++) {
			for (int j = 0; j < 16; j++) {
				// skip merged blocks
				if (merged[i][j]) continue;

				BlockType block = layer[i][j];

				// skip air
				if (block == BlockType::Air) continue;

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
	static inline ivec2 get_max_size(const BlockType(&layer)[16][16], const bool(&merged)[16][16], ivec2 start_point, BlockType block_type) {
		assert(block_type != BlockType::Air);
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
		// TODO: Don't recreate buffer every frame!

		// Figure out corners / block types
		BlockType blocks[6];
		ivec3 corner1s[6];
		ivec3 corner2s[6];
		ivec3 faces[6];

		ivec3 mini_coords = get_mini_coords(x, y, z);

		ivec3 relative_coords = get_chunk_relative_coordinates(x, y, z);
		ivec3 block_coords = { relative_coords[0], y % 16, relative_coords[2] };

		for (int i = 0; i < 6; i++) {
			blocks[i] = BlockType::Outline; // outline
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
		GLuint mini_coords_buf;

		// create buffers with just the right sizes
		glCreateBuffers(1, &quad_block_type_buf);
		glCreateBuffers(1, &quad_corner1_buf);
		glCreateBuffers(1, &quad_corner2_buf);
		glCreateBuffers(1, &quad_face_buf);
		glCreateBuffers(1, &mini_coords_buf);

		// allocate them just enough space
		glNamedBufferStorage(quad_block_type_buf, sizeof(BlockType) * 6, blocks, NULL);
		glNamedBufferStorage(quad_corner1_buf, sizeof(ivec3) * 6, corner1s, NULL);
		glNamedBufferStorage(quad_corner2_buf, sizeof(ivec3) * 6, corner2s, NULL);
		glNamedBufferStorage(quad_face_buf, sizeof(ivec3) * 6, faces, NULL);
		glNamedBufferStorage(mini_coords_buf, sizeof(ivec3), mini_coords, NULL);

		// DRAW!

		// quad VAO
		glBindVertexArray(glInfo->vao_quad);

		// bind to quads attribute binding point
		glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->quad_block_type_bidx, quad_block_type_buf, 0, sizeof(BlockType));
		glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->q_corner1_bidx, quad_corner1_buf, 0, sizeof(ivec3));
		glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->q_corner2_bidx, quad_corner2_buf, 0, sizeof(ivec3));
		glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->q_face_bidx, quad_face_buf, 0, sizeof(ivec3));
		glVertexArrayVertexBuffer(glInfo->vao_quad, glInfo->q_base_coords_bidx, mini_coords_buf, 0, sizeof(ivec3));

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
		glNamedBufferSubData(glInfo->trans_uni_buf, sizeof(vmath::mat4), sizeof(proj_matrix), proj_matrix); // proj matrix

		// DRAW!
		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

		glDrawArrays(GL_POINTS, 0, 6);

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
		glNamedBufferSubData(glInfo->trans_uni_buf, sizeof(vmath::mat4), sizeof(proj_matrix), proj_matrix); // proj matrix

		// unbind VAO jic
		glBindVertexArray(0);

		// DELETE
		glDeleteBuffers(1, &quad_block_type_buf);
		glDeleteBuffers(1, &quad_corner1_buf);
		glDeleteBuffers(1, &quad_corner2_buf);
		glDeleteBuffers(1, &quad_face_buf);
		glDeleteBuffers(1, &mini_coords_buf);
	}

	inline void highlight_block(OpenGLInfo* glInfo, GlfwInfo* windowInfo, ivec3 xyz) { return highlight_block(glInfo, windowInfo, xyz[0], xyz[1], xyz[2]); }

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
	void on_mini_update(OpenGLInfo* glInfo, MiniChunk* mini, vmath::ivec3 block) {
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
				MiniChunkMesh* mesh = gen_minichunk_mesh(mini);
				//MiniChunkMesh* mesh = gen_minichunk_mesh_gpu(glInfo, mini);

				MiniChunkMesh* non_water = new MiniChunkMesh;
				MiniChunkMesh* water = new MiniChunkMesh;

				for (auto &quad : mesh->quads3d) {
					if ((BlockType)quad.block == BlockType::StillWater) {
						water->quads3d.push_back(quad);
					}
					else {
						non_water->quads3d.push_back(quad);
					}
				}

				assert(mesh->size() == non_water->size() + water->size());

				mini->mesh = non_water;
				mini->water_mesh = water;

				mini->update_quads_buf(glInfo);
			}
		}
	}

	void destroy_block(OpenGLInfo* glInfo, int x, int y, int z) {
		// update data
		MiniChunk* mini = get_mini_containing_block(x, y, z);
		ivec3 mini_coords = get_mini_relative_coords(x, y, z);
		mini->set_block(mini_coords, BlockType::Air);

		// regenerate textures for all neighboring minis (TODO: This should be a maximum of 3 neighbors, since >=3 sides of the destroyed block are facing its own mini.)
		on_mini_update(glInfo, mini, { x, y, z });
	}

	void destroy_block(OpenGLInfo* glInfo, ivec3 xyz) { return destroy_block(glInfo, xyz[0], xyz[1], xyz[2]); };

	void add_block(OpenGLInfo* glInfo, int x, int y, int z, BlockType block) {
		// update data
		MiniChunk* mini = get_mini_containing_block(x, y, z);
		ivec3 mini_coords = get_mini_relative_coords(x, y, z);
		mini->set_block(mini_coords, block);

		// regenerate textures for all neighboring minis (TODO: This should be a maximum of 3 neighbors, since the block always has at least 3 sides inside its mini.)
		on_mini_update(glInfo, mini, { x, y, z });
	}

	void add_block(OpenGLInfo* glInfo, ivec3 xyz, BlockType block) { return add_block(glInfo, xyz[0], xyz[1], xyz[2], block); };

	// generate a minichunk mutex from queue
	bool gen_minichunk_mesh_from_queue(vec3 player_pos) {
		// lock queue lock
		mesh_gen_mutex.lock();

		// if queue empty, return
		if (mesh_gen_queue.size() == 0) {
			mesh_gen_mutex.unlock();
			return false;
		}

		// get mini
		MiniChunk *mini = dequeue_mesh_gen();

		// no longer need queue
		mesh_gen_mutex.unlock();

		// lock mini
		mini->mesh_lock.lock();

		// update invisibility
		mini->invisible = mini->all_air() || check_if_covered(*mini);

		// if visible, update mesh
		if (!mini->invisible) {
			MiniChunkMesh* mesh = gen_minichunk_mesh(mini);

			MiniChunkMesh* non_water = new MiniChunkMesh;
			MiniChunkMesh* water = new MiniChunkMesh;

			for (auto &quad : mesh->quads3d) {
				if ((BlockType)quad.block == BlockType::StillWater) {
					water->quads3d.push_back(quad);
				}
				else {
					non_water->quads3d.push_back(quad);
				}
			}

			assert(mesh->size() == non_water->size() + water->size());

			mini->mesh = non_water;
			mini->water_mesh = water;
			mini->meshes_updated = true;
		}

		// unlock mini
		mini->mesh_lock.unlock();

		// generated mini
		return true;
	}

	MiniChunkMesh* gen_minichunk_mesh(MiniChunk* mini);
};

#endif /* __WORLD_H__ */
