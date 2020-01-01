#version 450 core

/**
 * This shader merges fbo1 (depth1) and fbo2 (color2/depth2) by drawing pixels that are more forwards
 */

// width and height of image we're processing
layout (location = 0) uniform float width;
layout (location = 1) uniform float height;

// one pixel distance in each direction (lmao nice one OpenGL this is dumb)
float dx = 1/width;
float dy = 1/height;

// texture arrays
layout (binding = 6) uniform sampler2D depth1_in;
layout (binding = 7) uniform sampler2D color2_in;
layout (binding = 8) uniform sampler2D depth2_in;

// outputs
layout (location = 0) out vec4 color_out;
//layout (depth_less) out float gl_FragDepth;
out float gl_FragDepth; // DEBUG: no layout

// global vars -- x and y are in [0.0, 1.0] now.
float x = gl_FragCoord.x / width;
float y = gl_FragCoord.y / height;

// TODO: compare generating x/y like that ^ with passing texture coordinate
in vec2 gs_tex_coord;

void main() {
	// get color/depth values
	float depth1 = texture(depth1_in, vec2(x, y))[0];
	vec4 color2 = texture(color2_in, vec2(x, y));
	float depth2 = texture(depth2_in, vec2(x, y))[0];

	// OK HERE'S WHERE I'M STUCK:
	//   OPTION 1: merge_fbos() reads fbo1 and fbo2's colors/depths and merges colors depending on the depths. Downside: Need to output to a 3rd FBO.
	//   OPTION 2: merge_fbos() reads fbo1's depths and fbo2's colors/depths and draws fbo2 if it's nearer than fbo1. Really easy to use with alpha blending. Can disable depth writing and write back to fbo1's colors!

	// OPTION 2 IT IS!

	// depth test
	if (depth2 >= depth1) {
		discard;
	}

	// output new color and depth
	// NOTE: if you don't want depth to be updated (e.g. if drawing translucent stuff), just disable depth writing before calling the shader
	color_out = color2;
	gl_FragDepth = depth2;
}
