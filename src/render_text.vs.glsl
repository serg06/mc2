#version 450 core

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8

// ascii character code
layout (location = 0) in uint char_code;

out uint vs_char_code;
out uint vs_char_idx;

void main(void)
{
	// geometry shader does all the work
	vs_char_code = char_code;
	vs_char_idx = gl_VertexID;
}
