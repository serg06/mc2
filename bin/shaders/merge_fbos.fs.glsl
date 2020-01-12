#version 450 core

/**
 * This shader just outputs the texture input again.
 */

// texture arrays
layout (binding = 6) uniform sampler2D depth_in;
layout (binding = 7) uniform sampler2D color_in;

// outputs
layout (location = 0) out vec4 color_out;
out float gl_FragDepth;

in vec2 gs_tex_coord;

void main() {
	color_out = texture(color_in, gs_tex_coord);
	gl_FragDepth = texture(depth_in, gs_tex_coord)[0];
}
