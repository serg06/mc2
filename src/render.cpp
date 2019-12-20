#include "block.h"
#include "render.h"
#include "shapes.h"
#include "util.h"

#include "GLFW/glfw3.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

#include <experimental/filesystem>
#include <string>
#include <sys/stat.h>
#include <tuple>
#include <vector>
#include <vmath.h>

#define BLOCK_TEXTURE_WIDTH 16
#define BLOCK_TEXTURE_HEIGHT 16
#define TEXTURE_COMPONENTS 4
#define MAX_CHARS 256

namespace fs = std::experimental::filesystem;
using namespace vmath;

// setup glfw window
// windowInfo -> info for setting up this window
// window -> pointer to a window pointer, which we're gonna set up
void setup_glfw(GlfwInfo* windowInfo, GLFWwindow** window) {
	if (!glfwInit()) {
		MessageBox(NULL, "Failed to initialize GLFW.", "GLFW error", MB_OK);
		return;
	}

	// OpenGL 4.5
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);

	// using OpenGL core profile
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// remove deprecated functionality (might as well, 'cause I'm using gl3w)
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	// disable MSAA
	glfwWindowHint(GLFW_SAMPLES, windowInfo->msaa);

	// debug mode
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, windowInfo->debug);

	// create window
	*window = glfwCreateWindow(windowInfo->width, windowInfo->height, windowInfo->title.c_str(), nullptr, nullptr);

	if (!*window) {
		MessageBox(NULL, "Failed to create window.", "GLFW error", MB_OK);
		return;
	}

	// set this window as current window
	glfwMakeContextCurrent(*window);

	// lock mouse into screen, for camera controls
	glfwSetInputMode(*window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetInputMode(*window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

	// finally init gl3w
	if (gl3wInit()) {
		MessageBox(NULL, "Failed to initialize OpenGL.", "gl3w error", MB_OK);
		return;
	}

	// disable vsync
	glfwSwapInterval(0);
}

namespace {
	void setup_game_rendering_program(OpenGLInfo* glInfo) {
		// list of shaders to create program with
		std::vector <std::tuple<std::string, GLenum>> shader_fnames = {
			{ "../src/simple.vs.glsl", GL_VERTEX_SHADER },
			//{"../src/simple.tcs.glsl", GL_TESS_CONTROL_SHADER },
			//{"../src/simple.tes.glsl", GL_TESS_EVALUATION_SHADER },
			//{ "../src/simple.gs.glsl", GL_GEOMETRY_SHADER },
			{ "../src/simple.fs.glsl", GL_FRAGMENT_SHADER },
		};

		// create program
		glInfo->game_rendering_program = compile_shaders(shader_fnames);

		// use our program object for rendering
		glUseProgram(glInfo->game_rendering_program);
	}

	void setup_text_rendering_program(OpenGLInfo* glInfo) {
	// list of shaders to create program with
		std::vector <std::tuple<std::string, GLenum>> shader_fnames = {
			{ "../src/render_text.vs.glsl", GL_VERTEX_SHADER },
			{ "../src/render_text.gs.glsl", GL_GEOMETRY_SHADER },
			{ "../src/render_text.fs.glsl", GL_FRAGMENT_SHADER },
		};

		// create program
		glInfo->text_rendering_program = compile_shaders(shader_fnames);

		// use our program object for rendering
		glUseProgram(glInfo->text_rendering_program);
	}

	void setup_opengl_vao_cube(OpenGLInfo* glInfo) {
		const GLfloat(&cube)[108] = shapes::cube_full;

		// vao: create VAO for cube[s], so we can tell OpenGL how to use it when it's bound
		glCreateVertexArrays(1, &glInfo->vao_cube);

		// buffers: create
		glCreateBuffers(1, &glInfo->vert_buf);

		// buffers: allocate space
		// allocate enough for all vertices, and allow editing
		glNamedBufferStorage(glInfo->vert_buf, sizeof(cube), NULL, GL_DYNAMIC_STORAGE_BIT);

		// buffers: insert static data
		// vertex positions
		glNamedBufferSubData(glInfo->vert_buf, 0, sizeof(cube), cube);

		 // vao: enable all cube's attributes, 1 at a time
		glEnableVertexArrayAttrib(glInfo->vao_cube, glInfo->position_attr_idx);
		glEnableVertexArrayAttrib(glInfo->vao_cube, glInfo->chunk_types_attr_idx);

		// vao: set up formats for cube's attributes, 1 at a time
		glVertexArrayAttribFormat(glInfo->vao_cube, glInfo->position_attr_idx, 3, GL_FLOAT, GL_FALSE, 0);
		glVertexArrayAttribIFormat(glInfo->vao_cube, glInfo->chunk_types_attr_idx, 1, GL_BYTE, 0);

		// vao: set binding points for all attributes, 1 at a time
		//      - 1 buffer per binding point; for clarity, to keep it clear, I should only bind 1 attr per binding point
		glVertexArrayAttribBinding(glInfo->vao_cube, glInfo->position_attr_idx, glInfo->vert_buf_bidx);
		glVertexArrayAttribBinding(glInfo->vao_cube, glInfo->chunk_types_attr_idx, glInfo->chunk_types_bidx);

		// vao: bind buffers to their binding points, 1 at a time
		glVertexArrayVertexBuffer(glInfo->vao_cube, glInfo->vert_buf_bidx, glInfo->vert_buf, 0, sizeof(vec3));

		// vao: extra properties
		glBindVertexArray(glInfo->vao_cube);
		glVertexAttribDivisor(glInfo->chunk_types_attr_idx, 1);
		glBindVertexArray(0);
	}

	void setup_opengl_vao_quad(OpenGLInfo* glInfo) {
		// vao: create VAO for Quads, so we can tell OpenGL how to use it when it's bound
		glCreateVertexArrays(1, &glInfo->vao_quad);

		// vao: enable all Quad's attributes, 1 at a time
		glEnableVertexArrayAttrib(glInfo->vao_quad, glInfo->q_block_type_attr_idx);
		glEnableVertexArrayAttrib(glInfo->vao_quad, glInfo->q_corner1_attr_idx);
		glEnableVertexArrayAttrib(glInfo->vao_quad, glInfo->q_corner2_attr_idx);
		glEnableVertexArrayAttrib(glInfo->vao_quad, glInfo->q_face_attr_idx);

		// vao: set up formats for Quad's attributes, 1 at a time
		glVertexArrayAttribIFormat(glInfo->vao_quad, glInfo->q_block_type_attr_idx, 1, GL_UNSIGNED_BYTE, 0);
		glVertexArrayAttribIFormat(glInfo->vao_quad, glInfo->q_corner1_attr_idx, 3, GL_INT, 0);
		glVertexArrayAttribIFormat(glInfo->vao_quad, glInfo->q_corner2_attr_idx, 3, GL_INT, 0);
		glVertexArrayAttribIFormat(glInfo->vao_quad, glInfo->q_face_attr_idx, 3, GL_INT, 0);

		// vao: match attributes to binding indices
		glVertexArrayAttribBinding(glInfo->vao_quad, glInfo->q_block_type_attr_idx, glInfo->quad_block_type_bidx);
		glVertexArrayAttribBinding(glInfo->vao_quad, glInfo->q_corner1_attr_idx, glInfo->q_corner1_bidx);
		glVertexArrayAttribBinding(glInfo->vao_quad, glInfo->q_corner2_attr_idx, glInfo->q_corner2_bidx);
		glVertexArrayAttribBinding(glInfo->vao_quad, glInfo->q_face_attr_idx, glInfo->q_face_bidx);

		// vao: extra properties
		glBindVertexArray(glInfo->vao_quad);

		glVertexAttribDivisor(glInfo->q_block_type_attr_idx, 1); // instance attribute
		glVertexAttribDivisor(glInfo->q_corner1_attr_idx, 1); // instance attribute
		glVertexAttribDivisor(glInfo->q_corner2_attr_idx, 1); // instance attribute
		glVertexAttribDivisor(glInfo->q_face_attr_idx, 1); // instance attribute

		glBindVertexArray(0);
	}

	void setup_opengl_vao_text(OpenGLInfo* glInfo) {
		// vao: create VAO for text, so we can tell OpenGL how to use it when it's bound
		glCreateVertexArrays(1, &glInfo->vao_text);

		// vao: enable all text's attributes, 1 at a time
		glEnableVertexArrayAttrib(glInfo->vao_text, glInfo->text_char_code_attr_idx);

		// vao: set up formats for text's attributes, 1 at a time
		glVertexArrayAttribIFormat(glInfo->vao_text, glInfo->text_char_code_attr_idx, 1, GL_UNSIGNED_BYTE, 0);

		// vao: match attributes to binding indices
		glVertexArrayAttribBinding(glInfo->vao_text, glInfo->text_char_code_attr_idx, glInfo->text_char_code_bidx);

		// buffers: create
		glCreateBuffers(1, &glInfo->text_buf);

		// buffers: allocate
		glNamedBufferStorage(glInfo->text_buf, sizeof(char) * MAX_CHARS_HORIZONTAL, NULL, GL_MAP_WRITE_BIT);

		// buffers: bind once and for all
		glVertexArrayVertexBuffer(glInfo->vao_text, glInfo->text_char_code_bidx, glInfo->text_buf, 0, sizeof(char));
	}

	void setup_opengl_uniforms(OpenGLInfo* glInfo) {
		// create buffers
		glCreateBuffers(1, &glInfo->trans_buf);
		glCreateBuffers(1, &glInfo->text_uni_buf);

		// bind them
		// bind transform buffer to transform uniform
		glBindBufferBase(GL_UNIFORM_BUFFER, glInfo->trans_buf_uni_bidx, glInfo->trans_buf);
		// want to bind my uni buf, but I get performance warnings when I do it here... gonna do it in draw call instead

		// allocate 

		// allocate enough space for 2 transform matrices + current chunk coords + bool in_water
		// todo: use map, then invalidate range when writing
		glNamedBufferStorage(glInfo->trans_buf, 2 * sizeof(mat4) + sizeof(ivec4) + sizeof(GLuint), NULL, GL_DYNAMIC_STORAGE_BIT);
		glNamedBufferStorage(glInfo->text_uni_buf, 2 * sizeof(ivec2) + 2 * sizeof(GLuint), NULL, GL_MAP_WRITE_BIT);
	}

	void setup_opengl_extra_props(OpenGLInfo* glInfo) {
		glPointSize(5.0f);
		glFrontFace(GL_CW);

		// depth testing
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);

		// alpha blending
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}


	// load texture from file
	// writes width*height*4 floats to result
	void load_texture_data(const char* fname, unsigned width, unsigned height, float* result) {
		// check that file exists
		if (!fs::exists(fname)) {
			char buf[256];
			sprintf(buf, "Error: File '%s' doesn't exist!\n", fname);
			WindowsException(buf);
			exit(-1);
		}

		// load in texture from disk
		int tex_width, tex_height, tex_components;
		unsigned char *imgdata = stbi_load(fname, &tex_width, &tex_height, &tex_components, 0);

		// make sure it's correct dimensions
		if (tex_height != height || tex_width != width || tex_components != TEXTURE_COMPONENTS) {
			char buf[256];
			sprintf(buf, "Error: File '%s' has unexpected dimensions!\n", fname);
			WindowsException(buf);
			exit(-1);
		}

		// write result as floats
		for (int i = 0; i < height*width*tex_components; i++) {
			result[i] = imgdata[i] / 255.0f;
		}

		// free
		stbi_image_free(imgdata);
	}

	// load texture data for a block, plus generate mipmaps up to mipmap_level
	void load_block_texture_data(const char* tex_name, float(&data)[((16 * 16) + (8 * 8) + (4 * 4) + (2 * 2) + (1 * 1)) * 4], unsigned mipmap_level) {
		char fname[256];
		sprintf(fname, "./textures/blocks/%s.png", tex_name);
		load_texture_data(fname, 16, 16, data);

		if (mipmap_level > 4) {
			throw "err";
		}

		/* SPECIAL CASES START */

		// grass_top.png

		// gray by default, let's color it green.
		if (strcmp(tex_name, "grass_top") == 0) {
			for (int i = 0; i < BLOCK_TEXTURE_HEIGHT * BLOCK_TEXTURE_WIDTH * TEXTURE_COMPONENTS; i++) {
				// RED
				if ((i % 4) == 0) {
					data[i] *= 115 / 255.0f;
				}

				// GREEN
				if ((i % 4) == 1) {
					data[i] *= 0.8f;
				}

				// BLUE
				if ((i % 4) == 2) {
					data[i] *= 73 / 255.0f;
				}
			}
		}

		// leaves

		// let's make sure non-transparent spots are 100% non-transparent
		if (strcmp(tex_name, "leaves") == 0) {
			for (int i = 0; i < BLOCK_TEXTURE_HEIGHT * BLOCK_TEXTURE_WIDTH * TEXTURE_COMPONENTS; i++) {
				// RED
				if ((i % 4) == 0) {
					data[i] *= 115 / 255.0f;
				}

				// GREEN
				if ((i % 4) == 1) {
					data[i] *= 0.6f;
				}

				// BLUE
				if ((i % 4) == 2) {
					data[i] *= 73 / 255.0f;
				}

				// ALPHA
				if ((i % 4) == 3) {
					if (data[i] > 0.0f) {
						data[i] = 1.0f;
					}
				}
			}
		}

		/* SPECIAL CASES END */

		int start = 0;
		for (int i = 1; i < mipmap_level; i++) {
			int width = 16 / pown(2, i);
			for (int j = 0; j < width*width; j++) {
				data[start + j * 4 + 0] = data[i * 4 * pown(2 * 2, i) + 0];
				data[start + j * 4 + 1] = data[i * 4 * pown(2 * 2, i) + 1];
				data[start + j * 4 + 2] = data[i * 4 * pown(2 * 2, i) + 2];
				data[start + j * 4 + 3] = fmax(data[i * 4 * pown(2 * 2, i) + 0], 0.8f);
			}
			start = width * width * 4;
		}

		// set mipmaps
	}

	void setup_block_textures(OpenGLInfo* glInfo) {
		// data for one texture, plus all mipmaps
		float data[((16 * 16) + (8 * 8) + (4 * 4) + (2 * 2) + (1 * 1)) * 4];

		int mipmap_levels = 0;

		// create textures
		glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &glInfo->top_textures);
		glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &glInfo->side_textures);
		glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &glInfo->bottom_textures);

		// texture arrays
		const auto tex_arrays = { glInfo->top_textures, glInfo->side_textures, glInfo->bottom_textures };

		// allocate space (32-bit float RGBA 16x16) (TODO: GL_RGBA8 instead.)
		for (auto tex_arr : tex_arrays) {
			glTextureStorage3D(tex_arr, mipmap_levels + 1, GL_RGBA32F, 16, 16, MAX_BLOCK_TYPES);
		}

		float* red = new float[16 * 16 * MAX_BLOCK_TYPES * 4];
		for (int i = 0; i < 16 * 16 * MAX_BLOCK_TYPES; i++) {
			((vec4*)red)[i] = { 1.0f, 0.0f, 0.0f, 1.0f };
		}

		// set all textures as BRIGHT RED, so we know when something's wrong
		for (auto tex_arr : tex_arrays) {
			glTextureSubImage3D(tex_arr,
				0,							// Level 0
				0, 0, 0,					// Offset 0, 0, 0
				16, 16, MAX_BLOCK_TYPES,	// 16 x 16 x MAX_BLOCK_TYPES texels, replace entire image
				GL_RGBA,					// Four channel data
				GL_FLOAT,					// Floating point data
				red);						// Pointer to data
		}

		delete[] red;

		// write in each texture that we can
		for (int i = 0; i < MAX_BLOCK_TYPES; i++) {
			// get block
			Block b = Block(i);

			// get block's textures
			std::string top = b.top_texture();
			std::string side = b.side_texture();
			std::string bottom = b.bottom_texture();

			// if they exist, load them in
			if (top.size() > 0) {
				load_block_texture_data(top.c_str(), data, mipmap_levels);
				glTextureSubImage3D(glInfo->top_textures,
					0,			// Level 0
					0, 0, i,	// Offset 0, 0, block_id
					16, 16, 1,	// 16 x 16 x 1 texels, replace one texture
					GL_RGBA,	// Four channel data
					GL_FLOAT,	// Floating point data
					data);		// Pointer to data
			}

			if (side.size() > 0) {
				load_block_texture_data(side.c_str(), data, mipmap_levels);
				glTextureSubImage3D(glInfo->side_textures,
					0,			// Level 0
					0, 0, i,	// Offset 0, 0, block_id
					16, 16, 1,	// 16 x 16 x 1 texels, replace one texture
					GL_RGBA,	// Four channel data
					GL_FLOAT,	// Floating point data
					data);		// Pointer to data
			}

			if (bottom.size() > 0) {
				load_block_texture_data(bottom.c_str(), data, mipmap_levels);
				glTextureSubImage3D(glInfo->bottom_textures,
					0,			// Level 0
					0, 0, i,	// Offset 0, 0, block_id
					16, 16, 1,	// 16 x 16 x 1 texels, replace one texture
					GL_RGBA,	// Four channel data
					GL_FLOAT,	// Floating point data
					data);		// Pointer to data
			}
		}

		// for each texture array
		for (auto tex_arr : tex_arrays) {
			// generate mipmaps
			//glGenerateTextureMipmap(tex_arr);

			// wrap!
			glTextureParameteri(tex_arr, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTextureParameteri(tex_arr, GL_TEXTURE_WRAP_T, GL_REPEAT);

			// filter
			glTextureParameteri(tex_arr, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // TODO: try the 3 other options.
			glTextureParameteri(tex_arr, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}

		// bind them!
		glBindTextureUnit(glInfo->top_textures_tunit, glInfo->top_textures);
		glBindTextureUnit(glInfo->side_textures_tunit, glInfo->side_textures);
		glBindTextureUnit(glInfo->bottom_textures_tunit, glInfo->bottom_textures);
	}

	void setup_font_textures(OpenGLInfo* glInfo) {
		// the font atlas that comes with Minecraft
		// technically we only need 8*8*1 elements, or even less, since we don't need 4 frickin floats to say either 1 or 0, but eh, not like we're struggling for space.
		float *atlas = new float[8 * 8 * MAX_CHARS * 4];

		// create textures
		glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &glInfo->font_textures);

		// allocate space (32-bit float RGBA 16x16) (TODO: Use a smaller datatype. Hell, could even use a fucking bitmap. (As in 1 bit per pixel.))
		glTextureStorage3D(glInfo->font_textures, 1, GL_RGBA32F, 8, 8, MAX_CHARS);

		// fill all with red for debugging
		float* red = new float[8 * 8 * MAX_CHARS * 4];
		for (int i = 0; i < 8 * 8 * MAX_BLOCK_TYPES; i++) {
			((vec4*)red)[i] = { 1.0f, 0.0f, 0.0f, 1.0f };
		}

		glTextureSubImage3D(glInfo->font_textures,
			0,							// Level 0
			0, 0, 0,					// Offset 0, 0, 0
			8, 8, MAX_BLOCK_TYPES,		// 8 x 8 x MAX_CHARS texels, replace entire image
			GL_RGBA,					// Four channel data
			GL_FLOAT,					// Floating point data
			red);

		delete[] red;

		// load in textures
		// 16x16 atlas of 8x8 characters
		load_texture_data("font/default.png", 16 * 8, 16 * 8, atlas);

		// enough space for one character from the atlas
		float char_tex[8 * 8 * 4];

		// for each character
		for (int i = 0; i < MAX_CHARS; i++) {
			// extract texture
			extract_from_atlas(atlas, 16, 16, 4, 8, 8, i, char_tex);

			// write it into our 2D texture array
			glTextureSubImage3D(glInfo->font_textures,
				0,			// Level 0
				0, 0, i,	// Offset 0, 0, char ascii code
				8, 8, 1,	// 8 x 8 x 1 texels, replace one texture
				GL_RGBA,	// Four channel data
				GL_FLOAT,	// Floating point data
				char_tex);		// Pointer to data
		}

		// clamp texture to edge, in case float inaccuracy tries to screw with us
		glTextureParameteri(glInfo->font_textures, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(glInfo->font_textures, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// filter
		glTextureParameteri(glInfo->font_textures, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // TODO: try the 3 other options.
		glTextureParameteri(glInfo->font_textures, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		// bind them!
		glBindTextureUnit(glInfo->font_textures_tunit, glInfo->font_textures);
	}
}

void setup_opengl(OpenGLInfo* glInfo) {
	// setup shaders
	setup_game_rendering_program(glInfo);
	setup_text_rendering_program(glInfo);

	// setup VAOs
	//setup_opengl_vao_cube(glInfo);
	setup_opengl_vao_quad(glInfo);
	setup_opengl_vao_text(glInfo);

	// setup uniforms
	setup_opengl_uniforms(glInfo);

	// setup extra [default] properties
	setup_opengl_extra_props(glInfo);

	// set up textures
	setup_block_textures(glInfo);
	setup_font_textures(glInfo);
}

// render text right here and now, bam!
void render_text(OpenGLInfo* glInfo, const ivec2 start_pos, const ivec2 screen_dimensions, const char* text, const unsigned size) {
	if (size == 0) return;
	assert((start_pos[0] + size) <= MAX_CHARS_HORIZONTAL && "bro your text is gonna leave the screen bro");

	// bind program/VAO
	glUseProgram(glInfo->text_rendering_program);
	glBindVertexArray(glInfo->vao_text);
	glBindBufferBase(GL_UNIFORM_BUFFER, glInfo->text_uni_bidx, glInfo->text_uni_buf);

	// update uniform buffer
	char* uni = (char*)glMapNamedBufferRange(glInfo->text_uni_buf, 0, 2 * sizeof(ivec2) + 2 * sizeof(GLuint), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
	*((ivec2*)(uni + 0)) = start_pos;
	*((ivec2*)(uni + sizeof(ivec2))) = screen_dimensions;
	*((GLuint*)(uni + 2 * sizeof(ivec2))) = TOP_LEFT; // orientation
	*((GLuint*)(uni + 2 * sizeof(ivec2) + sizeof(GLuint))) = size; // string size

	glFlushMappedNamedBufferRange(glInfo->text_uni_buf, 0, 2 * sizeof(ivec2) + 2 * sizeof(GLuint));
	glUnmapNamedBuffer(glInfo->text_uni_buf);

	// update text buffer
	char* buftext = (char*)glMapNamedBufferRange(glInfo->text_buf, 0, sizeof(char) * MAX_CHARS_HORIZONTAL, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
	std::copy(text, text + size, buftext);

	glFlushMappedNamedBufferRange(glInfo->text_buf, 0, sizeof(char) * MAX_CHARS_HORIZONTAL);
	glUnmapNamedBuffer(glInfo->text_buf);

	// draw!
	glDrawArrays(GL_POINTS, 0, size);

	// unbind VAO jic
	glBindVertexArray(0);
}
