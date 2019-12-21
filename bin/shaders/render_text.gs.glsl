#version 450 core

#define MAX_CHARS_HORIZONTAL 48
#define SPACE_BETWEEN_LINES (0.2f)

// get char code, output char quad + texture coordinates
layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

// ascii character code
in uint vs_char_code[];
out flat uint gs_char_code;

// index of character in line
in uint vs_char_idx[];

// texture coords
out vec2 gs_tex_coords;

layout (std140, binding = 1) uniform UNI_IN
{
	// member					base alignment			base offset		size	aligned offset	extra info
	ivec2 start_pos;			// 16					0				16		16 				
	ivec2 screen_dimensions;	// 16					0				16		32 				
	uint alignment;
	uint str_size;
} uni;

void main(void)
{
	// passthrough data
	gs_char_code = vs_char_code[0];

	// width of each character - this is always relative to window width
	float unit_size = 2.0f / (MAX_CHARS_HORIZONTAL + 2);

	// how much the height needs to be stretched to keep the character square
	float y_stretch = float(uni.screen_dimensions.x) / float(uni.screen_dimensions.y);

	float x_offset, y_offset;	

	// first bit of alignment = decides whether to align to top or bottom
	if (bool(uni.alignment & 0x1)) {
		// align to top - uni.start_pos.y is relative to top
		y_offset = 1.0f - 2 * unit_size * y_stretch - float(uni.start_pos.y) * unit_size * y_stretch * (1 + SPACE_BETWEEN_LINES);
	} else {
		// align to bottom - uni.start_pos.y is relative to bottom
		y_offset = -1.0f + unit_size * y_stretch + float(uni.start_pos.y) * unit_size * y_stretch * (1 + SPACE_BETWEEN_LINES);
	}

	// second bit decides whether to align to left or right
	if (bool(uni.alignment & 0x2)) {
		// align to right
		x_offset = 1.0f - unit_size - float(uni.start_pos.x + (uni.str_size - vs_char_idx[0])) * unit_size;
	} else {
		// align to left
		x_offset = -1.0f + unit_size + float(uni.start_pos.x + vs_char_idx[0]) * unit_size;
	}

	// draw and texture character relative to their aligned corner
	gs_tex_coords = vec2(0, 1); gl_Position = vec4(x_offset, y_offset, 0.0f, 1.0f); EmitVertex();
	gs_tex_coords = vec2(1, 1); gl_Position = vec4(x_offset + unit_size, y_offset, 0.0f, 1.0f); EmitVertex();
	gs_tex_coords = vec2(0, 0); gl_Position = vec4(x_offset, y_offset + unit_size * y_stretch, 0.0f, 1.0f); EmitVertex();
	gs_tex_coords = vec2(1, 0); gl_Position = vec4(x_offset + unit_size, y_offset + unit_size * y_stretch, 0.0f, 1.0f); EmitVertex();
}
