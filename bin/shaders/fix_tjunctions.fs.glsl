#version 450 core
#define MAX_DEPTH 1.0

/**
 * This shader fixes tjunctions in a color/depth buffer pair by filling in tjunction holes/lines.
 */

// width and height of image we're processing
layout (location = 0) uniform float width;
layout (location = 1) uniform float height;

// one pixel distance in each direction (lmao nice one OpenGL this is dumb)
float dx = 1/width;
float dy = 1/height;

// texture arrays
layout (binding = 4) uniform sampler2D tjunc_color_in;
layout (binding = 5) uniform sampler2D tjunc_depth_in;

// outputs
layout (location = 0) out vec4 color_out;
layout (depth_less) out float gl_FragDepth;

// global vars -- x and y are in [0.0, 1.0] now.
float x = gl_FragCoord.x / width;
float y = gl_FragCoord.y / height;

// TODO: compare generating x/y like that ^ with passing texture coordinate
in vec2 gs_tex_coord;

struct color_depth_pair {
	vec4 color;
	float depth;
};

// fix color/depth of pixel depending on two other pixels
void fix_two(inout color_depth_pair cd, const color_depth_pair side1, const color_depth_pair side2) {
	// if depth is max (so pixel not filled) and other two aren't max (so other two are filled)
	if (cd.depth == MAX_DEPTH && side1.depth < MAX_DEPTH && side2.depth < MAX_DEPTH) {
		// combine their depths/colors into ours
		cd.color = mix(side1.color, side2.color, 0.5f);
		cd.depth = mix(side1.depth, side2.depth, 0.5f);
	}
}

// fix color/depth of pixel depending on top/bottom pixels
void fix_top_bottom(inout color_depth_pair cd) {
	color_depth_pair top, bottom;

	top.color = texture(tjunc_color_in, vec2(x, y + dy));
	top.depth = texture(tjunc_depth_in, vec2(x, y + dy))[0];

	bottom.color = texture(tjunc_color_in, vec2(x, y - dy));
	bottom.depth = texture(tjunc_depth_in, vec2(x, y - dy))[0];

	fix_two(cd, top, bottom);
}

// fix color/depth of pixel depending on left/right pixels
void fix_left_right(inout color_depth_pair cd) {
	color_depth_pair left, right;

	left.color = texture(tjunc_color_in, vec2(x - dx, y));
	left.depth = texture(tjunc_depth_in, vec2(x - dx, y))[0];

	right.color = texture(tjunc_color_in, vec2(x + dx, y));
	right.depth = texture(tjunc_depth_in, vec2(x + dx, y))[0];

	fix_two(cd, left, right);
}

void main() {
	// get color/depth values
	color_depth_pair cd;
	cd.color = texture(tjunc_color_in, vec2(x, y));
	cd.depth = texture(tjunc_depth_in, vec2(x, y))[0];

	// update
	fix_left_right(cd);
	fix_top_bottom(cd);

	// discard if not changed
	// ... TODO

	// output
	color_out = cd.color;
	gl_FragDepth = cd.depth;
}
