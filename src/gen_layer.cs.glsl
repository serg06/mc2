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

void go(const uint face_idx, const uint layer_idx, const ivec3 face, const uint u, const uint v);

// TODO: Incorporate backface.
void main() { 
	// index of the chunk we're working on
	uint chunk_idx = gl_WorkGroupID.x;

	// index of the face we're working on
	uint face_idx = gl_WorkGroupID.y % 3;
	bool backface = face_idx < 3;
	ivec3 face = ivec3(0, 0, 0);
	face[face_idx] = backface ? -1 : 1;

	// index of the layer we're working on, relative to face
	// NOTE: this goes from 0 to 14 (inclusive) because 1 layer in each direction cannot see its face (not in mini)
	uint layer_idx = gl_WorkGroupID.z;

	// index of element in current layer
	uint layer_x = gl_LocalInvocationID.x;
	uint layer_y = gl_LocalInvocationID.y;

	// just plug in my usual gen_layer algorithm!
	go(face_idx, layer_idx, face, layer_x, layer_y);
}

// get block at these coordinates
// NOTE: Should iterate on x then z then y for best efficiency.
uint get_block(const uint x, const uint y, const uint z) {
	return mini[x + z * 16 + y * 16 * 16];
}

uint get_block(const uvec3 xyz) { return get_block(xyz[0], xyz[1], xyz[2]); }

// set block at these coordinates
// NOTE: Should iterate on u then v then layer_idx then face_idx, for best efficiency.
void set_block(const uint u, const uint v, const uint face_idx, const uint layer_idx, const uint val) {
	uint result_layer_idx = face_idx * 15 + layer_idx;
	layers[u + v * 16 + result_layer_idx * 16 * 16] = val;
}

bool is_transparent(uint block) { return block == BLOCK_AIR; }
bool is_translucent(uint block) { return translucent_blocks[block]; }

void go(const uint face_idx, const uint layer_idx, const ivec3 face, const uint u, const uint v) {
	// working indices are always gonna be xy, xz, or yz.
	int working_idx_1 = face_idx == 0 ? 1 : 0;
	int working_idx_2 = face_idx == 2 ? 1 : 2;

	// get coords of block we want
	uvec3 coords;
	coords[face_idx] = layer_idx;
	coords[working_idx_1] = u;
	coords[working_idx_2] = v;

	// IF FACE IS NEGATIVE, SUBTRACT IT, CUZ BEHIND FIRST LAYER IS NOTHING.
	// TODO: can be checked by just doing if(backface)
	if (face[0] < 0 || face[1] < 0 || face[2] < 0) {
		coords -= face;
	}

	// get block and face block for these coords
	uint block = get_block(coords);
	uint face_block = get_block(coords + face);

	// result we're gonna write
	uint result_block = BLOCK_AIR;

	// if block's face is visible, keep it in layer
	if (block != BLOCK_AIR && (is_transparent(face_block) || (block != BLOCK_WATER && is_translucent(face_block)) || (is_translucent(face_block) && !is_translucent(block)))) {
		result_block = block;
	}

	// done!
	set_block(u, v, face_idx, layer_idx, result_block);
}

void assert(bool test){
	// TODO: Write error to output error buffer.
}
