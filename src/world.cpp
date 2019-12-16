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

	// compare ALL the layers!
	static void compare_all_layers(unsigned *output, World* world, MiniChunk* mini, unsigned global_minichunk_idx) {
		Block output_layer[16][16];
		Block expected_layer[16][16];

		int output_nonzeros = 0;
		int expected_nonzeros = 0;

		// for each face
		for (int global_face_idx = 0; global_face_idx < 6; global_face_idx++) {
			int local_face_idx = global_face_idx % 3;
			bool backface = global_face_idx < 3;

			ivec3 face = ivec3(0, 0, 0);
			face[local_face_idx] = backface ? -1 : 1;

			// working indices are always gonna be xy, xz, or yz.
			int working_idx_1 = local_face_idx == 0 ? 1 : 0;
			int working_idx_2 = local_face_idx == 2 ? 1 : 2;

			// for each layer
			for (int layer_idx = 0; layer_idx < 16; layer_idx++) {
				// get layers
				World::extract_layer(output, layer_idx, global_face_idx, global_minichunk_idx, output_layer);
				world->gen_layer(mini, local_face_idx, layer_idx, face, expected_layer);

				// compare
				for (int u = 0; u < 16; u++) {
					for (int v = 0; v < 16; v++) {
						if (output_layer[u][v] != Block::Air) {
							output_nonzeros++;
						}
						if (expected_layer[u][v] != Block::Air) {
							expected_nonzeros++;
						}
						assert(output_layer[u][v] == expected_layer[u][v]);
					}
				}
			}
		}

		assert(output_nonzeros == expected_nonzeros && "failed compare_all_layers");

		OutputDebugString("");
	}

	// compare only the fast layers
	static void compare_fast_layers(unsigned *output, MiniChunk* mini, unsigned global_minichunk_idx) {
		Block output_layer[16][16];
		Block expected_layer[16][16];

		int output_nonzeros = 0;
		int expected_nonzeros = 0;

		// for each face
		for (int global_face_idx = 0; global_face_idx < 6; global_face_idx++) {
			int local_face_idx = global_face_idx % 3;
			bool backface = global_face_idx < 3;

			ivec3 face = ivec3(0, 0, 0);
			face[local_face_idx] = backface ? -1 : 1;

			// working indices are always gonna be xy, xz, or yz.
			int working_idx_1 = local_face_idx == 0 ? 1 : 0;
			int working_idx_2 = local_face_idx == 2 ? 1 : 2;

			// for each FAST layer
			for (int layer_idx = 0; layer_idx < 15; layer_idx++) {
				// if backface, can't do 1st layer
				if (backface) {
					layer_idx++;
				}

				// get layers
				World::extract_layer(output, layer_idx, global_face_idx, global_minichunk_idx, output_layer);
				World::gen_layer_fast(mini, local_face_idx, layer_idx, face, expected_layer);

				// compare
				for (int u = 0; u < 16; u++) {
					for (int v = 0; v < 16; v++) {
						if (output_layer[u][v] != Block::Air) {
							output_nonzeros++;
						}
						if (expected_layer[u][v] != Block::Air) {
							expected_nonzeros++;
						}
						assert(output_layer[u][v] == expected_layer[u][v]);
					}
				}

				if (backface) {
					layer_idx--;
				}
			}
		}

		assert(output_nonzeros == expected_nonzeros && "failed compare_fast_layers");

		OutputDebugString("");
	}

	static void print_nonzero_cs_output_layers(unsigned *output) {
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

	static void test_gen_layers_compute_shader(OpenGLInfo *glInfo) {
		World world;

		char buf[1024];

		// gen chunk at 0,0
		//Chunk* chunk = gen_chunk_data(0, 0);

		// 16 chunks = 256 minis
#define NUM_CHUNKS_TO_RUN 16

		// generate minichunks
		vector<ivec2> to_generate;
		for (int i = 0; i < NUM_CHUNKS_TO_RUN; i++) {
			to_generate.push_back({ 0, i });
		}
		world.gen_chunks(to_generate);

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

		// fill input buffers
		for (int i = 0; i < NUM_CHUNKS_TO_RUN; i++) {
			Chunk *chunk = world.get_chunk(0, i);
			assert(chunk != nullptr);

			for (int j = 0; j < 16; j++) {
				auto &mini = chunk->minis[j];

				unsigned data[MINICHUNK_SIZE];
				for (int k = 0; k < MINICHUNK_SIZE; k++) {
					data[k] = (uint8_t)mini.data[k];
				}

				// load in mini
				glNamedBufferSubData(glInfo->gen_layer_mini_buf, (i * 16 + j) * MINICHUNK_SIZE * sizeof(unsigned), MINICHUNK_SIZE * sizeof(unsigned), data);
			}
		}

		// initialize layers buf with 0
		// TODO: REMOVE THIS ONCE WE CAN FILL IN BUF WITH GEN_LAYER()
		auto start_init_layers = std::chrono::high_resolution_clock::now();

		//// [NVIDIA] Trick GPU into NOT "copying/moving from VIDEO memory to DMA CACHED memory" when we use glClearNamedBufferSubData.
		//// [AMD] TODO: make sure you get similar results.
		//unsigned zero = 0;
		//glNamedBufferSubData(glInfo->gen_layer_layers_buf, 0, 1, &zero);

		//// actually initialize
		//glClearNamedBufferSubData(glInfo->gen_layer_layers_buf, GL_RGBA32UI, 0, 16 * 16 * 96 * sizeof(unsigned) * (NUM_CHUNKS_TO_RUN * 16), GL_RGBA_INTEGER, GL_UNSIGNED_INT, NULL);

		auto finish_init_layers = std::chrono::high_resolution_clock::now();
		long result_init_layers = std::chrono::duration_cast<std::chrono::microseconds>(finish_init_layers - start_init_layers).count();

		auto start_compute_1 = std::chrono::high_resolution_clock::now();

		// run program
		glDispatchCompute(
			NUM_CHUNKS_TO_RUN * 16,
			6, // 6 faces
			15 // 15 layers per face
		);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		auto finish_compute_1 = std::chrono::high_resolution_clock::now();
		long result_compute_1 = std::chrono::duration_cast<std::chrono::microseconds>(finish_compute_1 - start_compute_1).count();

		// TODO: REMOVE
		Block tmp[16][16];
		char *bufp = buf;
		auto start_manual_1 = std::chrono::high_resolution_clock::now();

		// manually generate all layers for all minis
		// for each chunk
		for (int i = 0; i < NUM_CHUNKS_TO_RUN; i++) {
			Chunk *chunk = world.get_chunk(0, i);
			assert(chunk != nullptr);

			// for each mini
			for (int j = 0; j < 16; j++) {
				auto &mini = chunk->minis[j];

				// for each face
				for (int k = 0; k < 6; k++) {
					int face_idx = k % 3;
					int backface = k < 3;
					ivec3 face = { 0, 0, 0 };
					face[face_idx] = backface ? -1 : 1;

					// generate 16 layers
					for (int layer_no = 0; layer_no < 16; layer_no++) {
						// generate layer
						world.gen_layer(&mini, face_idx, layer_no, face, tmp);

						// just to force it to optimize-out tmp
						bufp += sprintf(bufp, "%d", tmp[7][14]);
						if (abs(buf - bufp) > 200) {
							bufp = buf;
						}

						// generate quads for the layer
						vector<Quad2D> quads2d = World::gen_quads(tmp);
					}
				}
			}
		}
		// just to force it to optimize-out tmp
		OutputDebugString(buf);

		auto finish_manual_1 = std::chrono::high_resolution_clock::now();
		long result_manual_1 = std::chrono::duration_cast<std::chrono::microseconds>(finish_manual_1 - start_manual_1).count();


		//// read and print results
		//for (int i = 0; i < 8; i++) {
		//	for (int j = 0; j < 16; j++) {
		//		unsigned output[16 * 16 * 96]; // 96 16x16 layers
		//		glGetNamedBufferSubData(glInfo->gen_layer_layers_buf, 16 * 16 * 96 * sizeof(unsigned) * (i*16 + j), 16 * 16 * 96 * sizeof(unsigned), output);
		//		print_nonzero_cs_output_layers(output);
		//	}
		//}

		// read and print results for chunk[0]->minis[4] ((0, 64, 0))
		unsigned output_0_64_0[16 * 16 * 96]; // 96 16x16 layers
		glGetNamedBufferSubData(glInfo->gen_layer_layers_buf, 16 * 16 * 96 * sizeof(unsigned) * (0 * 16 + 4), 16 * 16 * 96 * sizeof(unsigned), output_0_64_0);
		print_nonzero_cs_output_layers(output_0_64_0);

		auto start_bigread = std::chrono::high_resolution_clock::now();

		unsigned *all_layers = new unsigned[16 * 16 * 96 * (NUM_CHUNKS_TO_RUN * 16)];
		glGetNamedBufferSubData(glInfo->gen_layer_layers_buf, 0, 16 * 16 * 96 * sizeof(unsigned) * (NUM_CHUNKS_TO_RUN * 16), all_layers);

		auto finish_bigread = std::chrono::high_resolution_clock::now();
		long result_bigread = std::chrono::duration_cast<std::chrono::microseconds>(finish_bigread - start_bigread).count();

		// now compare layers for minichunk at (0, 64, 0)
		int mini_idx_0_64_0 = 4;
		Chunk *chunk_0_0 = world.get_chunk(0, mini_idx_0_64_0 / 16);
		MiniChunk &mini_0_64_0 = chunk_0_0->minis[mini_idx_0_64_0 % 16];
		assert(mini_0_64_0.coords == ivec3(0, 64, 0));
		compare_fast_layers(all_layers, &mini_0_64_0, mini_idx_0_64_0);

		// now fill in missing layers for all minis and load them back in
		auto start_fill_missed_layers = std::chrono::high_resolution_clock::now();

		// fill
		for (int i = 0; i < NUM_CHUNKS_TO_RUN; i++) {
			Chunk *chunk = world.get_chunk(0, i);
			assert(chunk != nullptr);

			for (int j = 0; j < 16; j++) {
				auto &mini = chunk->minis[j];

				// fill missing layers for this mini
				world.fill_missed_layers(all_layers, &mini, i * 16 + j);
			}
		}

		// load
		glNamedBufferSubData(glInfo->gen_layer_layers_buf, 0, 16 * 16 * 96 * sizeof(unsigned) * (NUM_CHUNKS_TO_RUN * 16), all_layers);


		auto finish_fill_missed_layers = std::chrono::high_resolution_clock::now();
		long result_fill_missed_layers = std::chrono::duration_cast<std::chrono::microseconds>(finish_fill_missed_layers - start_fill_missed_layers).count();

		// now run again and compare ALL layers (for mini (0, 64, 0))
		compare_all_layers(all_layers, &world, &mini_0_64_0, mini_idx_0_64_0);

		// now generate and read quads
		auto start_quads = std::chrono::high_resolution_clock::now();

		// - set quads counter to 0
		glUseProgram(glInfo->gen_quads_program);
		GLuint num_quads = 0;
		glNamedBufferSubData(glInfo->gen_quads_atomic_buf, 0, sizeof(GLuint), &num_quads);

		glDispatchCompute(
			96 * (NUM_CHUNKS_TO_RUN * 16),
			1,
			1
		);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);

		auto finish_quads = std::chrono::high_resolution_clock::now();
		long result_quads = std::chrono::duration_cast<std::chrono::microseconds>(finish_quads - start_quads).count();

		// get number of quads
		glGetNamedBufferSubData(glInfo->gen_quads_atomic_buf, 0, sizeof(GLuint), &num_quads);
		sprintf(buf, "num generated quads: %u\n", num_quads);
		OutputDebugString(buf);

		// read back quad2ds
		Quad2DCS *quad2ds = new Quad2DCS[num_quads];
		glGetNamedBufferSubData(glInfo->gen_quads_quads2d_buf, 0, num_quads * sizeof(Quad2DCS), quad2ds);

		// read back quad3ds
		Quad3DCS *quad3ds = new Quad3DCS[num_quads];
		glGetNamedBufferSubData(glInfo->gen_quads_quads3d_buf, 0, num_quads * sizeof(Quad3DCS), quad3ds);

		// extract piece of quad2ds
		Quad2DCS piece2d[128];
		for (int i = 0; i < 128; i++) {
			piece2d[i].block = 0;
		}
		for (int i = 0; (i < 128) && (i < num_quads); i++) {
			piece2d[i] = quad2ds[i];
		}

		// extract piece of quad3ds
		Quad3DCS piece3d[128];
		memset(piece3d, 0, 128 * sizeof(Quad3DCS));
		for (int i = 0; (i < 128) && (i < num_quads); i++) {
			piece3d[i] = quad3ds[i];
		}

		// check highest 2D result
		Quad2DCS highest_results;
		for (int i = 0; i < num_quads; i++) {
			if (quad2ds[i].block > highest_results.block) {
				highest_results.block = quad2ds[i].block;
			}
			if (quad2ds[i].corners[0][0] > highest_results.corners[0][0]) {
				highest_results.corners[0][0] = quad2ds[i].corners[0][0];
			}
			if (quad2ds[i].corners[0][1] > highest_results.corners[0][1]) {
				highest_results.corners[0][1] = quad2ds[i].corners[0][1];
			}
			if (quad2ds[i].corners[1][0] > highest_results.corners[1][0]) {
				highest_results.corners[1][0] = quad2ds[i].corners[1][0];
			}
			if (quad2ds[i].corners[1][1] > highest_results.corners[1][1]) {
				highest_results.corners[1][1] = quad2ds[i].corners[1][1];
			}
			if (quad2ds[i].layer_idx > highest_results.layer_idx) {
				highest_results.layer_idx = quad2ds[i].layer_idx;
			}
		}

		// now create ALL QUADS for our minichunks, naturally.
		// OH MY GOD IT'S THE SAME SIZE WOOO!
		vector<Quad3D> expected_quads;
		for (int i = 0; i < NUM_CHUNKS_TO_RUN; i++) {
			Chunk *chunk = world.get_chunk(0, i);
			assert(chunk != nullptr);

			for (int j = 0; j < 16; j++) {
				auto &mini = chunk->minis[j];
				auto mesh = world.gen_minichunk_mesh(&mini);

				for (auto &quad : mesh->quads3d) {
					expected_quads.push_back(quad);
				}
			}
		}

		// print time results
		sprintf(buf, "\ninit layers time: %.2f ms\ngen_layers time: %.2f ms\ngen_quads time: %.2f ms\nmanual time: %.2f ms\nbigread time: %.2f ms\nfill missed layers: %.2fms\n\n", result_init_layers / 1000.0f, result_compute_1 / 1000.0f, result_quads / 1000.0f, result_manual_1 / 1000.0f, result_bigread / 1000.0f, result_fill_missed_layers / 1000.0f);
		OutputDebugString(buf);
		OutputDebugString("");

#undef NUM_CHUNKS_TO_RUN

		/*
		STATS (Debug) (16 chunks / 256 minis):
			num generated quads: 2249
			init layers time: 13.52 ms
			gen_layers time: 0.81 ms
			gen_quads time: 1.90 ms
			manual time: 326.33 ms
			bigread time: 10.31 ms

			Explanation:
				- Compute shaders are 20x faster than CPU for generating quads, less so for generating layers.

		STATS (Release) (16 chunks / 256 minis):
			num generated quads: 2249
			init layers time: 12.58 ms
			gen_layers time: 0.81 ms
			gen_quads time: 2.06 ms
			manual time: 42.50 ms
			bigread time: 9.29 ms

			Explanation:
				- Compute shaders are ONLY 3x faster than CPU for generating quads, less so for generating layers.

				WHY?
					- We're running on invisible minis too. (Could just cut those out if we wanted.)
					- We're spending most of our time initializing layers (without initialization it would be 15x faster!)
						- We won't have to initialize layers once we fill in missing layers with gen_layer()!
		*/

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
	}

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

			// if not backface (i.e. not facing (0,0,0)), move 1 forwards
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

