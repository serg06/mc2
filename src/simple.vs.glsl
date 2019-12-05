#version 450 core

#define CHUNK_WIDTH 16
#define CHUNK_DEPTH 16
#define CHUNK_HEIGHT 256
#define CHUNK_SIZE (CHUNK_WIDTH * CHUNK_DEPTH * CHUNK_HEIGHT)

layout (location = 0) in vec4 position;
layout (location = 1) in uint block_type; // fed in via instance array!

// Quad input
layout (location = 2) in uint q_block_type;
layout (location = 3) in ivec3 q_corner1;
layout (location = 4) in ivec3 q_corner2;

out vec4 vs_color;
out flat uint vs_block_type;
out vec2 vs_tex_coords; // texture coords in [0.0, 1.0]
out flat uint horizontal;

layout (std140, binding = 0) uniform UNI_IN
{
	// member			base alignment			base offset		size	aligned offset	extra info
	mat4 mv_matrix;		// 16 (same as vec4)	0				64		0 				(mv_matrix[0])
						//						0						16				(mv_matrix[1])
						//						0						32				(mv_matrix[2])
						//						0						48				(mv_matrix[3])
	mat4 proj_matrix;	// 16 (same as vec4)	64				64		64				(proj_matrix[0])
						//						80						80				(proj_matrix[1])
						//						96						96				(proj_matrix[2])
						//						112						112				(proj_matrix[3])
	ivec3 base_coords;	// 16 (same as vec4)	128				12		128
} uni;

float rand(float seed) {
	return fract(1.610612741 * seed);
}

void main(void)
{
	/* GENERATE CORNER BASED ON VERTEX ID & OUR 2 OPPOSITE CORNERS */

	vec4 offset_in_chunk = vec4(q_corner1, 0);
	ivec3 diffs = q_corner2 - q_corner1;

	// figure out which index stays the same and which indices change
	int zero_idx = diffs[0] == 0 ? 0 : diffs[1] == 0 ? 1 : 2;
	int working_idx_1 = zero_idx == 0 ? 1 : 0;
	int working_idx_2 = zero_idx == 2 ? 1 : 2;

	horizontal = zero_idx == 1 ? 1 : 0;

	// generate corner based on vertex ID
	switch(gl_VertexID) {
	case 0:
		// top-left corner
		vs_tex_coords = vec2(0, 0);
		break;
	case 1:
	case 4:
		// bottom-left corner
		vs_tex_coords = vec2(diffs[working_idx_1], 0);
		// vs_tex_coords = vec2(1, 0);
		offset_in_chunk[working_idx_1] += diffs[working_idx_1];
		break;
	case 2:
	case 3:
		// top-right corner
		vs_tex_coords = vec2(0, diffs[working_idx_2]);
		// vs_tex_coords = vec2(0, 1);
		offset_in_chunk[working_idx_2] += diffs[working_idx_2];
		break;
	case 5:
		// bottom-right corner
		vs_tex_coords = vec2(diffs[working_idx_1], diffs[working_idx_2]);
		// vs_tex_coords = vec2(1, 1);
		offset_in_chunk = vec4(q_corner2, 0);
		break;
	}

	/* CREATE OUR OFFSET VARIABLE */

	vec4 chunk_base = vec4(uni.base_coords.x * 16, uni.base_coords.y, uni.base_coords.z * 16, 0);
	vec4 instance_offset = chunk_base + offset_in_chunk;

	/* ADD IT TO VERTEX */

	gl_Position = uni.proj_matrix * uni.mv_matrix * (position + instance_offset);

	// set color
	int seed = int(instance_offset[0]) ^ int(instance_offset[1]) ^ int(instance_offset[2]);
	switch(q_block_type) {
		case 0: // air (just has a color for debugging purposes)
			vs_color = vec4(0.8 + rand(seed)*0.2, 0.0, 0.0, 1.0); // bright red
			break;
		case 1: // grass
			vs_color = vec4(0.2, 0.8 + rand(seed) * 0.2, 0.0, 1.0); // green
			// vs_color = vec4(0.2, 0.6, 0.0, 1.0); // green
			break;
		case 2: // stone
			vs_color = vec4(0.4, 0.4, 0.4, 1.0) + vec4(rand(seed), rand(seed), rand(seed), rand(seed))*0.2; // grey
			// vs_color = vec4(0.4, 0.4, 0.4, 1.0); // grey
			break;
		case 100: // black outline
			vs_color = vec4(0.0, 0.0, 0.0, 1.0);
			break;
		default:
			vs_color = vec4(1.0, 0.0, 1.0, 1.0); // SUPER NOTICEABLE (for debugging)
			break;
	}

    vs_block_type = q_block_type;
}

// shader starts executing here
void main_cubes(void)
{
	vs_block_type = block_type;

	// TODO: Add chunk offset

	// Given gl_InstanceID, calculate 3D coordinate relative to chunk origin
	int remainder = gl_InstanceID;
	int y = remainder / CHUNK_HEIGHT;
	remainder -= y * CHUNK_WIDTH * CHUNK_DEPTH; // TODO: I think this should be y * CHUNK_HEIGHT
	int z = remainder / CHUNK_DEPTH;
	remainder -= z * CHUNK_WIDTH; // TODO: I think this should be z * CHUNK_DEPTH
	int x = remainder;

	/* CREATE OUR OFFSET VARIABLE */

	vec4 chunk_base = vec4(uni.base_coords.x * 16, uni.base_coords.y, uni.base_coords.z * 16, 0);
	vec4 offset_in_chunk = vec4(x, y, z, 0);
	vec4 instance_offset = chunk_base + offset_in_chunk;

	/* ADD IT TO VERTEX */

	gl_Position = uni.proj_matrix * uni.mv_matrix * (position + instance_offset);
	vs_color = position * 2.0 + vec4(0.5, 0.5, 0.5, 1.0);

	int seed = gl_VertexID * gl_InstanceID;
	switch(block_type) {
		case 0: // air (just has a color for debugging purposes)
			vs_color = vec4(0.7, 0.7, 0.7, 1.0);
			break;
		case 1: // grass
			vs_color = vec4(0.2, 0.8 + rand(seed) * 0.2, 0.0, 1.0); // green
			break;
		case 2: // stone
			vs_color = vec4(0.4, 0.4, 0.4, 1.0) + vec4(rand(seed), rand(seed), rand(seed), rand(seed))*0.2; // grey
			break;
		default:
			vs_color = vec4(1.0, 0.0, 1.0, 1.0); // SUPER NOTICEABLE (for debugging)
			break;
	}

	// if top corner, make it darker!
	if (position.y > 0) {
		vs_color /= 2;
	}
}
