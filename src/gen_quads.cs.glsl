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
// NOTE: mini_idx = layer_idx / 96.
struct Quad2D {
	uint block_type;
	ivec2 coords[2];
	uint layer_idx;
};

// layers input
layout(std430, binding=1) buffer LAYERS { 
	uint layers[]; 
};

// quads output
layout(std430, binding=2) buffer QUADS { 
	Quad2D global_quads[]; 
};

layout(binding=0, offset=0) uniform atomic_uint global_num_quads;

// TODO: remove
void assert(bool test){}

// get block at these coordinates
// NOTE: Should iterate on x then z then y for best efficiency.
uint get_block(const uint layer_idx, const uint u, const uint v) {
	return layers[u + v * 16 + layer_idx * 16 * 16];
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
ivec2 get_max_size(const uint layer_idx, inout bool merged[16][16], const ivec2 start_point, const uint block_type) {
	assert(block_type != BLOCK_AIR);
	assert(!merged[start_point[0]][start_point[1]]);

	// TODO: Start max size at {1,1}, and for loops at +1.
	// TODO: Search width with find() instead of a for loop.

	// "max width and height"
	ivec2 max_size = ivec2(0, 0);

	// maximize width
	for (int i = start_point[0], j = start_point[1]; i < 16; i++) {
		// if extended by 1, add 1 to max width
		if (get_block(layer_idx, i, j) == block_type && !merged[i][j]) {
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
			if (get_block(layer_idx, i, j) != block_type || merged[i][j]) {
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
	uint layer_idx = gl_WorkGroupID.x;

	// create quad merged array
	// TODO: figure out if this is any faster than initializing with a for loop.
	bool merged[16][16] = {{false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}};

	// we'll generate at most one quad per square => 256 quads per layer
	Quad2D quads[MAX_LAYER_QUADS];
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
			uint block = get_block(layer_idx, u, v);

			// skip merged & air blocks
			if (merged[u][v] || block == BLOCK_AIR) continue;

			// start block of this quad
			ivec2 start = ivec2(u, v);

			// get max size of this quad
			ivec2 max_size = get_max_size(layer_idx, merged, start, block);

			// add it to results
			quads[num_quads].block_type = block;
			quads[num_quads].coords[0] = start;
			quads[num_quads].coords[1] = start + max_size;
			quads[num_quads].layer_idx = layer_idx;
			num_quads++;

			// mark all as merged
			mark_as_merged(merged, start, max_size);
		}
	}

	// write quads to result!

	// allocate space for us in results
	uint result_quad_idx = atomicCounterAdd(global_num_quads, num_quads);

	// store quads
	for (uint i = 0; i < num_quads; i++) {
		global_quads[result_quad_idx + i] = quads[i];
	}

	

	// RESULT SIZE: Should fit at least (MAX_LAYER_QUADS * (16+1) * 3) quads. (NOT (MAX_LAYER_QUADS * 16 * 6) QUADS!)
}
