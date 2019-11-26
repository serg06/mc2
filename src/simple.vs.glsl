#version 450 core

#define CHUNK_WIDTH 16
#define CHUNK_DEPTH 16
#define CHUNK_HEIGHT 256
#define CHUNK_SIZE (CHUNK_WIDTH * CHUNK_DEPTH * CHUNK_HEIGHT)

layout (location = 0) in vec4 position;
layout (location = 1) in uint block_type; // fed in via instance array!

out vec4 vs_color;

layout (std140, binding = 0) uniform UNI_IN
{
    mat4 mv_matrix; // takes up 16 bytes
    mat4 proj_matrix; // takes up 16 bytes
} uni;


vec4 get_color() {
	if (gl_VertexID < 6) {
		vs_color = vec4(1.0, 0.0, 0.0, 1.0);
	}
	else if (gl_VertexID < 12) {
		vs_color = vec4(0.0, 1.0, 0.0, 1.0);
	}
	else if (gl_VertexID < 18) {
		vs_color = vec4(0.0, 0.0, 1.0, 1.0);
	}
	else if (gl_VertexID < 24) {
		vs_color = vec4(0.5, 0.5, 0.0, 1.0);
	}
	else if (gl_VertexID < 30) {
		vs_color = vec4(0.0, 0.5, 0.5, 1.0);
	}
	else if (gl_VertexID < 36) {
		vs_color = vec4(0.5, 0.0, 0.5, 1.0);
	}

	return vs_color;
}

float rand(float seed) {
	return fract(1.610612741 * seed);
}

// shader starts executing here
void main(void)
{
	// TODO: Add chunk offset

	/* EXTRACT X, Y, Z */

	// int chunk_idx = x + z * CHUNK_WIDTH + y * CHUNK_WIDTH * CHUNK_DEPTH;
	// have gl_InstanceID
	int remainder = gl_InstanceID;
	// -> gl_InstanceID / CHUNK_HEIGHT = height index
	int y = remainder / CHUNK_HEIGHT;
	// ---> subtract y * CHUNK_WIDTH * CHUNK_DEPTH to get x+z*CHUNK_WIDTH
	remainder -= y * CHUNK_WIDTH * CHUNK_DEPTH;
	// -> remainder / CHUNK_DEPTH = depth index
	int z = remainder / CHUNK_DEPTH;
	remainder -= z * CHUNK_WIDTH;
	int x = remainder;

	/* CREATE OUR OFFSET VARIABLE */

	vec4 instance_offset = vec4(x, y, z, 0);

	/* ADD IT TO VERTEX */

	//gl_Position = uni.proj_matrix * uni.mv_matrix * position;
	gl_Position = uni.proj_matrix * uni.mv_matrix * (position + instance_offset);
	vs_color = position * 2.0 + vec4(0.5, 0.5, 0.5, 1.0);

	int seed = gl_VertexID * gl_InstanceID;
	switch(block_type) {
		case 0: // air
			vs_color = vec4(0.0, 0.0, 0.0, 0.0); // invisible
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



}
