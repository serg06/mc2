#include "minichunk.h"
#include "world.h"

#include <chrono>

/*************************************************************/
/* PLACING TESTS IN HERE UNTIL I LEARN HOW TO DO IT PROPERLY */
/*************************************************************/


namespace WorldTests {
	void run_all_tests(OpenGLInfo* glInfo) {
		test_gen_quads();
		test_mark_as_merged();
		test_get_max_size();
		//test_gen_layer();
		OutputDebugString("WorldTests completed successfully.\n");
	}

	//void test_gen_layer() {
	//	// gen chunk at 0,0
	//	Chunk* chunk = new Chunk({ 0, 0 });
	//	chunk->generate();

	//	// grab first mini that has stone, grass, and air blocks
	//	MiniChunk* mini;
	//	for (auto &mini2 : chunk->minis) {
	//		bool has_air = false, has_stone = false, has_grass = false;
	//		for (int i = 0; i < MINICHUNK_SIZE; i++) {
	//			has_air |= mini2.blocks[i] == BlockType::Air;
	//			has_stone |= mini2.blocks[i] == BlockType::Stone;
	//			has_grass |= mini2.blocks[i] == BlockType::Grass;
	//		}

	//		if (has_air && has_stone && has_grass) {
	//			mini = &mini2;
	//		}
	//	}

	//	if (mini == nullptr) {
	//		throw "No sufficient minis!";
	//	}

	//	// grab 2nd layer facing us in z direction
	//	BlockType result[16][16];
	//	int z = 1;
	//	ivec3 face = { 0, 0, -1 };

	//	for (int x = 0; x < 16; x++) {
	//		for (int y = 0; y < 16; y++) {
	//			// set working indices (TODO: move u to outer loop)
	//			ivec3 coords = { x, y, z };

	//			// get block at these coordinates
	//			BlockType block = mini->get_block(coords);

	//			// dgaf about air blocks
	//			if (block == BlockType::Air) {
	//				continue;
	//			}

	//			// get face block
	//			BlockType face_block = mini->get_block(coords + face);

	//			// if block's face is visible, set it
	//			if (World::is_face_visible(block, face_block)) {
	//				result[x][y] = block;
	//			}
	//		}
	//	}

	//	// Do the same with gen_layer_fast
	//	BlockType expected[16][16];
	//	World::gen_layer_generalized(mini, mini, 2, 1, face, expected);

	//	// Make sure they're the same
	//	for (int x = 0; x < 16; x++) {
	//		for (int y = 0; y < 16; y++) {
	//			if (result[x][y] != expected[x][y]) {
	//				throw "It broke!";
	//			}
	//		}
	//	}

	//	// free
	//	chunk->free();
	//}

	void test_gen_quads() {
		// create layer of all air
		BlockType layer[16][16];
		memset(layer, (uint8_t)BlockType::Air, 16 * 16 * sizeof(BlockType));

		// 1. Add rectangle from (1,3) to (3,6)
		for (int i = 1; i <= 3; i++) {
			for (int j = 3; j <= 6; j++) {
				layer[i][j] = BlockType::Stone;
			}
		}
		// expected result
		Quad2D q1;
		q1.block = BlockType::Stone;
		q1.corners[0] = { 1, 3 };
		q1.corners[1] = { 4, 7 };

		// 2. Add plus symbol - line from (7,5)->(7,9), and (5,7)->(9,7)
		for (int i = 7; i <= 7; i++) {
			for (int j = 5; j <= 9; j++) {
				layer[i][j] = BlockType::Stone;
			}
		}
		for (int i = 5; i <= 9; i++) {
			for (int j = 7; j <= 7; j++) {
				layer[i][j] = BlockType::Stone;
			}
		}
		// expected result
		vector<Quad2D> vq2;
		Quad2D q2;
		q2.block = BlockType::Stone;

		q2.corners[0] = { 5, 7 };
		q2.corners[1] = { 10, 8 };
		vq2.push_back(q2);

		q2.corners[0] = { 7, 5 };
		q2.corners[1] = { 8, 7 };
		vq2.push_back(q2);

		q2.corners[0] = { 7, 8 };
		q2.corners[1] = { 8, 10 };
		vq2.push_back(q2);

		// Finally, add line all along bottom
		for (int i = 0; i <= 15; i++) {
			for (int j = 15; j <= 15; j++) {
				layer[i][j] = BlockType::Grass;
			}
		}
		// expected result
		Quad2D q3;
		q3.block = BlockType::Grass;
		q3.corners[0] = { 0, 15 };
		q3.corners[1] = { 16, 16 };

		bool merged[16][16];
		vector<Quad2D> result = World::gen_quads(layer, merged);

		assert(result.size() == 5 && "wrong number of results");
		assert(find(begin(result), end(result), q1) != end(result) && "q1 not in results list");
		for (auto q : vq2) {
			assert(find(begin(result), end(result), q) != end(result) && "q2's element not in results list");
		}
		assert(find(begin(result), end(result), q3) != end(result) && "q3 not in results list");
	}

	void test_mark_as_merged() {
		bool merged[16][16];
		memset(merged, 0, 16 * 16 * sizeof(bool));

		ivec2 start = { 3, 4 };
		ivec2 max_size = { 2, 5 };

		World::mark_as_merged(merged, start, max_size);

		for (int i = 0; i < 16; i++) {
			for (int j = 0; j < 16; j++) {
				// if in right x-range and y-range
				if (start[0] <= i && i <= start[0] + max_size[0] - 1 && start[1] <= j && j <= start[1] + max_size[1] - 1) {
					// make sure merged
					if (!merged[i][j]) {
						throw "not merged when should be!";
					}
				}
				// else make sure not merged
				else {
					if (merged[i][j]) {
						throw "merged when shouldn't be!";
					}
				}
			}
		}
	}

	void test_get_max_size() {
		return; // TODO: fix

		// create layer of all air
		BlockType layer[16][16];
		memset(layer, (uint8_t)BlockType::Air, 16 * 16 * sizeof(BlockType));

		// let's say nothing is merged yet
		bool merged[16][16];
		memset(merged, false, 16 * 16 * sizeof(bool));

		// 1. Add rectangle from (1,3) to (3,6)
		for (int i = 1; i <= 3; i++) {
			for (int j = 3; j <= 6; j++) {
				layer[i][j] = BlockType::Stone;
			}
		}

		// 2. Add plus symbol - line from (7,5)->(7,9), and (5,7)->(9,7)
		for (int i = 7; i <= 7; i++) {
			for (int j = 5; j <= 9; j++) {
				layer[i][j] = BlockType::Stone;
			}
		}
		for (int i = 5; i <= 9; i++) {
			for (int j = 7; j <= 7; j++) {
				layer[i][j] = BlockType::Stone;
			}
		}

		// 3. Add line all along bottom
		for (int i = 0; i <= 15; i++) {
			for (int j = 15; j <= 15; j++) {
				layer[i][j] = BlockType::Grass;
			}
		}

		// Get max size for rectangle top-left-corner
		ivec2 max_size1 = World::get_max_size(layer, merged, { 1, 3 }, BlockType::Stone);
		if (max_size1[0] != 3 || max_size1[1] != 4) {
			throw "wrong max_size1";
		}

		// Get max size for plus center
		ivec2 max_size2 = World::get_max_size(layer, merged, { 7, 7 }, BlockType::Stone);
		if (max_size2[0] != 3 || max_size2[1] != 1) {
			throw "wrong max_size2";
		}

	}

	// given a layer and start point, find its best dimensions
	inline ivec2 get_max_size(BlockType layer[16][16], ivec2 start_point, BlockType block_type) {
		assert(block_type != BlockType::Air);

		// TODO: Start max size at {1,1}, and for loops at +1.
		// TODO: Search width with find() instead of a for loop.

		// "max width and height"
		ivec2 max_size = { 0, 0 };

		// maximize width
		for (int i = start_point[0], j = start_point[1]; i < 16; i++) {
			// if extended by 1, add 1 to max width
			if (layer[i][j] == block_type) {
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
				if (layer[i][j] != block_type) {
					break;
				}
			}

			// yep, entire width is correct! Extend max height and keep going
			max_size[1]++;
		}

		assert(max_size[1] > 0 && "WTF? Max height is 0? Doesn't make sense.");
	}

}

std::unique_ptr<MiniChunkMesh> World::gen_minichunk_mesh(MiniChunk* mini) {
	// got our mesh
	std::unique_ptr<MiniChunkMesh> mesh = std::make_unique<MiniChunkMesh>();

	// for all 6 sides
	for (int i = 0; i < 6; i++) {
		bool backface = i < 3;
		int layers_idx = i % 3;

		// most efficient to traverse working_idx_1 then working_idx_2;
		int working_idx_1, working_idx_2;
		gen_working_indices(layers_idx, working_idx_1, working_idx_2);

		// generate face variable
		ivec3 face = { 0, 0, 0 };
		// I don't think it matters whether we start with front or back face, as long as we switch halfway through.
		// BACKFACE => +X/+Y/+Z SIDE. 
		face[layers_idx] = backface ? -1 : 1;

		// for each layer
		for (int i = 0; i < 16; i++) {
			BlockType layer[16][16];
			bool merged[16][16];

			// extract it from the data
			gen_layer(mini, layers_idx, i, face, layer);

			// get quads from layer
			vector<Quad2D> quads2d = gen_quads(layer, merged);

			// if -x, -y, or +z, flip triangles around so that we're not drawing them backwards
			if (face[0] < 0 || face[1] < 0 || face[2] > 0) {
				for (auto& quad2d : quads2d) {
					ivec2 diffs = quad2d.corners[1] - quad2d.corners[0];
					quad2d.corners[0][0] += diffs[0];
					quad2d.corners[1][0] -= diffs[0];
				}
			}

			// TODO: rotate texture sides the correct way. (It's noticeable when placing down diamond block.)
			// -> Or alternatively, can just rotate texture lmao.

			// convert quads back to 3D coordinates
			vector<Quad3D> quads = quads_2d_3d(quads2d, layers_idx, i, face);

			// if not backface (i.e. not facing (0,0,0)), move 1 forwards
			if (face[0] > 0 || face[1] > 0 || face[2] > 0) {
				for (auto& quad : quads) {
					quad.corner1 += face;
					quad.corner2 += face;
				}
			}

			// append quads
			for (auto quad : quads) {
				mesh->add_quad(quad);
			}
		}
	}

	return mesh;
}

std::unique_ptr<MiniChunkMesh> World::gen_minichunk_mesh(std::shared_ptr<MeshGenRequest> req) {
	// got our mesh
	std::unique_ptr<MiniChunkMesh> mesh = std::make_unique<MiniChunkMesh>();

	// for all 6 sides
	for (int i = 0; i < 6; i++) {
		bool backface = i < 3;
		int layers_idx = i % 3;

		// most efficient to traverse working_idx_1 then working_idx_2;
		int working_idx_1, working_idx_2;
		gen_working_indices(layers_idx, working_idx_1, working_idx_2);

		// generate face variable
		ivec3 face = { 0, 0, 0 };
		// I don't think it matters whether we start with front or back face, as long as we switch halfway through.
		// BACKFACE => +X/+Y/+Z SIDE. 
		face[layers_idx] = backface ? -1 : 1;

		// for each layer
		for (int i = 0; i < 16; i++) {
			BlockType layer[16][16];
			bool merged[16][16];

			// extract it from the data
			gen_layer(req, layers_idx, i, face, layer);

			// get quads from layer
			vector<Quad2D> quads2d = gen_quads(layer, merged);

			// if -x, -y, or +z, flip triangles around so that we're not drawing them backwards
			if (face[0] < 0 || face[1] < 0 || face[2] > 0) {
				for (auto& quad2d : quads2d) {
					ivec2 diffs = quad2d.corners[1] - quad2d.corners[0];
					quad2d.corners[0][0] += diffs[0];
					quad2d.corners[1][0] -= diffs[0];
				}
			}

			// TODO: rotate texture sides the correct way. (It's noticeable when placing down diamond block.)
			// -> Or alternatively, can just rotate texture lmao.

			// convert quads back to 3D coordinates
			vector<Quad3D> quads = quads_2d_3d(quads2d, layers_idx, i, face);

			// if not backface (i.e. not facing (0,0,0)), move 1 forwards
			if (face[0] > 0 || face[1] > 0 || face[2] > 0) {
				for (auto& quad : quads) {
					quad.corner1 += face;
					quad.corner2 += face;
				}
			}

			// append quads
			for (auto quad : quads) {
				mesh->add_quad(quad);
			}
		}
	}

	return mesh;
}


