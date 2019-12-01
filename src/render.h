// All the info about our rendering practices
#ifndef __RENDER_H__
#define __RENDER_H__

#include "GL/gl3w.h"

#include <vmath.h>

#define TRANSFORM_MATRIX_COORDS_OFFSET (2*sizeof(vmath::mat4))

// all the opengl info for our game
struct OpenGLInfo {
	// progra
	GLuint rendering_program;

	// VAOs
	GLuint vao_cube, vao2;

	// buffers
	GLuint trans_buf; // transformations buffer - currently stores view and projection transformations.
	GLuint vert_buf; // vertices buffer - currently stores vertices for a single 3D cube

	// binding points
	const GLuint vert_buf_bidx = 0; // vertex buffer's binding-point index
	const GLuint chunk_types_bidx = 1;
	const GLuint mini_relative_coords_bidx = 2;

	// uniform binding points
	const GLuint trans_buf_uni_bidx = 0; // transformation buffer's uniform binding-point index

	// attribute indices
	const GLuint position_attr_idx = 0; // index of 'position' attribute
	const GLuint chunk_types_attr_idx = 1; // index of 'block_type' attribute
	const GLuint mini_relative_coords_attr_bidx = 2; // index of 'mini_relative_coords' attribute
};

void setup_opengl(OpenGLInfo);

#endif /* __RENDER_H__ */
