// All the info about our rendering practices
#ifndef __RENDER_H__
#define __RENDER_H__

#include "GL/gl3w.h"
#include "GLFW/glfw3.h"

#include <vmath.h>

#define TRANSFORM_BUFFER_COORDS_OFFSET (2*sizeof(vmath::mat4))

// all the GLFW info for our app
struct GlfwInfo {
	std::string title = "OpenGL";
	bool debug = GL_TRUE;
	bool msaa = GL_FALSE;
	int width = 800;
	int height = 600;
	float vfov = 59.0f; // vertical fov -- 59.0 vfov = 90.0 hfov
	float mouseX_Sensitivity = 0.25f;
	float mouseY_Sensitivity = 0.25f;
};

// all the OpenGL info for our game
struct OpenGLInfo {
	// program
	GLuint rendering_program;

	// VAOs
	GLuint vao_cube, vao_quad;

	// buffers
	GLuint trans_buf; // transformations buffer - currently stores view and projection transformations.
	GLuint vert_buf; // vertices buffer - currently stores vertices for a single 3D cube

	// binding points
	const GLuint vert_buf_bidx = 0; // vertex buffer's binding-point index
	const GLuint chunk_types_bidx = 1;
	const GLuint mini_relative_coords_bidx = 2;
	const GLuint quad_buf_bidx = 3;

	const GLuint quad_block_type_bidx = 4;
	const GLuint quad_corner_bidx = 5;

	// uniform binding points
	const GLuint trans_buf_uni_bidx = 0; // transformation buffer's uniform binding-point index

	// attribute indices
	const GLuint position_attr_idx = 0; // index of 'position' attribute
	const GLuint chunk_types_attr_idx = 1; // index of 'block_type' attribute
	const GLuint mini_relative_coords_attr_bidx = 2; // index of 'mini_relative_coords' attribute

	const GLuint q_size_attr_idx = 3;
	const GLuint q_is_back_face_attr_idx = 4;
	const GLuint q_block_type_attr_idx = 5;
	//const GLuint q_corners_attr_idx = 6;

	const GLuint q_corner1_attr_idx = 6;
	const GLuint q_corner2_attr_idx = 7;
	const GLuint q_corner3_attr_idx = 8;
	const GLuint q_corner4_attr_idx = 9;

	const GLuint q_corner_attr_idx = 10;
};

struct Quad {
	// for debugging
	int size;

	// block face
	bool is_back_face;
	uint8_t block;

	// coordinates of corners of quad
	//vmath::ivec3 corners[4];
	vmath::ivec3 corners[4]; // only uses ivec4 so we can feed it into a matrix
};

void setup_glfw(GlfwInfo*, GLFWwindow**);
void setup_opengl(OpenGLInfo*);

#endif /* __RENDER_H__ */
