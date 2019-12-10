#version 450 core

in vec4 vs_color;
in flat uint vs_block_type;
in vec2 vs_tex_coords; // texture coords in [0.0, 1.0]
in flat uint horizontal; // 0 = quad is vertical, 1 = quad is horizontal (TODO: Remove this once we start inputting quad face to vertex shader.)
in flat ivec3 vs_face;

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
	ivec4 base_coords;	// 16 (same as vec4)	128				12		144
	uint in_water;      // 4                    144             1       148
} uni;

out vec4 color;

// our grass texture!
layout (binding = 0) uniform sampler2D grass_top;
layout (binding = 1) uniform sampler2D grass_side;

// texture arrays
layout (binding = 2) uniform sampler2DArray top_textures;
layout (binding = 3) uniform sampler2DArray side_textures;
layout (binding = 4) uniform sampler2DArray bottom_textures;

float soft_increase(float x) {
	return -1/((x+1)) + 1;
}

void main(void)
{
	// TODO: Remove branching.
	if (vs_face[1] > 0) {
		color = texture(top_textures,  vec3(vs_tex_coords, vs_block_type));
	} else if (vs_face[1] < 0) {
		color = texture(bottom_textures,  vec3(vs_tex_coords, vs_block_type));
	} else {
		color = texture(side_textures,  vec3(vs_tex_coords, vs_block_type));
	}
	
	// discard stuff that we can barely see, else all it's gonna do is mess with our depth buffer
	if (color.a < 0.5) {
		discard;
	}

	// if in water, make everything blue depending on depth
	if (bool(uni.in_water)) {
		// get depth of current fragment
		float depth = gl_FragCoord.z;
		
		float strength = soft_increase(depth);

		// tint fragment blue
		// TODO: Figure out why this doesn't make farther stuff show up less visibly...
		vec4 ocean_blue = vec4(0.0, 0.0, 1.0, 1.0);
		color = mix(color, ocean_blue, strength / 1.5f);
	}
}
