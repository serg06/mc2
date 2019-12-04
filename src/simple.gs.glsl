#version 450 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

in uint vs_block_type[];
in vec4 vs_color[];

out vec4 gs_color;

void main(void)
{
	// // don't draw air
	// if (vs_block_type[0] == 0) {
	// 	return; 
	// }

	for (int i = 0; i < 3; i++) {		
		gl_Position = gl_in[i].gl_Position;
		gs_color = vs_color[i];
		EmitVertex();
	}
}
