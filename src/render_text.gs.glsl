#version 450 core

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8
#define MAX_CHARS_HORIZONTAL 64

// get char code, output char quad + texture coordinates
layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

// ascii character code
in uint vs_char_code[];
out flat uint gs_char_code;

// index of character in line
in uint vs_char_idx[];

// texture coords
out flat ivec2 gs_tex_coords;

layout (std140, binding = 1) uniform UNI_IN
{
	// member					base alignment			base offset		size	aligned offset	extra info
	ivec4 start_pos;			// 16					0				16		16 				
	ivec4 screen_dimensions;	// 16					0				16		32 				
} uni;

void main(void)
{
	// passthrough data
	gs_char_code = vs_char_code[0];

	// find character's top-left corner
	float unit_offset = uni.screen_dimensions.x / MAX_CHARS_HORIZONTAL;
	float x_offset = float(uni.start_pos.x + vs_char_idx[0]) * unit_offset;
	float y_offset = float(uni.start_pos.y) * unit_offset;

	// draw and texture character relative to that top-left corner
	gs_tex_coords = ivec2(0, 0); gl_Position = vec4(x_offset, y_offset, 0.0f, 1.0f); EmitVertex();
	gs_tex_coords = ivec2(CHAR_WIDTH, 0); gl_Position = vec4(x_offset + unit_offset, y_offset, 0.0f, 1.0f); EmitVertex();
	gs_tex_coords = ivec2(0, CHAR_HEIGHT); gl_Position = vec4(x_offset, y_offset + unit_offset, 0.0f, 1.0f); EmitVertex();
	gs_tex_coords = ivec2(CHAR_WIDTH, CHAR_HEIGHT); gl_Position = vec4(x_offset + unit_offset, y_offset + unit_offset, 0.0f, 1.0f); EmitVertex();
}
