/*
 * Copyright © 2012-2015 Graham Sellers
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * FROM NOW ON, THIS IS MYYYYY PROJECT
 * Stopped on page: 24: Drawing Our First Triangle.
 */

#include <sb7.h>

#include <iostream>
#include <string>
#include <fstream>
#include <streambuf>
#include <list> 
#include <vector> 
#include <iterator> 
#include <tuple> 
#include <vmath.h>
#include <math.h>


#define M_PI 3.14159265358979323846

class simpleclear_app : public sb7::application
{
public:
	void init()
	{
		static const char title[] = "OpenGL SuperBible - Simple Clear";

		sb7::application::init();

		memcpy(info.title, title, sizeof(title));
	}

	void startup() {
		// Cube at origin
		static const GLfloat vertex_positions[] =
		{
			-0.25f, 0.25f, -0.25f,
			-0.25f, -0.25f, -0.25f,
			0.25f, -0.25f, -0.25f,

			0.25f, -0.25f, -0.25f,
			0.25f, 0.25f, -0.25f,
			-0.25f, 0.25f, -0.25f,

			0.25f, -0.25f, -0.25f,
			0.25f, -0.25f, 0.25f,
			0.25f, 0.25f, -0.25f,

			0.25f, -0.25f, 0.25f,
			0.25f, 0.25f, 0.25f,
			0.25f, 0.25f, -0.25f,

			0.25f, -0.25f, 0.25f,
			-0.25f, -0.25f, 0.25f,
			0.25f, 0.25f, 0.25f,

			-0.25f, -0.25f, 0.25f,
			-0.25f, 0.25f, 0.25f,
			0.25f, 0.25f, 0.25f,

			-0.25f, -0.25f, 0.25f,
			-0.25f, -0.25f, -0.25f,
			-0.25f, 0.25f, 0.25f,

			-0.25f, -0.25f, -0.25f,
			-0.25f, 0.25f, -0.25f,
			-0.25f, 0.25f, 0.25f,

			-0.25f, -0.25f, 0.25f,
			0.25f, -0.25f, 0.25f,
			0.25f, -0.25f, -0.25f,

			0.25f, -0.25f, -0.25f,
			-0.25f, -0.25f, -0.25f,
			-0.25f, -0.25f, 0.25f,

			-0.25f, 0.25f, -0.25f,
			0.25f, 0.25f, -0.25f,
			0.25f, 0.25f, 0.25f,

			0.25f, 0.25f, 0.25f,
			-0.25f, 0.25f, 0.25f,
			-0.25f, 0.25f, -0.25f
		};

		// placeholder
		glCreateVertexArrays(1, &vao);
		glBindVertexArray(vao);

		// compile shaders
		rendering_program = compile_shaders();


		/*
		 * CUBE MOVEMENT
		 */

		// Simple projection matrix
		vmath::mat4 proj_matrix = vmath::perspective(
			40.0f, // 59.0 vfov = 90.0 hfov
			800.0f / 600.0f,  // aspect ratio - not sure if right
			0.1f,  // can't see behind 0.0 anyways
			-1000.0f // our object will be closer than 100.0
			);

		const GLuint uni_binding_idx = 0;
		const GLuint attrib_idx = 0;
		const GLuint vert_binding_idx = 0;

		// create buffers
		glCreateBuffers(1, &trans_buf);
		glCreateBuffers(1, &vert_buf);

		// bind them
		glBindBufferBase(GL_UNIFORM_BUFFER, uni_binding_idx, trans_buf); // bind trans buf to uniform buffer binding point
		glBindBuffer(GL_ARRAY_BUFFER, vert_buf); // why?
		glVertexArrayAttribBinding(vao, attrib_idx, vert_binding_idx); // connect vert -> position variable

		// allocate
		glNamedBufferStorage(trans_buf, 2 * sizeof(vmath::mat4), NULL, GL_DYNAMIC_STORAGE_BIT); // allocate 2 matrices of space for transforms, and allow editing
		glNamedBufferStorage(vert_buf, sizeof(vertex_positions), NULL, GL_DYNAMIC_STORAGE_BIT); // allocate enough for all vertices, and allow editing

		// insert data (skip model-view matrix; we'll update it in render())
		glNamedBufferSubData(trans_buf, sizeof(vmath::mat4), sizeof(vmath::mat4), proj_matrix); // proj matrix
		glNamedBufferSubData(vert_buf, 0, sizeof(vertex_positions), vertex_positions); // vertex positions

		// enable auto-filling of position
		glVertexArrayVertexBuffer(vao, vert_binding_idx, vert_buf, 0, sizeof(vmath::vec3));
		glVertexArrayAttribFormat(vao, attrib_idx, 3, GL_FLOAT, GL_FALSE, 0);
		glEnableVertexAttribArray(attrib_idx);


		/*
		 * MINECRAFT TEXTURE
		 */

		// TODO, maybe - looks kinda hard tbh

		//// create texture
		//GLuint texGrass;
		//glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &texGrass);

		//// allocate 6 sides
		//glTexStorage2D(GL_TEXTURE_CUBE_MAP, 1, GL_RGBA32F, 16, 16);

		//// bind them
		//glBindTexture(GL_TEXTURE_CUBE_MAP, texGrass);


		/*
		 * ETC
		 */

		glPointSize(5.0f);
		//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glEnable(GL_CULL_FACE);
		glFrontFace(GL_CW);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);

		// use our program object for rendering
		glUseProgram(rendering_program);

	}

	void shutdown() {
		// delete it all!
		glDeleteVertexArrays(1, &vao);
		glDeleteProgram(rendering_program);
		glDeleteVertexArrays(1, &vao); // why twice?
	}

	// execute shaders and actually draw on screen!
	virtual void render(double currentTime) {
		// BACKGROUND COLOUR

		//const GLfloat color[] = { (float)sin(currentTime) * 0.5f + 0.5f, (float)cos(currentTime) * 0.5f + 0.5f, 0.0f, 1.0f };
		const GLfloat color[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		glClearBufferfv(GL_COLOR, 0, color);
		glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0); // used for depth test somehow

		// MOVEMENT CALCULATION

		float f = (float)currentTime * (float)M_PI * 0.1f;
		vmath::mat4 model_view_matrix =
			vmath::translate(0.0f, 0.0f, -4.0f) *
			vmath::translate(sinf(2.1f * f) * 0.5f,
			cosf(1.7f * f) * 0.5f,
			sinf(1.3f * f) * cosf(1.5f * f) * 2.0f) *
			vmath::rotate((float)currentTime * 45.0f, 0.0f, 1.0f, 0.0f) *
			vmath::rotate((float)currentTime * 81.0f, 1.0f, 0.0f, 0.0f);

		// Update transformation buffer with mv matrix
		glNamedBufferSubData(trans_buf, 0, sizeof(model_view_matrix), model_view_matrix);

		// DRAWING

		glDrawArrays(GL_TRIANGLES, 0, 36); // draw triangle using 3 VAOs, starting at the 0th one (our only one!)

	}

	void print_arr(const GLfloat *arr, int size, int row_size) {
		char str[64];

		OutputDebugString("\nPRINTING ARR:\n");

		for (int i = 0; i < size; i++) {
			memset(str, '\0', 64);
			if (arr[i] >= 0) {
				str[0] = ' ';
				sprintf(str + 1, "%.2f ", arr[i]);
			}
			else {
				sprintf(str, "%.2f ", arr[i]);
			}
			OutputDebugString(str);

			if (((i + 1) % row_size) == 0) {
				OutputDebugString("\n");
			}
		}

		OutputDebugString("\nDONE\n");
	}

private:
	GLuint rendering_program;
	GLuint vao;
	GLuint trans_buf;
	GLuint vert_buf;

	GLuint compile_shaders(void) {
		GLuint program;
		std::list <std::tuple<std::string, GLenum>> shader_fnames;
		std::list <GLuint> shaders; // store compiled shaders

		// list of shader names to include in program
		shader_fnames.push_back(std::make_tuple("../src/simpleclear/simple.vs.glsl", GL_VERTEX_SHADER));
		//shader_fnames.push_back(std::make_tuple("../src/simpleclear/simple.tcs.glsl", GL_TESS_CONTROL_SHADER));
		//shader_fnames.push_back(std::make_tuple("../src/simpleclear/simple.tes.glsl", GL_TESS_EVALUATION_SHADER));
		//shader_fnames.push_back(std::make_tuple("../src/simpleclear/simple.gs.glsl", GL_GEOMETRY_SHADER));
		shader_fnames.push_back(std::make_tuple("../src/simpleclear/simple.fs.glsl", GL_FRAGMENT_SHADER));

		// for each input shader
		for (const std::tuple <std::string, GLenum> &shader_fname : shader_fnames)
		{
			// extract shader info
			const std::string fname = std::get<0>(shader_fname);
			const GLenum shadertype = std::get<1>(shader_fname);

			// load shader src
			std::ifstream shader_file(fname);

			if (!shader_file.is_open()) {
				OutputDebugString("could not open shader file: ");
				OutputDebugString(fname.c_str());
				OutputDebugString("\n");
				exit(1);
			}

			const std::string shader_src((std::istreambuf_iterator<char>(shader_file)), std::istreambuf_iterator<char>());
			const GLchar * shader_src_ptr = shader_src.c_str();

			// Create and compile shader
			const GLuint shader = glCreateShader(shadertype); // create empty shader
			glShaderSource(shader, 1, &shader_src_ptr, NULL); // set shader source code
			glCompileShader(shader); // compile shader

			// CHECK IF COMPILATION SUCCESSFUL
			GLint status = GL_TRUE;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
			if (status == GL_FALSE)
			{
				GLint logLen;
				glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
				std::vector <char> log(logLen);
				GLsizei written;
				glGetShaderInfoLog(shader, logLen, &written, log.data());

				OutputDebugString("compilation error with shader ");
				OutputDebugString(fname.c_str());
				OutputDebugString(":\n\n");
				OutputDebugString(log.data());
				OutputDebugString("\n");
				exit(1);
			}

			// Close file, save shader for later
			shader_file.close();
			shaders.push_back(shader);
		}

		// Create program, attach shaders to it, and link it
		program = glCreateProgram(); // create (empty?) program

		// attach shaders
		for (const GLuint &shader : shaders) {
			glAttachShader(program, shader);
		}

		glLinkProgram(program); // link together all attached shaders

		// CHECK IF LINKING SUCCESSFUL
		GLint status = GL_TRUE;
		glGetProgramiv(program, GL_LINK_STATUS, &status);
		if (status == GL_FALSE)
		{
			GLint logLen;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
			std::vector <char> log(logLen);
			GLsizei written;
			glGetProgramInfoLog(program, logLen, &written, log.data());

			OutputDebugString("linking error with program:\n\n");
			OutputDebugString(log.data());
			OutputDebugString("\n");
			exit(1);
		}

		// Delete the shaders as the program has them now
		for (const GLuint &shader : shaders) {
			glDeleteShader(shader);
		}

		return program;
	}

	std::list <std::ifstream> wew(void) {
		// open all our shader files and return them in a list
	}
};

DECLARE_MAIN(simpleclear_app)
