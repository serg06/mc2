#version 450 core

layout (location = 0) in vec4 position;

out vec4 vs_color;

layout (std140, binding = 0) uniform UNI_IN
{
    mat4 mv_matrix; // takes up 16 bytes
    mat4 proj_matrix; // takes up 16 bytes
} uni;

vec4 get_color() {
	if (gl_VertexID < 6) {
		vs_color = vec4(1.0, 0.0, 0.0, 1.0);
	}
	else if (gl_VertexID < 12) {
		vs_color = vec4(0.0, 1.0, 0.0, 1.0);
	}
	else if (gl_VertexID < 18) {
		vs_color = vec4(0.0, 0.0, 1.0, 1.0);
	}
	else if (gl_VertexID < 24) {
		vs_color = vec4(0.5, 0.5, 0.0, 1.0);
	}
	else if (gl_VertexID < 30) {
		vs_color = vec4(0.0, 0.5, 0.5, 1.0);
	}
	else if (gl_VertexID < 36) {
		vs_color = vec4(0.5, 0.0, 0.5, 1.0);
	}

	return vs_color;
}

// shader starts executing here
void main(void)
{
	gl_Position = uni.proj_matrix * uni.mv_matrix * position;
	vs_color = position * 2.0 + vec4(0.5, 0.5, 0.5, 1.0);
	//vs_color = get_color();
}
