#include "util.h"

#include "GL/gl3w.h"
#include "GLFW/glfw3.h"

#include <fstream>
#include <iterator> 
#include <tuple> 
#include <vector> 
#include <vmath.h>



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

GLuint compile_shaders(std::vector <std::tuple<std::string, GLenum>> shader_fnames) {
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
vmath::mat4 rotate_pitch_yaw(float pitch, float yaw) {
	return
		vmath::rotate(pitch, vmath::vec3(1.0f, 0.0f, 0.0f)) * // rotate pitch around X
		vmath::rotate(yaw, vmath::vec3(0.0f, 1.0f, 0.0f));    // rotate yaw   around Y
}
