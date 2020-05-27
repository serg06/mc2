#pragma once

#include "fbo.h"
#include "GL/gl3w.h"
#include "glfw/glfw3.h"
#include "util.h"
#include "vmath.h"

#include <cstdint>
#include <vector>

constexpr int TRANSFORM_BUFFER_COORDS_OFFSET = (2 * sizeof(vmath::mat4));
// max chars displayed on screen horizontally
constexpr int MAX_CHARS_HORIZONTAL = 48;

constexpr GLfloat color_black[] = { 0.0f, 0.0f, 0.0f, 1.0f };
constexpr GLfloat color_sky_blue[] = { 135 / 255.0f, 206 / 255.0f, 235 / 255.0f, 1.0f };
constexpr GLfloat color_empty[] = { 0.0f, 0.0f, 0.0f, 0.0f };

// all the GLFW info for our app
struct GlfwInfo {
	std::string title = "OpenGL";
	bool debug = GL_TRUE;
	bool msaa = GL_FALSE;
	int width = 800;
	int height = 600;

	// TODO: make these dimensions such that window defaults to middle of screen
	int width_before_fullscreen = 800;
	int height_before_fullscreen = 600;
	int xpos_before_fullscreen = 100;
	int ypos_before_fullscreen = 100;

	float vfov = 59.0f; // vertical fov -- 59.0 vfov = 90.0 hfov
	float mouseX_Sensitivity = 0.25f;
	float mouseY_Sensitivity = 0.25f;
};

const enum TextAlignment : GLuint {
	BOTTOM_LEFT = 0x0,
	TOP_LEFT = 0x1,
	BOTTOM_RIGHT = 0x2,
	TOP_RIGHT = 0x3
};

// all the OpenGL info for our game
struct OpenGLInfo {
	// programs
	GLuint game_rendering_program = 0;
	GLuint text_rendering_program = 0;
	GLuint tjunction_fixing_program = 0;
	GLuint fbo_merging_program = 0;

	// VAOs
	GLuint vao_empty = 0, vao_cube = 0, vao_quad = 0, vao_text = 0;

	// FBOs
	FBO fbo_out;
	FBO fbo_terrain;
	FBO fbo_water;
	FBO fbo_tjunc_fix;
	FBO fbo_merge_fbos;

	inline std::vector<FBO*> get_fbos() {
		return { &fbo_out, &fbo_terrain, &fbo_water, &fbo_tjunc_fix, &fbo_merge_fbos };
	}

	// render input buffers
	GLuint trans_uni_buf = 0; // transformations uniform buffer - for game rendering 
	GLuint text_buf = 0; // text input buffer - for text rendering
	GLuint text_uni_buf = 0; // text uniform buffer - for text rendering

	// textures
	GLuint top_textures;
	GLuint side_textures;
	GLuint bottom_textures;
	GLuint font_textures;

	// texture units to bind to (like binding points but for textures)
	// TODO: make sure I stay within limits - maybe re-use old binding points
	GLuint top_textures_tunit = 0;
	GLuint side_textures_tunit = 1;
	GLuint bottom_textures_tunit = 2;

	GLuint font_textures_tunit = 3;

	GLuint tjunc_color_in_tunit = 4;
	GLuint tjunc_depth_in_tunit = 5;

	GLuint merger_color_in_tunit = 6;
	GLuint merger_depth_in_tunit = 7;

	// QUAD VAO binding points
	const GLuint vert_buf_bidx = 0; // vertex buffer's binding-point index
	const GLuint chunk_types_bidx = 1;

	const GLuint quad_data_bidx = 2;
	const GLuint q_base_coords_bidx = 6;

	// TEXT VAO binding points
	const GLuint text_char_code_bidx = 0;

	// uniform binding points
	const GLuint trans_buf_uni_bidx = 0; // transformation uniform for QUAD VAO
	const GLuint text_uni_bidx = 1; // text uniform info

	// uniform locations relative to current program
	const GLuint fix_tjunc_uni_width_loc = 0; // text uniform info
	const GLuint fix_tjunc_uni_height_loc = 1; // text uniform info

	// attribute indices for QUAD VAO
	const GLuint position_attr_idx = 0; // index of 'position' attribute
	const GLuint chunk_types_attr_idx = 1; // index of 'block_type' attribute

	const GLuint q_block_type_attr_idx = 2;
	const GLuint q_corner1_attr_idx = 3;
	const GLuint q_corner2_attr_idx = 4;
	const GLuint q_face_attr_idx = 5;
	const GLuint q_base_coords_attr_idx = 6;
	const GLuint q_lighting_attr_idx = 7;
	const GLuint q_metadata_attr_idx = 8;

	// attribute indices for TEXT VAO
	const GLuint text_char_code_attr_idx = 0;
};

// packed so that quads match quads on GPU
#pragma pack(push, 1)
struct Quad3D {
	uint8_t block;
	vmath::ivec3 corner1;
	vmath::ivec3 corner2;
	vmath::ivec3 face;
	uint8_t lighting; // left 4 bits: sunlight. right 4 bits: torchlight.
	uint8_t metadata; // other metadata that a block can have. Should never use more than 4 bits.
};
#pragma pack(pop)

void setup_glfw(GlfwInfo*, GLFWwindow**);
void setup_opengl(GlfwInfo*, OpenGLInfo*);
void render_text(OpenGLInfo* glInfo, const vmath::ivec2 start_pos, const vmath::ivec2 screen_dimensions, const char* text, const unsigned size);
void fix_tjunctions(OpenGLInfo* glInfo, GlfwInfo *windowInfo, GLuint fbo_out, FBO& fbo_in);
void merge_fbos(OpenGLInfo* glInfo, GLuint fbo_out, FBO& fbo_in);
void opengl_on_resize(OpenGLInfo& glInfo, int width, int height);
