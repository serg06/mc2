#include "world.h"

/*************************************************************/
/* PLACING TESTS IN HERE UNTIL I LEARN HOW TO DO IT PROPERLY */
/*************************************************************/

namespace WorldTests {

	void run_all_tests() {
		test_gen_quads();
		test_mark_as_merged();
		test_get_max_size();
	}

	void test_gen_quads() {
		// create layer of all air
		Block layer[16][16];
		memset(layer, (uint8_t)Block::Air, 16 * 16 * sizeof(Block));

		// 1. Add rectangle from (1,3) to (3,6)
		for (int i = 1; i <= 3; i++) {
			for (int j = 3; j <= 6; j++) {
				layer[i][j] = Block::Stone;
			}
		}

		//// 2. Add plus symbol - line from (7,7)->(7,11), and (5,7)->(9,7)
		//for (int i = 7; i <= 7; i++) {
		//	for (int j = 7; j <= 11; j++) {
		//		layer[i][j] = Block::Stone;
		//	}
		//}
		//for (int i = 5; i <= 9; i++) {
		//	for (int j = 7; j <= 7; j++) {
		//		layer[i][j] = Block::Stone;
		//	}
		//}
		//// expected result
		//Quad2D q2;
		//q2.block = Block::Stone;
		//q2.corners[0] = { 0, 15 };
		//q2.corners[1] = { 16, 16 };

		//// Finally, add line all along bottom
		//for (int i = 0; i <= 15; i++) {
		//	for (int j = 15; j <= 15; j++) {
		//		layer[i][j] = Block::Grass;
		//	}
		//}
		//// expected result
		//Quad2D q3;
		//q3.block = Block::Grass;
		//q3.corners[0] = { 0, 15 };
		//q3.corners[1] = { 16, 16 };

		vector<Quad2D> result = World::gen_quads(layer);

		OutputDebugString("");
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
		// create layer of all air
		Block layer[16][16];
		memset(layer, (uint8_t)Block::Air, 16 * 16 * sizeof(Block));

		// let's say nothing is merged yet
		bool merged[16][16];
		memset(merged, false, 16 * 16 * sizeof(bool));

		// 1. Add rectangle from (1,3) to (3,6)
		for (int i = 1; i <= 3; i++) {
			for (int j = 3; j <= 6; j++) {
				layer[i][j] = Block::Stone;
			}
		}

		// 2. Add plus symbol - line from (7,7)->(7,11), and (5,7)->(9,7)
		for (int i = 7; i <= 7; i++) {
			for (int j = 7; j <= 11; j++) {
				layer[i][j] = Block::Stone;
			}
		}
		for (int i = 5; i <= 9; i++) {
			for (int j = 7; j <= 7; j++) {
				layer[i][j] = Block::Stone;
			}
		}

		// Finally, add line all along bottom
		for (int i = 0; i <= 15; i++) {
			for (int j = 15; j <= 15; j++) {
				layer[i][j] = Block::Grass;
			}
		}

		// Now let's try to get max size for all of them
		ivec2 max_size1 = World::get_max_size(layer, merged, { 1, 3 }, Block::Stone);
		if (max_size1[0] != 3 || max_size1[1] != 4) {
			throw "wrong max_size1";
		}

		OutputDebugString("");
	}

	// given a layer and start point, find its best dimensions
	inline ivec2 get_max_size(Block layer[16][16], ivec2 start_point, Block block_type) {
		assert(block_type != Block::Air);
	
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
