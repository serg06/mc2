#version 450 core

layout (local_size_x = 64) in;

// 6 frustum planes
layout (location = 0) uniform vec4 planes[6];

// radius
layout (location = 6) uniform float radius;

// huge array of center-of-chunk coordinates
// TODO: Make this vec3, or even ivec3.
layout (std430, binding = 0) buffer COORDS_IN { 
	vec4 coords[];
};

// output
// TODO: Make this bool.
layout (std430, binding = 1) buffer COORDS_OUT { 
	vec4 results[];
};

// shader starts executing here
void main(void)
{
	// I'm only invoking everyone in x direction
	uint idx = gl_GlobalInvocationID.x;

	// Get your coordinate
	vec4 coord = coords[idx];
	coord[3] = 1.0f;

	// pre-prep result (set to true)
	vec4 result = vec4(1, 0, 0, 0);

	// check if possible and set result accordingly
	for (int i = 0; i < 6; i++) {
		if (dot(coord, planes[i]) <= -radius) {
			result[0] = 0;
		}
	}

	results[idx] = result;
}
