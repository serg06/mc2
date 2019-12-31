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
layout (location = 5) in ivec3 q_face;
layout (location = 6) in ivec3 q_base_coords;
layout (location = 7) in uint q_lighting;
layout (location = 8) in uint q_metadata;

//out vec2 vs_tex_coords; // texture coords in [0.0, 1.0]
out uint vs_block_type;
out ivec3 vs_face;
out ivec3 vs_corner1;
out ivec3 vs_corner2;
out ivec3 vs_base_coords;
out uint vs_lighting;
out uint vs_metadata;

layout (std140, binding = 0) uniform UNI_IN
{
	// member			base alignment			base offset		size	aligned offset	extra info
	mat4 mv_matrix;		// 16 (same as vec4)	0				64		16 				(mv_matrix[0])
						//						16						32				(mv_matrix[1])
						//						32						48				(mv_matrix[2])
						//						48						64				(mv_matrix[3])
	mat4 proj_matrix;	// 16 (same as vec4)	64				64		80				(proj_matrix[0])
						//						80						96				(proj_matrix[1])
						//						96						112				(proj_matrix[2])
						//						112						128				(proj_matrix[3])
	uint in_water;      // 4                    128             4       132
} uni;

float rand(float seed) {
	return fract(1.610612741 * seed);
}

void main(void)
{
	// data passthrough
	vs_block_type = q_block_type;
	vs_face = q_face;
	vs_corner1 = q_corner1;
	vs_corner2 = q_corner2;
	vs_base_coords = q_base_coords;
	vs_lighting = q_lighting;
	vs_metadata = q_metadata;
}
