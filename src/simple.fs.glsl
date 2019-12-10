#version 450 core

in vec4 vs_color;
in flat uint vs_block_type;
in vec2 vs_tex_coords; // texture coords in [0.0, 1.0]
in flat uint horizontal; // 0 = quad is vertical, 1 = quad is horizontal (TODO: Remove this once we start inputting quad face to vertex shader.)

out vec4 color;

// our grass texture!
layout (binding = 0) uniform sampler2D grass_top;
layout (binding = 1) uniform sampler2D grass_side;

void main(void)
{
	// TODO: Remove branching.	

	// GRASS
	if (vs_block_type == 2) {
		if (horizontal != 0) {
			color = texture(grass_top, vs_tex_coords);
		} else {
			color = texture(grass_side, vs_tex_coords);
		}
	// DEFAULT
	} else {
		color = vs_color;
	}
}
