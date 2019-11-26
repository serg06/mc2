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

out vec4 vs_color;

layout (std140, binding = 0) uniform UNI_IN
{
    mat4 mv_matrix; // takes up 16 bytes
    mat4 proj_matrix; // takes up 16 bytes
} uni;

void main(void)
{
	int remainder = gl_InstanceID;
	int y = remainder / CHUNK_HEIGHT;
	remainder -= y * CHUNK_WIDTH * CHUNK_DEPTH;
	int z = remainder / CHUNK_DEPTH;
	remainder -= z * CHUNK_WIDTH;
	int x = remainder;

	vec4 instance_offset = vec4(x, y, z, 0);

	gl_Position = uni.proj_matrix * uni.mv_matrix * (position + instance_offset);
	vs_color = position * 2.0 + vec4(0.5, 0.5, 0.5, 1.0);
}
)";
	const char fshader_src[] = R"(#version 450 core

in vec4 vs_color;
out vec4 color;

void main(void)
{
	color = vs_color;
}
)";
	const char gshader_src[] = R"()";
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

// Create rotation matrix given pitch and yaw
mat4 rotate_pitch_yaw(float pitch, float yaw) {
	return
		rotate(pitch, vec3(1.0f, 0.0f, 0.0f)) * // rotate pitch around X
		rotate(yaw, vec3(0.0f, 1.0f, 0.0f));    // rotate yaw   around Y
}

