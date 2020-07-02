#version 450 core

// texture arrays
layout (binding = 8) uniform sampler2D bckgnd_tex;

// outputs
layout (location = 0) out vec4 color_out;

// change layout of gl_FragCoord to be upper-left like Minecraft
layout(origin_upper_left) in vec4 gl_FragCoord;

// global vars
float bckgnd_res = 16.0f;

void main()
{
	color_out = texture(bckgnd_tex, gl_FragCoord.xy / (bckgnd_res * 4.0f));
}
