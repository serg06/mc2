#version 450 core

layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

// generate 4 vertices representing 4 screen corners
void main(void)
{
	gl_Position = vec4(-1, -1, 0, 1); EmitVertex();
	gl_Position = vec4(-1, 1, 0, 1); EmitVertex();
	gl_Position = vec4(1, -1, 0, 1); EmitVertex();
	gl_Position = vec4(1, 1, 0, 1); EmitVertex();
}
