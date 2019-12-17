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
	void setup_opengl_program(OpenGLInfo* glInfo) {
		// list of shaders to create program with
		// TODO: Embed these into binary somehow - maybe generate header file with cmake.
		std::vector <std::tuple<std::string, GLenum>> shader_fnames = {
			{ "../src/simple.vs.glsl", GL_VERTEX_SHADER },
			//{"../src/simple.tcs.glsl", GL_TESS_CONTROL_SHADER },
			//{"../src/simple.tes.glsl", GL_TESS_EVALUATION_SHADER },
			//{ "../src/simple.gs.glsl", GL_GEOMETRY_SHADER },
			{ "../src/simple.fs.glsl", GL_FRAGMENT_SHADER },
		};

		// create program
		glInfo->rendering_program = compile_shaders(shader_fnames);

		// use our program object for rendering
		glUseProgram(glInfo->rendering_program);
	}

	void setup_gen_layer_program(OpenGLInfo* glInfo) {
		// list of shaders to create program with
		// TODO: Embed these into binary somehow - maybe generate header file with cmake.
		std::vector <std::tuple<std::string, GLenum>> shader_fnames = {
			{ "../src/gen_layer.cs.glsl", GL_COMPUTE_SHADER},
		};

		// create program
		glInfo->gen_layer_program = compile_shaders(shader_fnames);
	}

	void setup_gen_quads_program(OpenGLInfo* glInfo) {
		// list of shaders to create program with
		// TODO: Embed these into binary somehow - maybe generate header file with cmake.
		std::vector <std::tuple<std::string, GLenum>> shader_fnames = {
			{ "../src/gen_quads.cs.glsl", GL_COMPUTE_SHADER },
		};

		// create program
		glInfo->gen_quads_program = compile_shaders(shader_fnames);

		// create and bind atomic counter buff
		glUseProgram(glInfo->gen_quads_program);

		glCreateBuffers(1, &glInfo->gen_quads_atomic_buf);
		glNamedBufferStorage(glInfo->gen_quads_atomic_buf, sizeof(GLuint), NULL, GL_DYNAMIC_STORAGE_BIT | GL_CLIENT_STORAGE_BIT);
		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, glInfo->gen_quads_atomic_abidx, glInfo->gen_quads_atomic_buf);

		glCreateBuffers(1, &glInfo->gen_quads_atomic_buf_tmp);
		glNamedBufferStorage(glInfo->gen_quads_atomic_buf_tmp, sizeof(GLuint), NULL, GL_DYNAMIC_STORAGE_BIT | GL_CLIENT_STORAGE_BIT);
	}

	void setup_gen_layers_and_quads_program(OpenGLInfo* glInfo) {
		// list of shaders to create program with
		// TODO: Embed these into binary somehow - maybe generate header file with cmake.
		std::vector <std::tuple<std::string, GLenum>> shader_fnames = {
			{ "../src/gen_layers_quads.cs.glsl", GL_COMPUTE_SHADER },
		};

		// create program
		glInfo->gen_layers_and_quads_program = compile_shaders(shader_fnames);

		// everything else is already made in other setup functions!
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

		// vao: set up formats for cube's attributes, 1 at a time
		glVertexArrayAttribIFormat(glInfo->vao_quad, glInfo->q_block_type_attr_idx, 1, GL_BYTE, 0);
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

	void setup_opengl_uniforms(OpenGLInfo* glInfo) {
		// create buffers
		glCreateBuffers(1, &glInfo->trans_buf);

		// bind them
		// bind transform buffer to transform uniform
		glBindBufferBase(GL_UNIFORM_BUFFER, glInfo->trans_buf_uni_bidx, glInfo->trans_buf); 

		// allocate 
		// allocate enough space for 2 transform matrices + current chunk coords + bool in_water
		glNamedBufferStorage(glInfo->trans_buf, sizeof(mat4) * 2 + sizeof(ivec4) + sizeof(GLuint), NULL, GL_DYNAMIC_STORAGE_BIT);
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

	void setup_opengl_storage_blocks(OpenGLInfo* glInfo) {
		// size of buffer we'll need
		// TODO: fill 'er up with bytes instead of uints
		GLuint mini_bufsize = 16 * 16 * 16 * sizeof(unsigned) * (256 * 16); // 256 chunks / 4096 minis
		GLuint layers_bufsize = 16 * 16 * (16 * 6) * sizeof(unsigned) * (256 * 16); // 256 chunks / 4096 minis
		GLuint quads2d_bufsize = 16 * 16 * ((16 + 1) * 3) * sizeof(unsigned) * (64 * 16); // 16 chunks / 256 minis
		GLuint quads3d_bufsize = 16 * 16 * ((16 + 1) * 3) * sizeof(unsigned) * (64 * 16); // no clue if this size is good enough
		GLuint mini_coords_bufsize = sizeof(ivec4) * (1024 * 16); // 1024 chunks / 16384 minis

		// TODO: adjust storage bits

		// mini buffer
		glCreateBuffers(1, &glInfo->gen_layer_mini_buf);
		glNamedBufferStorage(glInfo->gen_layer_mini_buf, mini_bufsize, NULL, GL_DYNAMIC_STORAGE_BIT);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, glInfo->gen_layer_mini_ssbidx, glInfo->gen_layer_mini_buf);

		// layers buffer
		glCreateBuffers(1, &glInfo->gen_layer_layers_buf);
		glNamedBufferStorage(glInfo->gen_layer_layers_buf, layers_bufsize, NULL, GL_DYNAMIC_STORAGE_BIT);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, glInfo->gen_layer_layers_ssbidx, glInfo->gen_layer_layers_buf);

		// quads2d buffer
		glCreateBuffers(1, &glInfo->gen_quads_quads2d_buf);
		glNamedBufferStorage(glInfo->gen_quads_quads2d_buf, quads2d_bufsize, NULL, GL_DYNAMIC_STORAGE_BIT | GL_CLIENT_STORAGE_BIT);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, glInfo->gen_quads_quads2d_ssbidx, glInfo->gen_quads_quads2d_buf);

		// quads3d buffer
		glCreateBuffers(1, &glInfo->gen_quads_quads3d_buf);
		glNamedBufferStorage(glInfo->gen_quads_quads3d_buf, quads3d_bufsize, NULL, GL_DYNAMIC_STORAGE_BIT | GL_CLIENT_STORAGE_BIT);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, glInfo->gen_quads_quads3d_ssbidx, glInfo->gen_quads_quads3d_buf);

		// mini coords buffer
		glCreateBuffers(1, &glInfo->gen_layer_mini_coords_buf);
		glNamedBufferStorage(glInfo->gen_layer_mini_coords_buf, mini_coords_bufsize, NULL, GL_DYNAMIC_STORAGE_BIT);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, glInfo->gen_layer_mini_coords_ssbidx, glInfo->gen_layer_mini_coords_buf);
	}
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

// load texture data for a block
void load_block_texture_data(const char* tex_name, float(&data)[16 * 16 * 4]) {
	char fname[256];
	sprintf(fname, "./textures/blocks/%s.png", tex_name);
	load_texture_data(fname, 16, 16, data);

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
}

void setup_texture_arrays(OpenGLInfo* glInfo) {
	// data for one texture
	float data[16 * 16 * 4];

	// create textures
	glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &glInfo->top_textures);
	glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &glInfo->side_textures);
	glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &glInfo->bottom_textures);

	auto tex_arrays = { glInfo->top_textures, glInfo->side_textures, glInfo->bottom_textures };

	// allocate space (32-bit float RGBA 16x16) (TODO: GL_RGBA8 instead.)
	for (auto tex_arr : tex_arrays) {
		glTextureStorage3D(tex_arr, 1, GL_RGBA32F, 16, 16, MAX_BLOCK_TYPES);
	}

	float* red = new float[16 * 16 * MAX_BLOCK_TYPES * 4];
	for (int i = 0; i < 16 * 16 * MAX_BLOCK_TYPES; i++) {
		red[i * 4 + 0] = 1.0f; // R
		red[i * 4 + 1] = 0.0f; // G
		red[i * 4 + 2] = 0.0f; // B
		red[i * 4 + 3] = 1.0f; // A
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
			load_block_texture_data(top.c_str(), data);
			glTextureSubImage3D(glInfo->top_textures,
				0,			// Level 0
				0, 0, i,	// Offset 0, 0, block_id
				16, 16, 1,	// 16 x 16 x 1 texels, replace one texture
				GL_RGBA,	// Four channel data
				GL_FLOAT,	// Floating point data
				data);		// Pointer to data
		}

		if (side.size() > 0) {
			load_block_texture_data(side.c_str(), data);
			glTextureSubImage3D(glInfo->side_textures,
				0,			// Level 0
				0, 0, i,	// Offset 0, 0, block_id
				16, 16, 1,	// 16 x 16 x 1 texels, replace one texture
				GL_RGBA,	// Four channel data
				GL_FLOAT,	// Floating point data
				data);		// Pointer to data
		}

		if (bottom.size() > 0) {
			load_block_texture_data(bottom.c_str(), data);
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
		// wrap!
		glTextureParameteri(tex_arr, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(tex_arr, GL_TEXTURE_WRAP_T, GL_REPEAT);

		// filter
		glTextureParameteri(tex_arr, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(tex_arr, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	// bind them!
	glBindTextureUnit(2, glInfo->top_textures);
	glBindTextureUnit(3, glInfo->side_textures);
	glBindTextureUnit(4, glInfo->bottom_textures);
}

// create texture object, fill it with block texture, store it in texture object
void load_block_texture(GLuint* texture, const char* tex_name) {
	// create texture
	glCreateTextures(GL_TEXTURE_2D, 1, texture);

	// allocate space (32-bit float RGBA 16x16)
	glTextureStorage2D(*texture, 1, GL_RGBA32F, 16, 16);

	float data[16 * 16 * 4];
	load_block_texture_data(tex_name, data);

	// load data into texture
	glTextureSubImage2D(*texture,
		0,              // Level 0
		0, 0,           // Offset 0, 0
		16, 16,         // 16 x 16 texels, replace entire image
		GL_RGBA,        // Four channel data
		GL_FLOAT,       // Floating point data
		data);          // Pointer to data

	// wrap!
	glTextureParameteri(*texture, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(*texture, GL_TEXTURE_WRAP_T, GL_REPEAT);

	// filter
	glTextureParameteri(*texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(*texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void setup_block_textures(OpenGLInfo* glInfo) {
	int width, height, components;
	unsigned char *imgdata;
	float data[16 * 16 * 4];

	/* GRASS TOP & SIDE TEXTURES */

	// create textures
	load_block_texture(&glInfo->grass_top, Block(Block::Grass).top_texture().c_str());
	load_block_texture(&glInfo->grass_side, Block(Block::Grass).side_texture().c_str());

	// bind
	glBindTextureUnit(0, glInfo->grass_top);
	glBindTextureUnit(1, glInfo->grass_side);
}



void setup_opengl(OpenGLInfo* glInfo) {
	// setup shaders
	setup_opengl_program(glInfo);
	setup_gen_layer_program(glInfo);
	setup_gen_quads_program(glInfo);
	setup_gen_layers_and_quads_program(glInfo);

	// setup VAOs
	//setup_opengl_vao_cube(glInfo);
	setup_opengl_vao_quad(glInfo);

	// setup uniforms
	setup_opengl_uniforms(glInfo);

	// setup ssbs
	setup_opengl_storage_blocks(glInfo);

	// setup extra [default] properties
	setup_opengl_extra_props(glInfo);

	// set up textures
	setup_block_textures(glInfo);
	setup_texture_arrays(glInfo);
}
