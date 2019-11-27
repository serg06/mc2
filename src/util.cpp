#include "util.h"

#include "GL/gl3w.h"
#include "GLFW/glfw3.h"

#include <fstream>
#include <iterator> 
#include <tuple> 
#include <vector> 
#include <vmath.h>

using namespace vmath;

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

// Temporary workaround to inability to package shader sources with executable.
namespace {
	// Fill these with actual sources before running.
	const char vshader_src[] = R"(#version 450 core

#define CHUNK_WIDTH 16
#define CHUNK_DEPTH 16
#define CHUNK_HEIGHT 256
#define CHUNK_SIZE (CHUNK_WIDTH * CHUNK_DEPTH * CHUNK_HEIGHT)

layout (location = 0) in vec4 position;
layout (location = 1) in uint block_type; // fed in via instance array!

out vec4 vs_color;
out uint vs_block_type;

layout (std140, binding = 0) uniform UNI_IN
{
	mat4 mv_matrix; // takes up 16 bytes
	mat4 proj_matrix; // takes up 16 bytes
} uni;

float rand(float seed) {
	return fract(1.610612741 * seed);
}

// shader starts executing here
void main(void)
{
	vs_block_type = block_type;

	// TODO: Add chunk offset

	// Given gl_InstanceID, calculate 3D coordinate relative to chunk origin
	int remainder = gl_InstanceID;
	int y = remainder / CHUNK_HEIGHT;
	remainder -= y * CHUNK_WIDTH * CHUNK_DEPTH;
	int z = remainder / CHUNK_DEPTH;
	remainder -= z * CHUNK_WIDTH;
	int x = remainder;

	/* CREATE OUR OFFSET VARIABLE */

	vec4 instance_offset = vec4(x, y, z, 0);

	/* ADD IT TO VERTEX */

	//gl_Position = uni.proj_matrix * uni.mv_matrix * position;
	gl_Position = uni.proj_matrix * uni.mv_matrix * (position + instance_offset);
	vs_color = position * 2.0 + vec4(0.5, 0.5, 0.5, 1.0);

	int seed = gl_VertexID * gl_InstanceID;
	switch(block_type) {
		case 0: // air (just has a color for debugging purposes)
			vs_color = vec4(0.7, 0.7, 0.7, 1.0);
			break;
		case 1: // grass
			vs_color = vec4(0.2, 0.8 + rand(seed) * 0.2, 0.0, 1.0); // green
			break;
		case 2: // stone
			vs_color = vec4(0.4, 0.4, 0.4, 1.0) + vec4(rand(seed), rand(seed), rand(seed), rand(seed))*0.2; // grey
			break;
		default:
			vs_color = vec4(1.0, 0.0, 1.0, 1.0); // SUPER NOTICEABLE (for debugging)
			break;
	}

	// if top corner, make it darker!
	if (position.y > 0) {
		vs_color /= 2;
	}

}
)";
	const char fshader_src[] = R"(#version 450 core

in vec4 gs_color;
out vec4 color;

void main(void)
{
	color = gs_color;
}
)";
	const char gshader_src[] = R"(#version 450 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

in uint vs_block_type[];
in vec4 vs_color[];

out vec4 gs_color;

void main(void)
{
	// don't draw air
	if (vs_block_type[0] == 0) {
		return;
	}

	for (int i = 0; i < 3; i++) {		
		gl_Position = gl_in[i].gl_Position;
		gs_color = vs_color[i];
		EmitVertex();
	}
}
)";
	const char tcsshader_src[] = R"()";
	const char tesshader_src[] = R"()";

	GLuint compile_shaders_hardcoded(std::vector <std::tuple<std::string, GLenum>> shader_fnames) {
		GLuint program;
		std::vector <GLuint> shaders; // store compiled shaders

		// for each input shader
		for (const auto&[fname, shadertype] : shader_fnames)
		{
			// get src
			const GLchar * shader_src_ptr;
			
			switch (shadertype) {
			case GL_VERTEX_SHADER:
				if (vshader_src[0] == '\0') {
					throw "missing vshader source";
				}
				shader_src_ptr = vshader_src;
				break;
			case GL_FRAGMENT_SHADER:
				if (fshader_src[0] == '\0') {
					throw "missing fshader source";
				}
				shader_src_ptr = fshader_src;
				break;
			case GL_GEOMETRY_SHADER:
				if (gshader_src[0] == '\0') {
					throw "missing fshader source";
				}
				shader_src_ptr = gshader_src;
				break;
			default:
				throw "missing a shader source";
			}

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

			// Save shader for later
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
}

GLuint link_program(GLuint program) {
	// link program w/ error-checking

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

	return program;
}

GLuint compile_shaders(std::vector <std::tuple<std::string, GLenum>> shader_fnames) {
	//return compile_shaders_hardcoded(shader_fnames);

	GLuint program;
	std::vector <GLuint> shaders; // store compiled shaders

	// for each input shader
	for (const auto&[fname, shadertype] : shader_fnames)
	{
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

	// link program
	program = link_program(program);

	// Delete the shaders as the program has them now
	for (const GLuint &shader : shaders) {
		glDeleteShader(shader);
	}

	return program;
}



