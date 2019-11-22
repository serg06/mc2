#version 450 core

// output batch of 3 control points
// same as input size, currently
layout (vertices = 3) out;

void main(void)
{
    // Only if I am invocation 0 ... (why?)
    if (gl_InvocationID == 0)
    {
		// when passing a triangle, the first index of gl_TessLevelInner corresponds to the inside
        gl_TessLevelInner[0] = 5.0;

		// when passing a triangle, the first 3 indices of gl_TessLevelOuter correspond to the sides
        gl_TessLevelOuter[0] = 5.0;
        gl_TessLevelOuter[1] = 5.0;
        gl_TessLevelOuter[2] = 5.0;
    }

    // for every input control point, copy its position to output control point.
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
}
