#version 450 core
#define FLOAT_BEFORE_1 0.999999940f
#define SIDE_DARK_FACTOR 0.1f

in flat uint gs_block_type;
in vec2 gs_tex_coords; // texture coords in [0.0, 1.0]
in flat ivec3 gs_face;


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

layout (location = 0) out vec4 color;
layout (depth_greater) out float gl_FragDepth;

// block textures
layout (binding = 0) uniform sampler2DArray block_textures[3];
// 0: top_textures
// 1: side_textures
// 2: bottom_textures

float soft_increase(float x) {
	return -1/((x+1)) + 1;
}

float soft_increase2(float x, float cap) {
	return min(sqrt(x/cap), 1.0f);
}

void main(void)
{
	color = texture(block_textures[1 - sign(gs_face[1])],  vec3(gs_tex_coords, gs_block_type));
	gl_FragDepth = gl_FragCoord.z;

	// make sides a little darker so it looks a little better - that's whiat Minecraft does!
	int face_idx = abs(gs_face[1] * 1 + gs_face[2] * 2);
	color = vec4(color.xyz * (1.0f - ((face_idx + 2) % 3) * SIDE_DARK_FACTOR), color.a);

	// if fragment is transparent, update depth buffer a tiny bit just so we know that's not a t-junction
	if (color.a == 0) {
		gl_FragDepth = FLOAT_BEFORE_1;
	}

	// if in water, make everything blue depending on depth
	else if (bool(uni.in_water)) {
		// get depth of current fragment
		float depth = gl_FragCoord.z / gl_FragCoord.w;
		
		float strength = soft_increase(depth/5.0f);

		// tint fragment blue
		vec4 ocean_blue = vec4(0.0f, 0.0f, 0.5f, 1.0f);
		color = mix(color, ocean_blue, strength);
	}
}
