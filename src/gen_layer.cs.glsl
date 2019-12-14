#version 450 core

#define BLOCK_AIR 0
#define BLOCK_STONE 1
#define BLOCK_GRASS 2
#define BLOCK_WATER 9
#define BLOCK_OAKLOG 17
#define BLOCK_OAKLEAVES 18
#define BLOCK_OUTLINE 100

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

layout( local_size_x = 16, local_size_y = 16 ) in; 

// minichunk input (TODO: support multiple minichunks)
layout(std430, binding=0) buffer MINI { 
	uint mini[]; 
};

// minichunk output (TODO: support multiple minichunks)
layout(std430, binding=1) buffer LAYERS { 
	uint layers[]; 
};

// get block at these coordinates
// NOTE: Should iterate on x then z then y for best efficiency.
uint get_block(const uint x, const uint y, const uint z, const uint chunk_idx) {
	return mini[x + z * 16 + y * 16 * 16 + chunk_idx * 16 * 16 * 16];
}

//uint get_block(const uvec3 xyz) { return get_block(xyz[0], xyz[1], xyz[2]); }
uint get_block(const uvec3 xyz, const uint chunk_idx) {
	return mini[xyz[0] + xyz[1] * 256 + xyz[2] * 16 + chunk_idx * 16 * 16 * 16];
}

bool is_transparent(uint block) { return block == BLOCK_AIR; }
bool is_translucent(uint block) { return translucent_blocks[block]; }

// TODO: Incorporate backface.
void main() { 
	// index of the chunk we're working on
	uint chunk_idx = gl_WorkGroupID.x;

	// index of the face we're working on
	uint global_face_idx = gl_WorkGroupID.y;
	uint local_face_idx = global_face_idx % 3;
	bool backface = global_face_idx < 3;
	ivec3 face = ivec3(0, 0, 0);
	face[local_face_idx] = backface ? -1 : 1;

	// working indices are always gonna be xy, xz, or yz.
	int working_idx_1 = local_face_idx == 0 ? 1 : 0;
	int working_idx_2 = local_face_idx == 2 ? 1 : 2;

	// index of the layer we're working on, relative to face
	// NOTE: this goes from 0 to 14 (inclusive) because 1 layer in each direction cannot see its face (not in mini)
	uint layer_idx = gl_WorkGroupID.z;

	// index of element in current layer
	uint u = gl_LocalInvocationID.x;
	uint v = gl_LocalInvocationID.y;

	// just plug in my usual gen_layer algorithm!
	// get coords of block we want
	uvec3 coords;
	coords[local_face_idx] = layer_idx;
	coords[working_idx_1] = u;
	coords[working_idx_2] = v;

	// IF FACE IS NEGATIVE, SUBTRACT IT, CUZ BEHIND FIRST LAYER IS NOTHING.
	// TODO: can be checked by just doing if(backface)
	if (face[0] < 0 || face[1] < 0 || face[2] < 0) {
		coords -= face;
	}

	// get block and face block for these coords
	uint block = get_block(coords, chunk_idx);
	uint face_block = get_block(coords + face, chunk_idx);

	// result we're gonna write
	uint result_block = BLOCK_AIR;

	// if block's face is visible, keep it in layer
	if (block != BLOCK_AIR && (is_transparent(face_block) || (block != BLOCK_WATER && is_translucent(face_block)) || (is_translucent(face_block) && !is_translucent(block)))) {
		result_block = block;
	}

	// done!
	layers[u + v * 16 + layer_idx * 16 * 16 + global_face_idx * 15 * 16 * 16 + chunk_idx * 6 * 15 * 16 * 16] = result_block;
}
