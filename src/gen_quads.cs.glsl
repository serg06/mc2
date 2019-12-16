#version 460 core

#define BLOCK_AIR 0
#define BLOCK_STONE 1
#define BLOCK_GRASS 2
#define BLOCK_WATER 9
#define BLOCK_OAKLOG 17
#define BLOCK_OAKLEAVES 18
#define BLOCK_OUTLINE 100

#define MAX_LAYER_QUADS (16*16)
#define MAX_MINI_QUADS (MAX_LAYER_QUADS * (16+1) * 3)

// TODO: Remove overall, since local_size_x is size 1 by default.
layout( local_size_x = 1 ) in; 

// 6 ints... jesus that's a LOTTA space.
// NOTE: mini_idx = total_mini_layer_idx / 96.
// MUST MATCH STRUCT IN world.h
// SIZE: 24 bytes (6 ints)
struct Quad2D {
	ivec2 coords[2];			// size = 16 | alignment = 8 (size of ivec2)
	uint block_type;			// size =  4 | alignment = 4 (size of uint)
	uint total_mini_layer_idx;	// size =  4 | alignment = 4
};

//// MUST MATCH STRUCT IN world.h
//// SIZE: 32 bytes (8 ints)
//struct Quad3D {
//	uint coords[2][3];	// size = 24 | alignment = 4 (NOTE: Cannot make this 2 ivec3s, or else alignment would be 12 bytes, which is fucking GROSS.)
//	uint block_type;	// size =  4 | alignment = 4
//	uint empty;
//};

// MUST MATCH STRUCT IN world.h
// SIZE: 48 bytes (12 ints)
// TODO: Use the other Quad3D (Can be just 32 bytes, 2/3 of this size.)
struct Quad3D {
	uvec4 coords[2];		// size = 32 | alignment = 16
	uint block_type;		// size =  4 | alignment =  4
	uint global_face_idx;
	uint empty1;
	uint empty2;
};


// layers input
layout(std430, binding=1) buffer LAYERS { 
	uint layers[]; 
};

// 2D quads output
layout(std430, binding=2) buffer QUAD2DS { 
	Quad2D global_quad2ds[]; 
};

// 3D quads output
layout(std430, binding=3) buffer QUAD3DS { 
	Quad3D global_quad3ds[]; 
};

layout(binding=0, offset=0) uniform atomic_uint global_num_quads;

// TODO: remove
void assert(bool test){}

// get block at these coordinates
// NOTE: Should iterate on x then z then y for best efficiency.
uint get_block(const uint total_mini_layer_idx, const uint u, const uint v) {
	return layers[u + v * 16 + total_mini_layer_idx * 16 * 16];
}

// mark elements as merged
void mark_as_merged(inout bool merged[16][16], const ivec2 start, const ivec2 max_size) {
	for (int i = start[0]; i < start[0] + max_size[0]; i++) {
		for (int j = start[1]; j < start[1] + max_size[1]; j++) {
			merged[i][j] = true;
		}
	}
}

// given a layer and start point, find its best dimensions
ivec2 get_max_size(const uint total_mini_layer_idx, inout bool merged[16][16], const ivec2 start_point, const uint block_type) {
	assert(block_type != BLOCK_AIR);
	assert(!merged[start_point[0]][start_point[1]]);

	// TODO: Start max size at {1,1}, and for loops at +1.
	// TODO: Search width with find() instead of a for loop.

	// "max width and height"
	ivec2 max_size = ivec2(0, 0);

	// maximize width
	for (int i = start_point[0], j = start_point[1]; i < 16; i++) {
		// if extended by 1, add 1 to max width
		if (!merged[i][j] && get_block(total_mini_layer_idx, i, j) == block_type) {
			max_size[0]++;
		}
		// else give up
		else {
			break;
		}
	}

	assert(max_size[0] > 0);

	// now that we've maximized width, need to
	// maximize height

	// for each height
	bool stop = false;
	for (int j = start_point[1]; j < 16; j++) {
		// check if entire width is correct
		for (int i = start_point[0]; i < start_point[0] + max_size[0]; i++) {
			// if wrong block type, give up on extending height
			if (merged[i][j] || get_block(total_mini_layer_idx, i, j) != block_type) {
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

	assert(max_size[1] > 0);

	return max_size;
}

void main() {
	// index of the layer we've been assigned
	uint total_mini_layer_idx = gl_WorkGroupID.x;

	// generate variables (assuming we have minis as inputs (i.e. 96 layers at a time), instead of just one layer at a time)
	uint global_face_idx = int((total_mini_layer_idx % 96) / 16);
	uint local_face_idx = global_face_idx % 3;
	bool backface = global_face_idx < 3;
	uint layer_idx = total_mini_layer_idx % 16;
	uint working_idx_1 = local_face_idx == 0 ? 1 : 0;
	uint working_idx_2 = local_face_idx == 2 ? 1 : 2;
	ivec3 face = {0, 0, 0};
	face[local_face_idx] = backface ? -1 : 1;

	// create quad merged array
	// TODO: figure out if this is any faster than initializing with a for loop.
	bool merged[16][16] = {{false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}};

	// we'll generate at most one quad per square => 256 quads per layer
	Quad2D quad2ds[MAX_LAYER_QUADS];
	Quad3D quad3ds[MAX_LAYER_QUADS];
	int num_quads = 0;

	// generate quads
	// I think this is gonna be super slow because of branching...
	// IDEA: Make ALL THREADS find next start of quad
	// -- If no next quad, EXIT THREAD
	// -- If exists, then ALL THREADS run get_max_size / mark_as_merged / add to results.
	// -- ALL THREADS continue searching from their last u/v / start-block.
	for (int u = 0; u < 16; u++) {
		for (int v = 0; v < 16; v++) {
			// get block
			uint block = get_block(total_mini_layer_idx, u, v);

			// skip merged & air blocks
			if (merged[u][v] || block == BLOCK_AIR) continue;

			// start block of this quad
			ivec2 start = ivec2(u, v);

			// get max size of this quad
			ivec2 max_size = get_max_size(total_mini_layer_idx, merged, start, block);

			// save it as corners
			ivec2 start_and_end[2] = {start, start + max_size};

			// if -x, -y, or +z, flip triangles around so that we're not drawing them backwards
			// TODO: Run this in a loop at the end?
			if (face[0] < 0 || face[1] < 0 || face[2] > 0) {
				ivec2 diffs = start_and_end[1] - start_and_end[0];
				start_and_end[0][0] += diffs[0];
				start_and_end[1][0] -= diffs[0];
			}

			// add it to 2D results
			quad2ds[num_quads].block_type = block;
			quad2ds[num_quads].coords[0] = start;
			quad2ds[num_quads].coords[1] = start + max_size;
			quad2ds[num_quads].total_mini_layer_idx = total_mini_layer_idx;

			// add it to 3D results
			quad3ds[num_quads].block_type = block;
			quad3ds[num_quads].global_face_idx = global_face_idx; // from this can get `uint local_face_idx = global_face_idx % 3;` and `bool backface = global_face_idx < 3;`
			quad3ds[num_quads].empty1 = 12345;
			quad3ds[num_quads].empty2 = 34567;
			for (int i = 0; i < 2; i++) {
				// if NOT backface (i.e. if facing AWAY from cube origin), need to add 1 to layer idx
				// TODO: remove branch from for loop (2 branches -> 1)
				quad3ds[num_quads].coords[i][local_face_idx] = backface ? layer_idx : layer_idx+1;
				quad3ds[num_quads].coords[i][working_idx_1] = start_and_end[i][0];
				quad3ds[num_quads].coords[i][working_idx_2] = start_and_end[i][1];
			}

			// mark all as merged
			mark_as_merged(merged, start, max_size);

			num_quads++;
		}
	}

	// write quads to result!

	// allocate space for us in results
	uint result_quad_idx = atomicCounterAdd(global_num_quads, num_quads);

	// store quads
	for (uint i = 0; i < num_quads; i++) {
		global_quad2ds[result_quad_idx + i] = quad2ds[i];
		global_quad3ds[result_quad_idx + i] = quad3ds[i];
	}

	// RESULT SIZE: Should fit at least (MAX_LAYER_QUADS * (16+1) * 3) quads. (NOT (MAX_LAYER_QUADS * 16 * 6) QUADS!)
}
