#include "util.h"

#include <assert.h>
#include <fstream>
#include <functional>
#include <iterator> 
#include <tuple> 
#include <vector>

constexpr long SEED = 5370157038;

using namespace std;
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

GLuint link_program(const GLuint program) {
	// link program w/ error-checking

	glLinkProgram(program); // link together all attached shaders

	// CHECK IF LINKING SUCCESSFUL
	GLint status = GL_TRUE;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)
	{
		GLint logLen;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
		std::vector<char> log(logLen);
		GLsizei written;
		glGetProgramInfoLog(program, logLen, &written, log.data());

		OutputDebugString("linking error with program:\n\n");
		OutputDebugString(log.data());
		OutputDebugString("\n");
		exit(1);
	}

	return program;
}

GLuint compile_shaders(const std::vector<std::tuple<std::string, GLenum>>& shader_fnames) {
	GLuint program;
	std::vector<GLuint> shaders; // store compiled shaders

	// for each input shader
	for (const auto&[fname, shadertype] : shader_fnames)
	{
		// load shader src
		std::ifstream shader_file(fname);

		if (!shader_file.is_open()) {
			char buf[1024];
			sprintf(buf, "could not open shader file: %s\n", fname.c_str());
			WindowsException(buf);
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

// create a (deterministic) 2D random gradient
vec2 rand_grad(int seed, int x, int y) {
	vec2 result;
	auto h = hash<int>();
	srand(h(seed) ^ h(x) ^ h(y));

	for (int i = 0; i < result.size(); i++) {
		result[i] = ((float)rand() / (float)RAND_MAX);
	}

	result = normalize(result);

	return result;
}

// apply ease curve to p, which should be a number btwn 0 and 1
// p -> 3p^2 - 2p^3
double ease(double p) {
	assert(0 <= p && p <= 1 && "Invalid p value.");
	return 3 * pow(p, 2) - 2 * pow(p, 3);
}

// calculate weighted avg
double weighted_avg(double point, double left, double right) {
	assert(0 <= point && point <= 1 && "Invalid p value 2.");

	return left + (right - left)*point;
}

// perlin function gives us y value at (x,z).
// for now call it (x,y) for math-clarity
double noise2d(float x, float y) {
	// get coordinates
	int xMin = (int)floorf(x);
	int xMax = (int)ceilf(x);
	int yMin = (int)floorf(y);
	int yMax = (int)ceilf(y);

	ivec2 coords[4] = {
		{ xMin, yMin },
		{ xMax, yMin },
		{ xMin, yMax },
		{ xMax, yMax },
	};

	int len = sizeof(coords) / sizeof(coords[0]);

	// get "random" gradient for each coordinate
	vec2 grads[4];

	for (int i = 0; i < len; i++) {
		grads[i] = rand_grad(SEED, coords[i][0], coords[i][1]);
	}

	// generate vectors from coordinates to (x,y)
	vec2 vecs_to_xy[4];

	for (int i = 0; i < len; i++) {
		vecs_to_xy[i] = vec2(x, y) - vec2((float)coords[i][0], (float)coords[i][1]);
	}

	// dot product vec_to_xy with grad
	float dots[4];

	for (int i = 0; i < len; i++) {
		dots[i] = dot(grads[i], vecs_to_xy[i]);
	}

	// get x-dimension weight, Sx = ease(x-x0)
	double Sx = ease(x - xMin);

	// apply weighted avg along top corners (where both have y=yMin), and along bottom corners (where both have y=yMax)
	double a = weighted_avg(Sx, dots[0], dots[1]);
	double b = weighted_avg(Sx, dots[2], dots[3]);

	// get y-dimension weight, Sy = ease(y-y0)
	double Sy = ease(y - yMin);

	// apply weighted avg using our two previous weighted avgs
	double z = weighted_avg(Sy, a, b);

	z = (z + 1.0) / 2.0;
	assert(z >= 0.0 && z <= 1.0);

	return z;
}

