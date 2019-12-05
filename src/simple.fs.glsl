#version 450 core

in vec4 vs_color;
in flat uint vs_block_type;
in vec2 vs_tex_coords; // texture coords in [0.0, 1.0]

out vec4 color;

// our grass texture!
layout (binding = 0) uniform sampler2D grass_tex;

void main(void)
{
	// color = vs_color;
	// color = texture(grass_tex, vs_tex_coords);
	if (vs_block_type == 1) {
		color = texture(grass_tex, vs_tex_coords);
	} else {
		color = vs_color;
	}
}
