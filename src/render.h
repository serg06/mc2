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
	GLuint game_rendering_program;
	GLuint text_rendering_program;

	// VAOs
	GLuint vao_cube, vao_quad;

	// render buffers
	GLuint trans_buf = 0; // transformations buffer - currently stores view and projection transformations.
	GLuint vert_buf = 0; // vertices buffer - currently stores vertices for a single 3D cube

	// textures
	GLuint top_textures;
	GLuint side_textures;
	GLuint bottom_textures;
	GLuint font_textures;
	
	// texture units to bind to
	GLuint top_textures_tunit = 0;
	GLuint side_textures_tunit = 1;
	GLuint bottom_textures_tunit = 2;
	GLuint font_textures_tunit = 3;

	// render binding points
	const GLuint vert_buf_bidx = 0; // vertex buffer's binding-point index
	const GLuint chunk_types_bidx = 1;

	const GLuint quad_block_type_bidx = 2;
	const GLuint q_corner1_bidx = 3;
	const GLuint q_corner2_bidx = 4;
	const GLuint q_face_bidx = 5;

	// uniform binding points
	const GLuint trans_buf_uni_bidx = 0; // transformation buffer's uniform binding-point index

	// attribute indices
	const GLuint position_attr_idx = 0; // index of 'position' attribute
	const GLuint chunk_types_attr_idx = 1; // index of 'block_type' attribute

	const GLuint q_block_type_attr_idx = 2;
	const GLuint q_corner1_attr_idx = 3;
	const GLuint q_corner2_attr_idx = 4;
	const GLuint q_face_attr_idx = 5;
};

struct Quad3D {
	uint8_t block;
	vmath::ivec3 corners[2];
	vmath::ivec3 face;
};

void setup_glfw(GlfwInfo*, GLFWwindow**);
void setup_opengl(OpenGLInfo*);

#endif /* __RENDER_H__ */
