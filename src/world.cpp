#include "world.h"

#include <chrono>

/*************************************************************/
/* PLACING TESTS IN HERE UNTIL I LEARN HOW TO DO IT PROPERLY */
/*************************************************************/

namespace WorldTests {

	void run_all_tests(OpenGLInfo *glInfo) {
		test_gen_quads();
		test_mark_as_merged();
		test_get_max_size();
		test_gen_layer();
		test_gen_layers_compute_shader(glInfo);
		OutputDebugString("WorldTests completed successfully.\n");
	}

	//void get_layer_from_cs_output(unsigned *output, ivec3 face, int layer_idx, unsigned(&result)[16 * 16]) {
	//	int face_idx = face[0] != 0 ? 0 : face[1] != 0 ? 1 : 2;
	//	assert(face[face_idx] != 0);
	//	bool backface = face[face_idx] < 0; // double check
	//	int working_idx_1, working_idx_2;
	//	gen_working_indices(face_idx, working_idx_1, working_idx_2);

	//	// get global face idx
	//	int global_face_idx = face_idx;
	//	if (!backface) {
	//		global_face_idx += 3;
	//	}

	//	// get resulting layer idx
	//	int result_layer_idx = global_face_idx * 15 + layer_idx;

	//	unsigned *layer_start = output + result_layer_idx * 16 * 16;

	//	// extract
	//	for (int u = 0; u < 16; u++) {
	//		for (int v = 0; v < 16; v++) {
	//			result[u + v * 16] = layer_start[u + v * 16];
	//		}
	//	}

	//	/*
	//	uint result_layer_idx = global_face_idx * 15 + layer_idx;
	//	layers[u + v * 16 + result_layer_idx * 16 * 16] = val;
	//	*/
	//}

	void print_nonzero_cs_output_layers(unsigned *output) {
		// for each face
		for (int global_face_idx = 0; global_face_idx < 6; global_face_idx++) {
			bool backface = global_face_idx < 3;
			ivec3 face = { 0, 0, 0 };
			face[global_face_idx % 3] = backface ? -1 : 1;

			int working_idx_1, working_idx_2;
			gen_working_indices(global_face_idx % 3, working_idx_1, working_idx_2);

			// go through layers one-by-one
			// FUCK IT PRINT ALL LAYERS
			for (int layer_idx = 0; layer_idx < 16; layer_idx++) {
				//// if backface, want layers 1-15 instead of 0-14
				//if (backface) {
				//	layer_idx++;
				//}

				// get start of layer
				unsigned *layer_start = output + layer_idx * 16 * 16 + global_face_idx * 16 * 16 * 16;

				// check if layer has non-zero element
				bool has_nonzero = false;
				for (int i = 0; i < 16 * 16; i++) {
					if (layer_start[i] != 0) {
						has_nonzero = true;
						break;
					}
				}

				// if layer has a non-zero element, print it
				if (has_nonzero) {
					OutputDebugString("NONZERO LAYER:\n");
					char *result = new char[16 * 16 * 32]; // up to 32 chars per block type
					char* tmp = result;

					char *nonzero_coords = new char[16 * 16 * 128]; // up to 1 vertex per block, 128 chars per vertex
					char* tmp2 = nonzero_coords;
					tmp2 += sprintf(tmp2, "Coords: ");

					for (int u = 0; u < 16; u++) {
						tmp += sprintf(tmp, "[ ");

						for (int v = 0; v < 16; v++) {
							ivec3 coords = { 0, 0, 0 };
							coords[global_face_idx % 3] = layer_idx;
							coords[working_idx_1] = u;
							coords[working_idx_2] = v;

							tmp += sprintf(tmp, "%d ", layer_start[u + v * 16]);
							OutputDebugString("");

							if (layer_start[u + v * 16] != 0) {
								char* s = vec2str(coords);
								tmp2 += sprintf(tmp2, "%s[layer_idx = %d, backface = %s] ", s, layer_idx, backface ? "TRUE" : "FALSE");
								delete[] s;
							}
						}

						tmp += sprintf(tmp, "]\n");
					}

					OutputDebugString(result);

					OutputDebugString(nonzero_coords);
					OutputDebugString("\n");

					OutputDebugString("\n");

					delete[] result;
					delete[] nonzero_coords;
				}

				//if (backface) {
				//	layer_idx--;
				//}
			}
		}
		OutputDebugString("");
		// LATEST:
		// DONE! WORKS!
		// NOW TRY RUNNING IT ON THE ACTUAL MINI AT (0,64,0) AND MAKING SURE IT MATCHES GEN_LAYERS!
	}

	void test_gen_layers_compute_shader(OpenGLInfo *glInfo) {
		// gen chunk at 0,0
		//Chunk* chunk = gen_chunk_data(0, 0);

		// generate 128 minichunks
		Chunk* chunks[8];
		for (int i = 0; i < 8; i++) {
			chunks[i] = gen_chunk_data(0, i);
		}

		//// grab mini at 0,0,0
		//MiniChunk &mini = chunks[0]->minis[0];

		//// set all to air
		//mini.set_all_air();

		//// have a single block -- at (0,0,0) -- be stone
		//// so 3 of its faces should be visible.

		//mini.set_block(5, 5, 5, Block::Stone);
		//mini.set_block(5, 6, 5, Block::Stone);
		//mini.set_block(6, 5, 5, Block::Stone);
		//mini.set_block(6, 6, 5, Block::Stone);

		// use program
		glUseProgram(glInfo->gen_layer_program);

		// fill input buffer
		for (int i = 0; i < 8; i++) {
			for (int j = 0; j < 16; j++) {
				auto &mini = chunks[i]->minis[j];

				unsigned data[MINICHUNK_SIZE];
				for (int k = 0; k < MINICHUNK_SIZE; k++) {
					data[k] = (uint8_t)mini.data[k];
				}
				glNamedBufferSubData(glInfo->gen_layer_mini_buf, (i*16 + j) * MINICHUNK_SIZE * sizeof(unsigned), MINICHUNK_SIZE * sizeof(unsigned), data);
			}
		}

		//// fill output buf with 0 (DEBUG)
		//unsigned *empty = new unsigned[16 * 16 * 96];
		//memset(empty, 0, 16 * 16 * 96 * sizeof(unsigned));
		//glNamedBufferSubData(glInfo->gen_layer_layers_buf, 0, 16 * 16 * 96 * sizeof(unsigned), empty);

		auto start_compute_1 = std::chrono::high_resolution_clock::now();

		// run program
		glDispatchCompute(
			128, // 128 minichunks
			6, // 6 faces
			15 // 15 layers per face
		);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		auto finish_compute_1 = std::chrono::high_resolution_clock::now();
		long result_compute_1 = std::chrono::duration_cast<std::chrono::nanoseconds>(finish_compute_1 - start_compute_1).count();

		// TODO: REMOVE
		Block tmp[16][16];

		auto start_manual_1 = std::chrono::high_resolution_clock::now();

		// for each face
		for (int i = 0; i < 8; i++) {
			for (int j = 0; j < 16; j++) {
				auto &mini = chunks[i]->minis[j];
				
				for (int k = 0; k < 6; k++) {
					int face_idx = k % 3;
					int backface = k < 3;
					ivec3 face = { 0, 0, 0 };
					face[face_idx] = backface ? -1 : 1;

					// for every item you can get using gen_layer_fast
					for (int layer_no = 0; layer_no < 15; layer_no++) {
						// if backface, want layers 1-15 instead of 0-14
						if (backface) {
							layer_no++;
						}
						World::gen_layer_fast(&mini, face_idx, layer_no, face, tmp);
						if (backface) {
							layer_no--;
						}
					}
				}
			}
		}

		auto finish_manual_1 = std::chrono::high_resolution_clock::now();
		long result_manual_1 = std::chrono::duration_cast<std::chrono::nanoseconds>(finish_manual_1 - start_manual_1).count();

		char buf[256];
		sprintf(buf, "compute shader time: %ld\nmanual time: %ld\n", result_compute_1, result_manual_1);
		OutputDebugString(buf);


		//// read and print results
		//for (int i = 0; i < 8; i++) {
		//	for (int j = 0; j < 16; j++) {
		//		unsigned output[16 * 16 * 96]; // 96 16x16 layers
		//		glGetNamedBufferSubData(glInfo->gen_layer_layers_buf, 16 * 16 * 96 * sizeof(unsigned) * (i*16 + j), 16 * 16 * 96 * sizeof(unsigned), output);
		//		print_nonzero_cs_output_layers(output);
		//	}
		//}

		// read and print results for chunk[0]->minis[4] ((0, 64, 0))
		// Looks right to me!!
		unsigned output[16 * 16 * 96]; // 96 16x16 layers
		glGetNamedBufferSubData(glInfo->gen_layer_layers_buf, 16 * 16 * 96 * sizeof(unsigned) * (0*16 + 4), 16 * 16 * 96 * sizeof(unsigned), output);
		print_nonzero_cs_output_layers(output);

		//// count all non-zero faces
		//int num_nonzero = 0;
		//for (int i = 0; i < 16 * 16 * 96; i++) {
		//	if (output[i] != 0) {
		//		num_nonzero += 1;
		//	}
		//}

		//ivec3 face = { 0, 0, -1 };
		//get_layer_from_cs_output(output, face, 14, result);

		//print_nonzero_cs_output_layers(output);

		OutputDebugString("wait here!\n");
	}

	//void test_gen_layers_compute_shader2(OpenGLInfo *glInfo) {
	//	// gen chunk at 0,0
	//	Chunk* chunk = gen_chunk_data(0, 0);

	//	// grab first mini that has stone, grass, and air blocks
	//	MiniChunk* mini;
	//	for (auto &mini2 : chunk->minis) {
	//		bool has_air = false, has_stone = false, has_grass = false;
	//		for (int i = 0; i < MINICHUNK_SIZE; i++) {
	//			has_air |= mini2.data[i] == Block::Air;
	//			has_stone |= mini2.data[i] == Block::Stone;
	//			has_grass |= mini2.data[i] == Block::Grass;
	//		}

	//		if (has_air && has_stone && has_grass) {
	//			mini = &mini2;
	//		}
	//	}

	//	if (mini == nullptr) {
	//		throw "No sufficient minis!";
	//	}

	//	char buf[256];
	//	char* s = vec2str(mini->real_coords());
	//	sprintf(buf, "Grabbed mini: %s.\n", s);
	//	delete[] s;
	//	OutputDebugString(buf);

	//	char* layer_str = mini->print_layer(2, 1);
	//	char* face_str = mini->print_layer(2, 0);

	//	OutputDebugString("LAYER:\n");
	//	OutputDebugString(layer_str);
	//	OutputDebugString("FACE:\n");
	//	OutputDebugString(face_str);


	//	// use program
	//	glUseProgram(glInfo->gen_layer_program);

	//	// fill input buffer
	//	unsigned data[MINICHUNK_SIZE];
	//	for (int i = 0; i < MINICHUNK_SIZE; i++) {
	//		data[i] = (uint8_t)mini->data[i];
	//	}
	//	glNamedBufferSubData(glInfo->gen_layer_mini_buf, 0, MINICHUNK_SIZE * sizeof(unsigned), data);

	//	// TODO: This part and shit.
	//	//int face_idx = 0;
	//	//bool backface = true;
	//	//int working_idx_1, working_idx_2;
	//	//gen_working_indices(face_idx, working_idx_1, working_idx_2);
	//	//ivec3 face = { 0, 0, 0 };
	//	//face[face_idx] = backface ? -1 : 1;
	//	int face_idx = 2;
	//	ivec3 face = { 0, 0, -1 };

	//	// run program
	//	glDispatchCompute(
	//		1, // 1 minichunk
	//		6, // 6 faces
	//		15 // 15 layers per face
	//	);
	//	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	//	// read result
	//	unsigned output[16 * 16 * 90]; // 90 16x16 layers
	//	glGetNamedBufferSubData(glInfo->gen_layer_layers_buf, 0, 16 * 16 * 90 * sizeof(unsigned), output);

	//	// read result along z in -z face direction
	//	unsigned result_layer_idx = face_idx * 15 + 1;

	//	unsigned neg_z_layer[16 * 16];
	//	memcpy(neg_z_layer, output + result_layer_idx * 16 * 16, 16 * 16 * sizeof(unsigned));

	//	// Get same thing with gen_layer_fast!
	//	Block expected_blocks[16][16];
	//	World::gen_layer_fast(mini, 2, 1, face, expected_blocks);
	//	unsigned expected1[16 * 16];
	//	unsigned expected2[16 * 16];
	//	for (int x = 0; x < 16; x++) {
	//		for (int y = 0; y < 16; y++) {
	//			expected1[x + y * 16] = (uint8_t)expected_blocks[x][y];
	//			expected2[y + x * 16] = (uint8_t)expected_blocks[x][y];
	//		}
	//	}

	//	bool any_grass = false;
	//	for (int i = 0; i < 16; i++) {
	//		for (int j = 0; j < 16; j++) {
	//			if (expected_blocks[i][j] == Block::Grass) {
	//				any_grass = true;
	//			}
	//		}
	//	}

	//	//void set_block(const uint u, const uint v, const uint face_idx, const uint layer_idx, const uint val) {
	//	//	uint result_layer_idx = face_idx * 15 + layer_idx;
	//	//	layers[u + v * 16 + result_layer_idx * 16 * 16] = val;
	//	//}

	//	OutputDebugString("wait here!\n");
	//}

	void test_gen_layer() {
		// gen chunk at 0,0
		Chunk* chunk = gen_chunk_data(0, 0);

		// grab first mini that has stone, grass, and air blocks
		MiniChunk* mini;
		for (auto &mini2 : chunk->minis) {
			bool has_air = false, has_stone = false, has_grass = false;
			for (int i = 0; i < MINICHUNK_SIZE; i++) {
				has_air |= mini2.data[i] == Block::Air;
				has_stone |= mini2.data[i] == Block::Stone;
				has_grass |= mini2.data[i] == Block::Grass;
			}

			if (has_air && has_stone && has_grass) {
				mini = &mini2;
			}
		}

		if (mini == nullptr) {
			throw "No sufficient minis!";
		}

		// grab 2nd layer facing us in z direction
		Block result[16][16];
		int z = 1;
		ivec3 face = { 0, 0, -1 };

		for (int x = 0; x < 16; x++) {
			for (int y = 0; y < 16; y++) {
				// set working indices (TODO: move u to outer loop)
				ivec3 coords = { x, y, z };

				// get block at these coordinates
				Block block = mini->get_block(coords);

				// dgaf about air blocks
				if (block == Block::Air) {
					continue;
				}

				// get face block
				Block face_block = mini->get_block(coords + face);

				// if block's face is visible, set it
				if (World::is_face_visible(block, face_block)) {
					result[x][y] = block;
				}
			}
		}

		// Do the same with gen_layer_fast
		Block expected[16][16];
		World::gen_layer_fast(mini, 2, 1, face, expected);

		// Make sure they're the same
		for (int x = 0; x < 16; x++) {
			for (int y = 0; y < 16; y++) {
				if (result[x][y] != expected[x][y]) {
					throw "It broke!";
				}
			}
		}

		// free
		chunk->free_data();
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
		// expected result
		Quad2D q1;
		q1.block = Block::Stone;
		q1.corners[0] = { 1, 3 };
		q1.corners[1] = { 4, 7 };

		// 2. Add plus symbol - line from (7,5)->(7,9), and (5,7)->(9,7)
		for (int i = 7; i <= 7; i++) {
			for (int j = 5; j <= 9; j++) {
				layer[i][j] = Block::Stone;
			}
		}
		for (int i = 5; i <= 9; i++) {
			for (int j = 7; j <= 7; j++) {
				layer[i][j] = Block::Stone;
			}
		}
		// expected result
		vector<Quad2D> vq2;
		Quad2D q2;
		q2.block = Block::Stone;

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
				layer[i][j] = Block::Grass;
			}
		}
		// expected result
		Quad2D q3;
		q3.block = Block::Grass;
		q3.corners[0] = { 0, 15 };
		q3.corners[1] = { 16, 16 };

		vector<Quad2D> result = World::gen_quads(layer);

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

		// 2. Add plus symbol - line from (7,5)->(7,9), and (5,7)->(9,7)
		for (int i = 7; i <= 7; i++) {
			for (int j = 5; j <= 9; j++) {
				layer[i][j] = Block::Stone;
			}
		}
		for (int i = 5; i <= 9; i++) {
			for (int j = 7; j <= 7; j++) {
				layer[i][j] = Block::Stone;
			}
		}

		// 3. Add line all along bottom
		for (int i = 0; i <= 15; i++) {
			for (int j = 15; j <= 15; j++) {
				layer[i][j] = Block::Grass;
			}
		}

		// Get max size for rectangle top-left-corner
		ivec2 max_size1 = World::get_max_size(layer, merged, { 1, 3 }, Block::Stone);
		if (max_size1[0] != 3 || max_size1[1] != 4) {
			throw "wrong max_size1";
		}

		// Get max size for plus center
		ivec2 max_size2 = World::get_max_size(layer, merged, { 7, 7 }, Block::Stone);
		if (max_size2[0] != 3 || max_size2[1] != 1) {
			throw "wrong max_size2";
		}

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

MiniChunkMesh* World::gen_minichunk_mesh(MiniChunk* mini) {
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

			// get quads from layer
			vector<Quad2D> quads2d = gen_quads(layer);

			// if -x, -y, or +z, flip triangles around so that we're not drawing them backwards
			if (face[0] < 0 || face[1] < 0 || face[2] > 0) {
				for (auto &quad2d : quads2d) {
					ivec2 diffs = quad2d.corners[1] - quad2d.corners[0];
					quad2d.corners[0][0] += diffs[0];
					quad2d.corners[1][0] -= diffs[0];
				}
			}

			// convert quads back to 3D coordinates
			vector<Quad3D> quads = quads_2d_3d(quads2d, layers_idx, i, face);

			// if -x, -y, or -z, move 1 forwards
			if (face[0] > 0 || face[1] > 0 || face[2] > 0) {
				for (auto &quad : quads) {
					quad.corners[0] += face;
					quad.corners[1] += face;
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

