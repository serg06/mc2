#version 450 core

layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

out vec2 gs_tex_coord;

// generate 4 vertices representing 4 screen corners
void main(void)
{
	gl_Position = vec4(-1, -1, 0, 1); gs_tex_coord = vec2(0, 0); EmitVertex();
	gl_Position = vec4(-1, 1, 0, 1); gs_tex_coord = vec2(0, 1); EmitVertex();
	gl_Position = vec4(1, -1, 0, 1); gs_tex_coord = vec2(1, 0); EmitVertex();
	gl_Position = vec4(1, 1, 0, 1); gs_tex_coord = vec2(1, 1); EmitVertex();
}
