#version 450 core

in vec4 vs_color;
out vec4 color;

void main(void)
{


	color = vs_color;

// COOL RANDOM COLORS; but my eyes hurt.
//	// The beauty of this algorithm, is that super-nearby pixels are similar in color!	
//	color = vec4(sin(gl_FragCoord.x * 0.25) * 0.5 + 0.5,
//               cos(gl_FragCoord.y * 0.25) * 0.5 + 0.5,
//                sin(gl_FragCoord.x * 0.15) * cos(gl_FragCoord.y * 0.15),
//               1.0);
}
