#version 450 core

in vec4 gs_color;
out vec4 color;

void main(void)
{
	color = gs_color;
}
