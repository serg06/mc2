#version 450 core

// ascii character code
in flat uint gs_char_code;

// texture coords
in vec2 gs_tex_coords;

out vec4 color;

// font textures
layout (binding = 3) uniform sampler2DArray font_textures;

void main(void)
{
	color = texture(font_textures, vec3(gs_tex_coords, gs_char_code));
}
