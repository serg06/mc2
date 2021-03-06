#include "render.h"

#include "block.h"
#include "fbo.h"
#include "shapes.h"
#include "util.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"
//#define STB_IMAGE_WRITE_IMPLEMENTATION
//#include "stb_image_write.h"

#include <filesystem>
#include <numeric>
#include <string>
#include <tuple>
#include <vector>

constexpr int BLOCK_TEXTURE_WIDTH = 16;
constexpr int BLOCK_TEXTURE_HEIGHT = 16;
constexpr int TEXTURE_COMPONENTS = 4;
constexpr int MAX_CHARS = 256;
constexpr int BLOCKS_NUM_MIPMAPS = 3;

namespace fs = std::filesystem;
using namespace vmath;

// setup glfw window
// windowInfo -> info for setting up this window
// window -> pointer to a window pointer, which we're gonna set up
void setup_glfw(GlfwInfo* windowInfo, GLFWwindow** window) {
	if (!glfwInit()) {
		MessageBox(NULL, "Failed to initialize GLFW.", "GLFW error", MB_OK);
		return;
	}

	// OpenGL 4.6
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);

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
	//glfwSetInputMode(*window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

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
			{ "shaders/render_quads.vs.glsl", GL_VERTEX_SHADER },
			{ "shaders/render_quads.gs.glsl", GL_GEOMETRY_SHADER },
			{ "shaders/render_quads.fs.glsl", GL_FRAGMENT_SHADER },
		};

		// create program
		glInfo->game_rendering_program = compile_shaders(shader_fnames);
	}

	void setup_tjunction_fixing_program(OpenGLInfo* glInfo) {
		// list of shaders to create program with
		std::vector <std::tuple<std::string, GLenum>> shader_fnames = {
			{ "shaders/noop.vs.glsl", GL_VERTEX_SHADER },
			{ "shaders/full_screen_quad.gs.glsl", GL_GEOMETRY_SHADER },
			{ "shaders/fix_tjunctions.fs.glsl", GL_FRAGMENT_SHADER },
		};

		// create program
		glInfo->tjunction_fixing_program = compile_shaders(shader_fnames);
	}

	void setup_fbo_merging_program(OpenGLInfo* glInfo) {
		// list of shaders to create program with
		std::vector <std::tuple<std::string, GLenum>> shader_fnames = {
			{ "shaders/noop.vs.glsl", GL_VERTEX_SHADER },
			{ "shaders/full_screen_quad.gs.glsl", GL_GEOMETRY_SHADER },
			{ "shaders/merge_fbos.fs.glsl", GL_FRAGMENT_SHADER },
		};

		// create program
		glInfo->fbo_merging_program = compile_shaders(shader_fnames);
	}

	void setup_menubckgnd_program(OpenGLInfo* glInfo) {
		// list of shaders to create program with
		std::vector <std::tuple<std::string, GLenum>> shader_fnames = {
			{ "shaders/noop.vs.glsl", GL_VERTEX_SHADER },
			{ "shaders/full_screen_quad.gs.glsl", GL_GEOMETRY_SHADER },
			{ "shaders/render_bckgnd.fs.glsl", GL_FRAGMENT_SHADER },
		};

		// create program
		glInfo->menubckgnd_program = compile_shaders(shader_fnames);
	}

	void setup_opengl_vao_quad(OpenGLInfo* glInfo) {
		// vao: create VAO for Quads, so we can tell OpenGL how to use it when it's bound
		glCreateVertexArrays(1, &glInfo->vao_quad);

		// vao: enable all Quad's attributes, 1 at a time
		glEnableVertexArrayAttrib(glInfo->vao_quad, glInfo->q_block_type_attr_idx);
		glEnableVertexArrayAttrib(glInfo->vao_quad, glInfo->q_corner1_attr_idx);
		glEnableVertexArrayAttrib(glInfo->vao_quad, glInfo->q_corner2_attr_idx);
		glEnableVertexArrayAttrib(glInfo->vao_quad, glInfo->q_face_attr_idx);
		glEnableVertexArrayAttrib(glInfo->vao_quad, glInfo->q_base_coords_attr_idx);
		glEnableVertexArrayAttrib(glInfo->vao_quad, glInfo->q_lighting_attr_idx);
		glEnableVertexArrayAttrib(glInfo->vao_quad, glInfo->q_metadata_attr_idx);

		// vao: set up formats for Quad's attributes, 1 at a time
		glVertexArrayAttribIFormat(glInfo->vao_quad, glInfo->q_block_type_attr_idx, 1, GL_UNSIGNED_BYTE, offsetof(Quad3D, block));
		glVertexArrayAttribIFormat(glInfo->vao_quad, glInfo->q_corner1_attr_idx, 3, GL_INT, offsetof(Quad3D, corner1));
		glVertexArrayAttribIFormat(glInfo->vao_quad, glInfo->q_corner2_attr_idx, 3, GL_INT, offsetof(Quad3D, corner2));
		glVertexArrayAttribIFormat(glInfo->vao_quad, glInfo->q_face_attr_idx, 3, GL_INT, offsetof(Quad3D, face));
		glVertexArrayAttribIFormat(glInfo->vao_quad, glInfo->q_lighting_attr_idx, 1, GL_UNSIGNED_BYTE, offsetof(Quad3D, lighting));
		glVertexArrayAttribIFormat(glInfo->vao_quad, glInfo->q_metadata_attr_idx, 1, GL_UNSIGNED_BYTE, offsetof(Quad3D, metadata));

		glVertexArrayAttribIFormat(glInfo->vao_quad, glInfo->q_base_coords_attr_idx, 3, GL_INT, 0);

		// vao: match attributes to binding indices
		glVertexArrayAttribBinding(glInfo->vao_quad, glInfo->q_block_type_attr_idx, glInfo->quad_data_bidx);
		glVertexArrayAttribBinding(glInfo->vao_quad, glInfo->q_corner1_attr_idx, glInfo->quad_data_bidx);
		glVertexArrayAttribBinding(glInfo->vao_quad, glInfo->q_corner2_attr_idx, glInfo->quad_data_bidx);
		glVertexArrayAttribBinding(glInfo->vao_quad, glInfo->q_face_attr_idx, glInfo->quad_data_bidx);
		glVertexArrayAttribBinding(glInfo->vao_quad, glInfo->q_lighting_attr_idx, glInfo->quad_data_bidx);
		glVertexArrayAttribBinding(glInfo->vao_quad, glInfo->q_metadata_attr_idx, glInfo->quad_data_bidx);

		glVertexArrayAttribBinding(glInfo->vao_quad, glInfo->q_base_coords_attr_idx, glInfo->q_base_coords_bidx);

		// vao: extra properties
		glBindVertexArray(glInfo->vao_quad);

		// instance attribute
		glVertexAttribDivisor(glInfo->q_base_coords_attr_idx, 1);

		glBindVertexArray(0);
	}

	// set up empty (dummy) vao
	void setup_opengl_vao_empty(OpenGLInfo* glInfo) {
		glCreateVertexArrays(1, &glInfo->vao_empty);
	}

	void setup_opengl_uniforms(OpenGLInfo* glInfo) {
		// create buffers
		glCreateBuffers(1, &glInfo->trans_uni_buf);

		// bind them
		// bind transform buffer to transform uniform
		glBindBufferBase(GL_UNIFORM_BUFFER, glInfo->trans_buf_uni_bidx, glInfo->trans_uni_buf);
		// want to bind my uni buf, but I get performance warnings when I do it here... gonna do it in draw call instead

		// allocate 

		// allocate enough space for 2 transform matrices + current chunk coords + bool in_water
		// todo: use map, then invalidate range when writing
		glNamedBufferStorage(glInfo->trans_uni_buf, 2 * sizeof(mat4) + sizeof(GLuint), NULL, GL_DYNAMIC_STORAGE_BIT);
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
	template<typename T>
	void load_texture_data(const char* fname, unsigned width, unsigned height, T* result) {
		// check that file exists
		if (!fs::exists(fname)) {
			char buf[256];
			sprintf(buf, "Error: File '%s' doesn't exist!\n", fname);
			WindowsException(buf);
			exit(-1);
		}

		// load in texture from disk
		int tex_width, tex_height, tex_components;
		unsigned char *imgdata = stbi_load(fname, &tex_width, &tex_height, &tex_components, TEXTURE_COMPONENTS);

		// make sure it's correct dimensions
		if (tex_height != height || tex_width != width) {
			char buf[256];
			sprintf(buf, "Error: File '%s' has unexpected dimensions!\n", fname);
			WindowsException(buf);
			exit(-1);
		}

		if (tex_components != TEXTURE_COMPONENTS)
			OutputDebugString("Warning: pointlessly using too much data\n");

		// write result as floats
		std::copy(imgdata, imgdata + height * width * TEXTURE_COMPONENTS, result);

		// free
		stbi_image_free(imgdata);
	}


	// by using vec4s this is kinda hard-coded to have components=4. Probably should do something else.
	template<typename T>
	static const inline Tvec4<T>& get_pixel(const T* data, const unsigned width, const unsigned height, const unsigned components, const unsigned x, const unsigned y) {
		return *(Tvec4<T>*)(data + (x + y * width) * components);
	}

	template<typename T>
	static inline void set_pixel(const T* data, const unsigned width, const unsigned height, const unsigned components, const unsigned x, const unsigned y, const Tvec4<T> &pixel) {
		*(Tvec4<T>*)(data + (x + y * width) * components) = pixel;
	}

	template<typename T>
	static void gen_mipmap(T* source, T* dest, int src_width, int src_height, int components, bool normalized) {
		// make sure width & height are even
		assert(!(src_width & 0x1) && "width not even");
		assert(!(src_height & 0x1) && "height not even");

		// make sure they're > 0
		assert(src_width > 0 && "width <= 0");
		assert(src_height > 0 && "height <= 0");
		assert(components > 0 && "invalid # of components");

		T max_val = normalized ? 1.0 : (std::numeric_limits<T>::max)();

		// calculate destination width/height
		int dst_width = src_width >> 1;
		int dst_height = src_height >> 1;


		// set all pixel data
		for (int y = 0; y < dst_height; y++) {
			for (int x = 0; x < dst_width; x++) {
				Tvec4<T> pixels[4];

				// extract all the pixels that we're gonna merge int one
				for (int dy = 0; dy < 2; dy++) {
					for (int dx = 0; dx < 2; dx++) {
						pixels[dx + dy * 2] = get_pixel(source, src_width, src_height, components, x * 2 + dx, y * 2 + dy);
					}
				}

				// examine pixels
				// NOTE: Cannot use <T> for pixel sum because it'll overflow.
				dvec4 pixel_sum = dvec4(0);
				bool any_partially_translucent = false;
				int num_transparent = 0;
				for (int i = 0; i < 4; i++) {
					// record if any pixels are partially translucent (i.e. neither opaque nor transparent)
					// so far, this should only happen for water
					if (0 < pixels[i][3] && pixels[i][3] < max_val) {
						any_partially_translucent = true;
					}
					// if pixel is visible, add it to sum, allowing it to have an effect on final output pixel
					if (0 < pixels[i][3]) {
						for (int j = 0; j < 4; j++) {
							pixel_sum[j] += pixels[i][j];
						}
					}
					// count fully-transparent pixels
					else {
						num_transparent++;
					}
				}

				// output pixel
				Tvec4<T> pixel_result;

				// if all were transparent, just output an empty pixel
				if (num_transparent == 4) {
					pixel_result = Tvec4<T>(0);
				}
				// if some non-transparent ones, output linear combination of non-transparent ones
				else {
					for (int j = 0; j < 4; j++) {
						pixel_result[j] = pixel_sum[j] / (4 - num_transparent);
					}
				}

				// double check that opaque result is opaque
				if (!any_partially_translucent) {
					if (pixel_result[3] > 0) {
						pixel_result[3] = max_val;
					}
				}

				set_pixel(dest, dst_width, dst_height, components, x, y, pixel_result);
			}
		}
	}

#ifdef INCLUDE_STB_IMAGE_WRITE_H
	template<typename T>
	static void write_png(char* fname, T* data, int width, int height, int components, bool normalized) {
		int whc = width * height * components;
		unsigned char *fixed = new unsigned char[whc];
		std::transform(data, data + whc, fixed, [&normalized](auto val) { return normalized ? val * 255 : val; });
		stbi_write_png(fname, width, height, components, fixed, width * components);
		delete[] fixed;
	}
#endif

	// load texture data for a block, plus generate mipmaps up to mipmap_level
	void load_block_texture_data(const char* tex_name, unsigned char(&data)[((16 * 16) + (8 * 8) + (4 * 4) + (2 * 2) + (1 * 1)) * 4], unsigned num_mipmaps) {
		char fname[256];
		sprintf(fname, "./textures/blocks/%s.png", tex_name);
		load_texture_data(fname, 16, 16, data);

		if (num_mipmaps > 4) {
			throw "err";
		}

		/* SPECIAL CASES START */

		// grass_top.png

		// gray by default, let's color it green.
		if (strcmp(tex_name, "grass_top") == 0) {
			for (int i = 0; i < BLOCK_TEXTURE_HEIGHT * BLOCK_TEXTURE_WIDTH * TEXTURE_COMPONENTS; i++) {
				// RED
				if ((i % 4) == 0) {
					data[i] *= 0.45f;
				}

				// GREEN
				if ((i % 4) == 1) {
					data[i] *= 0.8f;
				}

				// BLUE
				if ((i % 4) == 2) {
					data[i] *= 0.3f;
				}
			}
		}

		// leaves.png

		// let's make sure non-transparent spots are 100% non-transparent
		if (strcmp(tex_name, "leaves") == 0) {
			for (int i = 0; i < BLOCK_TEXTURE_HEIGHT * BLOCK_TEXTURE_WIDTH * TEXTURE_COMPONENTS; i++) {
				// RED
				if ((i % 4) == 0) {
					data[i] *= 0.45f;
				}

				// GREEN
				if ((i % 4) == 1) {
					data[i] *= 0.6f;
				}

				// BLUE
				if ((i % 4) == 2) {
					data[i] *= 0.3f;
				}

				// ALPHA
				if ((i % 4) == 3) {
					if (data[i] > 0) {
						data[i] = 255;
					}
				}
			}
		}

		// water_square_light.png

		// let's make water_square_light lighter than default
		if (strcmp(tex_name, "water_square_light") == 0) {
			for (int i = 0; i < BLOCK_TEXTURE_HEIGHT * BLOCK_TEXTURE_WIDTH * TEXTURE_COMPONENTS; i++) {
				// ALPHA
				if ((i % 4) == 3) {
					data[i] *= 0.75f;
				}
			}
		}

		/* SPECIAL CASES END */

		// generate mipmaps
		unsigned char* prev_mipmap_location = data;
		unsigned char* this_mipmap_location = data + pown(2, 4) * pown(2, 4) * 4;
		int current_width = 8;
		int current_height = 8;

		for (int mipmap_level = 1; mipmap_level <= num_mipmaps; mipmap_level++) {
			gen_mipmap(prev_mipmap_location, this_mipmap_location, current_width * 2, current_height * 2, 4, false);

			prev_mipmap_location = this_mipmap_location;
			this_mipmap_location += current_width * current_height * 4;

			current_width >>= 1;
			current_height >>= 1;
		}
	}

	void setup_block_textures(OpenGLInfo* glInfo) {
		// data for one texture, plus all mipmaps
		unsigned char data[((16 * 16) + (8 * 8) + (4 * 4) + (2 * 2) + (1 * 1)) * 4];

		// create textures
		glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &glInfo->top_textures);
		glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &glInfo->side_textures);
		glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &glInfo->bottom_textures);

		// texture arrays
		const auto tex_arrays = { glInfo->top_textures, glInfo->side_textures, glInfo->bottom_textures };

		// allocate space
		for (auto tex_arr : tex_arrays) {
			glTextureStorage3D(tex_arr, BLOCKS_NUM_MIPMAPS + 1, GL_RGBA8, 16, 16, MAX_BLOCK_TYPES);
		}

		unsigned char* red = new unsigned char[16 * 16 * MAX_BLOCK_TYPES * 4];
		for (int i = 0; i < 16 * 16 * MAX_BLOCK_TYPES; i++) {
			((Tvec4<unsigned char>*)red)[i] = { 255, 0, 0, 255 };
		}

		// set all textures as BRIGHT RED, so we know when something's wrong
		// TODO: for all mipmap levels!
		for (auto tex_arr : tex_arrays) {
			for (int i = 0; i < BLOCKS_NUM_MIPMAPS + 1; i++) {
				glTextureSubImage3D(tex_arr,
					i,							// mipmap level
					0, 0, 0,					// Offset 0, 0, 0
					pown(2, 4 - i), pown(2, 4 - i), MAX_BLOCK_TYPES, // replace entire image for every layer
					GL_RGBA,					// Four channel data
					GL_UNSIGNED_BYTE,
					red);						// Pointer to data
			}
		}

		delete[] red;

		// write in each texture that we can
		for (int i = 0; i < MAX_BLOCK_TYPES; i++) {
			// get block
			BlockType b = BlockType(i);

			// get block's textures
			std::string top = b.top_texture();
			std::string side = b.side_texture();
			std::string bottom = b.bottom_texture();

			// if they exist, load them in
			if (top.size() > 0) {
				auto current_mipmap_start = data;
				load_block_texture_data(top.c_str(), data, BLOCKS_NUM_MIPMAPS);
				for (int mipmap_level = 0; mipmap_level <= BLOCKS_NUM_MIPMAPS; mipmap_level++) {
					int width = pown(2, 4 - mipmap_level);
					int height = pown(2, 4 - mipmap_level);
					glTextureSubImage3D(glInfo->top_textures,
						mipmap_level,								// mipmap level
						0, 0, i,									// Offset 0, 0, block_id
						width, height, 1,							// width x height x 1 texels, replace entire mipmap
						GL_RGBA,									// Four channel data
						GL_UNSIGNED_BYTE,
						current_mipmap_start);						// Pointer to mipmap
					current_mipmap_start += width * height * 4;
				}
			}

			if (side.size() > 0) {
				auto current_mipmap_start = data;
				load_block_texture_data(side.c_str(), data, BLOCKS_NUM_MIPMAPS);
				for (int mipmap_level = 0; mipmap_level <= BLOCKS_NUM_MIPMAPS; mipmap_level++) {
					int width = pown(2, 4 - mipmap_level);
					int height = pown(2, 4 - mipmap_level);
					glTextureSubImage3D(glInfo->side_textures,
						mipmap_level,								// mipmap level
						0, 0, i,									// Offset 0, 0, block_id
						width, height, 1,							// width x height x 1 texels, replace entire mipmap
						GL_RGBA,									// Four channel data
						GL_UNSIGNED_BYTE,
						current_mipmap_start);						// Pointer to mipmap
					current_mipmap_start += width * height * 4;
				}
			}

			if (bottom.size() > 0) {
				auto current_mipmap_start = data;
				load_block_texture_data(bottom.c_str(), data, BLOCKS_NUM_MIPMAPS);
				for (int mipmap_level = 0; mipmap_level <= BLOCKS_NUM_MIPMAPS; mipmap_level++) {
					int width = pown(2, 4 - mipmap_level);
					int height = pown(2, 4 - mipmap_level);
					glTextureSubImage3D(glInfo->bottom_textures,
						mipmap_level,								// mipmap level
						0, 0, i,									// Offset 0, 0, block_id
						width, height, 1,							// width x height x 1 texels, replace entire mipmap
						GL_RGBA,									// Four channel data
						GL_UNSIGNED_BYTE,
						current_mipmap_start);						// Pointer to mipmap
					current_mipmap_start += width * height * 4;
				}
			}
		}

		// for each texture array
		for (auto tex_arr : tex_arrays) {
			// wrap!
			glTextureParameteri(tex_arr, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTextureParameteri(tex_arr, GL_TEXTURE_WRAP_T, GL_REPEAT);

			// filter
			glTextureParameteri(tex_arr, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
			glTextureParameteri(tex_arr, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}

		// bind them!
		glBindTextureUnit(glInfo->top_textures_tunit, glInfo->top_textures);
		glBindTextureUnit(glInfo->side_textures_tunit, glInfo->side_textures);
		glBindTextureUnit(glInfo->bottom_textures_tunit, glInfo->bottom_textures);
	}

	void setup_gui_textures(OpenGLInfo* glInfo)
	{
		unsigned char data[(16 * 16) * 4];

		// create texture
		glCreateTextures(GL_TEXTURE_2D, 1, &glInfo->bckgnd_texture);

		// allocate space
		glTextureStorage2D(glInfo->bckgnd_texture, BLOCKS_NUM_MIPMAPS + 1, GL_RGBA8, 16, 16);

		// load texture from disk
		load_texture_data("./textures/gui/background.png", 16, 16, data);

		// load texture to GPU
		glTextureSubImage2D(
			glInfo->bckgnd_texture,
			0,							// mipmap level
			0, 0,						// Offset 0, 0
			16, 16,						// width x height
			GL_RGBA,					// Four channel data
			GL_UNSIGNED_BYTE,
			data);						// Pointer to data
		
		// wrap texture
		glTextureParameteri(glInfo->bckgnd_texture, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(glInfo->bckgnd_texture, GL_TEXTURE_WRAP_T, GL_REPEAT);

		// filter texture
		glTextureParameteri(glInfo->bckgnd_texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(glInfo->bckgnd_texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		// bind texture
		glBindTextureUnit(glInfo->bckgnd_texture_tunit, glInfo->bckgnd_texture);
	}

	void setup_fbos(GlfwInfo* windowInfo, OpenGLInfo* glInfo) {
		/*
		Setup FBO:
			- Create FBO
			- Create color texture, allocate, disable mipmaps
			- Create depth texture, allocate
			- Bind color/depth textures to FBO
			- Tell FBO to draw into its one color buffer
		*/

		// Create and initialize FBOs
		for (auto &fbo : glInfo->get_fbos()) {
			*fbo = FBO(windowInfo->width, windowInfo->height);
			fbo->update_fbo();
		}
	}
}

void setup_opengl(GlfwInfo* windowInfo, OpenGLInfo* glInfo) {
	// setup shaders
	setup_game_rendering_program(glInfo);
	setup_tjunction_fixing_program(glInfo);
	setup_fbo_merging_program(glInfo);
	setup_menubckgnd_program(glInfo);

	// setup VAOs
	setup_opengl_vao_empty(glInfo);
	setup_opengl_vao_quad(glInfo);

	// setup uniforms
	setup_opengl_uniforms(glInfo);

	// setup FBOs
	setup_fbos(windowInfo, glInfo);

	// setup extra [default] properties
	setup_opengl_extra_props(glInfo);

	// set up textures
	setup_block_textures(glInfo);
	setup_gui_textures(glInfo);
}

// fix the tjunctions in DEPTH/COLOR0 of fbo
// TODO: fbo_in instead of color/depth-in
// TODO: maybe write directly to fbo_out
void fix_tjunctions(OpenGLInfo* glInfo, GlfwInfo *windowInfo, GLuint fbo_out, FBO& fbo_in) {
	// set color/depth as inputs to tjunction fixing program
	glBindTextureUnit(glInfo->tjunc_color_in_tunit, fbo_in.get_color_buf());
	glBindTextureUnit(glInfo->tjunc_depth_in_tunit, fbo_in.get_depth_buf());

	// switch to tjunc fbo so output goes into it
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, glInfo->fbo_tjunc_fix.get_fbo());

	// clear color buffer
	//glClearBufferfv(GL_COLOR, 0, sky_blue);

	// clear depth buffer
	const GLfloat one = 1.0f;
	glClearBufferfv(GL_DEPTH, 0, &one);

	// bind program
	glBindVertexArray(glInfo->vao_empty);
	glUseProgram(glInfo->tjunction_fixing_program);

	// set uniform variables
	// TODO: probably set this in main onResize instead?
	glUniform1f(glInfo->fix_tjunc_uni_width_loc, windowInfo->width);
	glUniform1f(glInfo->fix_tjunc_uni_height_loc, windowInfo->height);

	// save properties before we overwrite them
	GLint polygon_mode; glGetIntegerv(GL_POLYGON_MODE, &polygon_mode);
	//GLint cull_face = glIsEnabled(GL_CULL_FACE);
	GLint depth_test = glIsEnabled(GL_DEPTH_TEST);

	// set properties
	glDisable(GL_DEPTH_TEST); // No reason to have it enabled
	glDisable(GL_BLEND); // DEBUG
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	// run program!
	glDrawArrays(GL_POINTS, 0, 1);

	// restore original properties
	//if (cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
	if (depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, polygon_mode);
	glEnable(GL_BLEND); // DEBUG

	// copy output from tjunction-fix fbo to output fbo
	glBlitNamedFramebuffer(glInfo->fbo_tjunc_fix.get_fbo(), fbo_out, 0, 0, windowInfo->width, windowInfo->height, 0, 0, windowInfo->width, windowInfo->height, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
}

// Readjust OpenGL viewports, FBOs, etc, based on the dimensions the window changed to.
// TODO: replace function contents with simply deleteFBOs() and setupFBOs().
void opengl_on_resize(OpenGLInfo& glInfo, int width, int height) {
	// update viewport
	glViewport(0, 0, width, height);

	// update FBOs
	for (auto &fbo : glInfo.get_fbos()) {
		fbo->set_dimensions(width, height);
		fbo->update_fbo();
	}
}

// blend fbo_out and fbo_in by drawing fbo_in on fbo_out
void merge_fbos(OpenGLInfo* glInfo, GLuint fbo_out, FBO& fbo_in) {
	// set inputs
	glBindTextureUnit(glInfo->merger_color_in_tunit, fbo_in.get_color_buf());
	glBindTextureUnit(glInfo->merger_depth_in_tunit, fbo_in.get_depth_buf());

	// switch to fbo so output goes into it
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_out);

	// bind program
	glBindVertexArray(glInfo->vao_empty);
	glUseProgram(glInfo->fbo_merging_program);

	// save properties before we overwrite them
	GLint depth_test = glIsEnabled(GL_DEPTH_TEST);
	GLint blend = glIsEnabled(GL_BLEND);
	GLint src_alpha, dst_alpha; glGetIntegerv(GL_BLEND_SRC_ALPHA, &src_alpha); glGetIntegerv(GL_BLEND_DST_ALPHA, &dst_alpha);
	GLint polygon_mode; glGetIntegerv(GL_POLYGON_MODE, &polygon_mode);

	// set properties
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
	glBlendColor(NULL, NULL, NULL, 0.6f); // not saved | 60% blend
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	// run program!
	glDrawArrays(GL_POINTS, 0, 1);

	// restore original properties
	if (depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
	if (blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
	glBlendFunc(src_alpha, dst_alpha);
	glPolygonMode(GL_FRONT_AND_BACK, polygon_mode);
}

// draw menu background to currently bound framebuffer
void draw_menubckgnd(OpenGLInfo* glInfo) {
	// clear with black (so we can draw background darker)
	GLint drawFboId; glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFboId);
	glClearBufferfv(GL_COLOR, drawFboId, color_black);

	// bind program
	glBindVertexArray(glInfo->vao_empty);
	glUseProgram(glInfo->menubckgnd_program);

	// save properties before we overwrite them
	GLint depth_test = glIsEnabled(GL_DEPTH_TEST);
	GLint blend = glIsEnabled(GL_BLEND);
	GLint src_alpha, dst_alpha; glGetIntegerv(GL_BLEND_SRC_ALPHA, &src_alpha); glGetIntegerv(GL_BLEND_DST_ALPHA, &dst_alpha);
	GLint polygon_mode; glGetIntegerv(GL_POLYGON_MODE, &polygon_mode);

	// set properties
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendColor(0, 0, 0, 0.25f);
	glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	// run program!
	glDrawArrays(GL_POINTS, 0, 1);

	// restore original properties
	if (depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
	if (blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
	glBlendFunc(src_alpha, dst_alpha);
	glPolygonMode(GL_FRONT_AND_BACK, polygon_mode);
}
