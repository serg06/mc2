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

// 16 x 16 array to quickly look-up if a block is transparent -- I've gotta improve this somehow lmao
const bool translucent_blocks[256] = {
	true,  false, false, false, false, false, false, false, false, true,  false, false, false, false, false, false,
	false, false, true,  false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
};

// MUST MATCH STRUCT IN world.h
// SIZE: 48 bytes (12 ints)
// TODO: Reduce size.
struct Quad3D {
	uvec4 coords[2];		// size = 32 | alignment = 16
	uint block_type;		// size =  4 | alignment =  4
	uint global_face_idx;
	uint mini_input_idx;	// index of mini in input buffer
	uint empty;
};

// minichunk input
layout(std430, binding=0) buffer MINI { 
	uint mini[]; 
};

// layers input
layout(std430, binding=1) buffer LAYERS { 
	uint layers[]; 
};

//// 2D quads output
//layout(std430, binding=2) buffer QUAD2DS { 
//	Quad2D global_quad2ds[]; 
//};

// 3D quads output
layout(std430, binding=3) buffer QUAD3DS { 
	Quad3D global_quad3ds[]; 
};

// mini coords input
// uvec4 just cuz that way I know I won't have any fuckin alignment problems.
// this should start with all the minis we wanna generate for, and end with minis that we don't wanna generate for, but are just used for generating layers.
layout(std430, binding=4) buffer COORDS { 
	ivec4 mini_coords[]; 
};

layout(binding=0, offset=0) uniform atomic_uint global_num_quads;
layout(location=0) uniform uint num_minis; // total number of minis in the minis/coords buffers

// TODO: remove
void assert(bool test){}

// mark elements as merged
void mark_as_merged(inout bool merged[16][16], const ivec2 start, const ivec2 max_size) {
	for (int i = start[0]; i < start[0] + max_size[0]; i++) {
		for (int j = start[1]; j < start[1] + max_size[1]; j++) {
			merged[i][j] = true;
		}
	}
}

// given a layer and start point, find its best dimensions
ivec2 get_max_size(const uint layer[16][16], inout bool merged[16][16], const ivec2 start_point, const uint block_type) {
	assert(block_type != BLOCK_AIR);
	assert(!merged[start_point[0]][start_point[1]]);

	// TODO: Start max size at {1,1}, and for loops at +1.
	// TODO: Search width with find() instead of a for loop.

	// "max width and height"
	ivec2 max_size = ivec2(0, 0);

	// maximize width
	for (int i = start_point[0], j = start_point[1]; i < 16; i++) {
		// if extended by 1, add 1 to max width
		if (!merged[i][j] && layer[i][j] == block_type) {
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
			if (merged[i][j] || layer[i][j] != block_type) {
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

bool is_transparent(uint block) { return block == BLOCK_AIR; }
bool is_translucent(uint block) { return translucent_blocks[block]; }

uint get_block(const uvec3 xyz, const uint chunk_idx) {
	return mini[xyz[0] + xyz[1] * 16 * 16 + xyz[2] * 16 + chunk_idx * 16 * 16 * 16];
}

void main() {
	// index of the layer we've been assigned
	uint total_mini_layer_idx = gl_WorkGroupID.x;

	// our own mini index in mini & coords arrays
	uint own_mini_idx = total_mini_layer_idx / 96;

	// generate misc. variables
	uint global_face_idx = int((total_mini_layer_idx % 96) / 16);
	uint local_face_idx = global_face_idx % 3;
	bool backface = global_face_idx < 3;
	uint layer_idx = total_mini_layer_idx % 16;
	uint working_idx_1 = local_face_idx == 0 ? 1 : 0;
	uint working_idx_2 = local_face_idx == 2 ? 1 : 2;
	ivec3 face = {0, 0, 0};
	ivec4 iface4 = {0, 0, 0, 0};
	face[local_face_idx] = backface ? -1 : 1;
	iface4[local_face_idx] = backface ? -1 : 1;
	uint mini_input_idx = total_mini_layer_idx / 96;

	// todo: get own coords, and top/bottom/etc coords.
	// then put them into an array or two where

	// OK: First figure out which mini coordinates we're gonna use.

	// the index of the mini that we're gonna grab face blocks from
	uint face_mini_idx = own_mini_idx;

	// if edge layer
	if ((layer_idx == 0 && backface) || (layer_idx == 15 && !backface)) {
		// hopefully this causes a crash if we don't find a value for this
		face_mini_idx = -1;

		// get own mini's coords
		ivec4 own_coords = mini_coords[own_mini_idx];

		// figure out which mini coordinates we need
		ivec4 neighbor_coords = own_coords + (local_face_idx == 1 ? iface4 * 16 : iface4);

		// then get our mini
		bool found = false;
		for (int i = 0; i < num_minis; i++) {
			if (mini_coords[i] == neighbor_coords) {
				face_mini_idx = i;
				found = true;
				break;
			}
		}
		// if not found, set error
		if (!found) {
			global_quad3ds[0].empty = 1;
			global_quad3ds[0].coords[0] = own_coords;
			global_quad3ds[0].coords[1] = neighbor_coords;
		}
	}

	// layer we're gonna extract
	uint layer[16][16];

	for (int u = 0; u < 16; u++) {
		for (int v = 0; v < 16; v++) {
			// get coords of block in mini
			uvec3 block_coords;
			block_coords[local_face_idx] = layer_idx;
			block_coords[working_idx_1] = u;
			block_coords[working_idx_2] = v;

			// get coords of face block in its mini
			ivec3 face_coords_tmp = ivec3(block_coords) + face;
			// if out of range of current mini, adjust coords to fix range and put in next mini
			face_coords_tmp[local_face_idx] = ((face_coords_tmp[local_face_idx] % 16) + 16) % 16;
			uvec3 face_coords = uvec3(face_coords_tmp);

			// get block and face block for those coords
			uint block = get_block(block_coords, own_mini_idx);
			uint face_block = get_block(face_coords, face_mini_idx);

			// result we're gonna write
			uint result_block = BLOCK_AIR;

			// if block's face is visible, keep it in layer
			if (block != BLOCK_AIR && (is_transparent(face_block) || (block != BLOCK_WATER && is_translucent(face_block)) || (is_translucent(face_block) && !is_translucent(block)))) {
				result_block = block;
			}

			// write correct block!
			layer[u][v] = result_block;
		}
	}

	// create quad merged array
	// TODO: figure out if this is any faster than initializing with a for loop.
	bool merged[16][16] = {{false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}, {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}};

	// we'll generate at most one quad per square => 256 quads per layer
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
			// TODO: maybe iterate on u first?
			uint block = layer[u][v];

			// skip merged & air blocks
			if (merged[u][v] || block == BLOCK_AIR) continue;

			// start block of this quad
			ivec2 start = ivec2(u, v);

			// get max size of this quad
			ivec2 max_size = get_max_size(layer, merged, start, block);

			// save it as corners
			ivec2 start_and_end[2] = {start, start + max_size};

			// if -x, -y, or +z, flip triangles around so that we're not drawing them backwards
			// TODO: Run this in a loop at the end?
			if (face[0] < 0 || face[1] < 0 || face[2] > 0) {
				ivec2 diffs = start_and_end[1] - start_and_end[0];
				start_and_end[0][0] += diffs[0];
				start_and_end[1][0] -= diffs[0];
			}

			// add it to 3D results
			quad3ds[num_quads].block_type = block;
			quad3ds[num_quads].global_face_idx = global_face_idx; // from this can get `uint local_face_idx = global_face_idx % 3;` and `bool backface = global_face_idx < 3;`
			quad3ds[num_quads].mini_input_idx = mini_input_idx;
			quad3ds[num_quads].empty = 12345;
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
		global_quad3ds[result_quad_idx + i] = quad3ds[i];
	}

	// RESULT SIZE: Should fit at least (MAX_LAYER_QUADS * (16+1) * 3) quads. (NOT (MAX_LAYER_QUADS * 16 * 6) QUADS!)
}
