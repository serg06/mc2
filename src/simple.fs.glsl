#version 450 core

in vec4 vs_color;
in flat uint vs_block_type;
in vec2 vs_tex_coords; // texture coords in [0.0, 1.0]
in flat uint horizontal; // 0 = quad is vertical, 1 = quad is horizontal (TODO: Remove this once we start inputting quad face to vertex shader.)

out vec4 color;

// our grass texture!
layout (binding = 0) uniform sampler2D grass_top;
layout (binding = 1) uniform sampler2D grass_side;

// texture arrays
layout (binding = 2) uniform sampler2DArray top_textures;
layout (binding = 3) uniform sampler2DArray side_textures;
layout (binding = 4) uniform sampler2DArray bottom_textures;

void main(void)
{
	// TODO: Remove branching.
	if (horizontal != 0) {
		color = texture(top_textures, vec3(vs_tex_coords, vs_block_type));
		if (color.a < 0.5) {
			discard;
		}
	} else {
		color = texture(side_textures,  vec3(vs_tex_coords, vs_block_type));
		if (color.a < 0.5) {
			discard;
		}
	}
}
