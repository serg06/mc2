#version 450 core

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8

// ascii character code
layout (location = 0) in uint char_code;

out uint vs_char_code;
out uint vs_char_idx;

layout (std140, binding = 1) uniform UNI_IN
{
	// member					base alignment			base offset		size	aligned offset	extra info
	ivec4 start_pos;			// 16					0				16		16 				
	ivec4 screen_dimensions;	// 16					0				16		32 				
} uni;


void main(void)
{
	// geometry shader does all the work
	vs_char_code = char_code;
	vs_char_idx = gl_VertexID;
}
