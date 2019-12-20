// All the info about our rendering practices
#ifndef __RENDER_H__
#define __RENDER_H__

#include "GL/gl3w.h"
#include "GLFW/glfw3.h"

#include <vmath.h>

#define TRANSFORM_BUFFER_COORDS_OFFSET (2*sizeof(vmath::mat4))
// max chars displayed on screen horizontally
#define MAX_CHARS_HORIZONTAL 48

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

static const enum TextAlignment : GLuint {
	BOTTOM_LEFT = 0x0,
	TOP_LEFT = 0x1,
	BOTTOM_RIGHT = 0x2,
	TOP_RIGHT = 0x3
};

// all the OpenGL info for our game
struct OpenGLInfo {
	// program
	GLuint game_rendering_program;
	GLuint text_rendering_program;

	// VAOs
	GLuint vao_cube, vao_quad, vao_text;

	// render buffers
	GLuint trans_buf = 0; // transformations buffer - currently stores view and projection transformations.
	GLuint vert_buf = 0; // vertices buffer - currently stores vertices for a single 3D cube
	GLuint text_buf = 0; // text input buffer
	GLuint text_uni_buf = 0; // text uniform buffer

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

	// QUAD VAO binding points
	const GLuint vert_buf_bidx = 0; // vertex buffer's binding-point index
	const GLuint chunk_types_bidx = 1;

	const GLuint quad_block_type_bidx = 2;
	const GLuint q_corner1_bidx = 3;
	const GLuint q_corner2_bidx = 4;
	const GLuint q_face_bidx = 5;

	// TEXT VAO binding points
	const GLuint text_char_code_bidx = 0;

	// uniform binding points
	const GLuint trans_buf_uni_bidx = 0; // transformation uniform for QUAD VAO
	const GLuint text_uni_bidx = 1; // text uniform info

	// attribute indices for QUAD VAO
	const GLuint position_attr_idx = 0; // index of 'position' attribute
	const GLuint chunk_types_attr_idx = 1; // index of 'block_type' attribute

	const GLuint q_block_type_attr_idx = 2;
	const GLuint q_corner1_attr_idx = 3;
	const GLuint q_corner2_attr_idx = 4;
	const GLuint q_face_attr_idx = 5;

	// attribute indices for TEXT VAO
	const GLuint text_char_code_attr_idx = 0;
};

struct Quad3D {
	uint8_t block;
	vmath::ivec3 corners[2];
	vmath::ivec3 face;
};

void setup_glfw(GlfwInfo*, GLFWwindow**);
void setup_opengl(OpenGLInfo*);
void render_text(OpenGLInfo* glInfo, const vmath::ivec2 start_pos, const vmath::ivec2 screen_dimensions, const char* text, const unsigned size);


#endif /* __RENDER_H__ */
