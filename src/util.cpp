#include "util.h"

#include "GL/gl3w.h"

#include <cassert>
#include <fstream>
#include <functional>
#include <iterator> 
#include <tuple> 
#include <vector>

constexpr long SEED = 5370157038;

using namespace std;
using namespace vmath;

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

GLuint compile_shaders(const std::vector<std::tuple<std::string, GLenum>>& shader_fnames)
{
	GLuint program;
	std::vector<GLuint> shaders; // store compiled shaders

	// for each input shader
	for (const auto& [fname, shadertype] : shader_fnames)
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
		const GLchar* shader_src_ptr = shader_src.c_str();

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
	for (const GLuint& shader : shaders) {
		glAttachShader(program, shader);
	}

	// link program
	program = link_program(program);

	// Delete the shaders as the program has them now
	for (const GLuint& shader : shaders) {
		glDeleteShader(shader);
	}

	return program;
}

// Create rotation matrix given pitch and yaw
vmath::mat4 rotate_pitch_yaw(float pitch, float yaw)
{
	return
		rotate(pitch, vmath::vec3(1.0f, 0.0f, 0.0f)) * // rotate pitch around X
		rotate(yaw, vmath::vec3(0.0f, 1.0f, 0.0f));    // rotate yaw   around Y
}

// extract planes from projection matrix
void extract_planes_from_projmat(const vmath::mat4& proj_mat, const vmath::mat4& mv_mat, vmath::vec4(&planes)[6])
{
	const vmath::mat4& mat = (proj_mat * mv_mat).transpose();

	planes[0] = mat[3] + mat[0];
	planes[1] = mat[3] - mat[0];
	planes[2] = mat[3] + mat[1];
	planes[3] = mat[3] - mat[1];
	planes[4] = mat[3] + mat[2];
	planes[5] = mat[3] - mat[2];
}

// check if sphere is inside frustum planes
bool sphere_in_frustum(const vmath::vec3& pos, const float radius, const vmath::vec4(&frustum_planes)[6])
{
	bool res = true;
	for (auto& plane : frustum_planes) {
		if (plane[0] * pos[0] + plane[1] * pos[1] + plane[2] * pos[2] + plane[3] <= -radius) {
			res = false;
		}
	}

	return res;
}

void WindowsException(const char* description)
{
	MessageBox(NULL, description, "Thrown exception", MB_OK);
}


// generate all points in a circle a center
// TODO: cache
// TODO: return std::array using -> to specify return type?
std::vector<vmath::ivec2> gen_circle(const int radius, const vmath::ivec2 center) {
	std::vector<vmath::ivec2> result;
	result.reserve(4 * radius * radius + 4 * radius + 1); // always makes <= (2r+1)^2 = 4r^2 + 4r + 1 elements

	CircleGenerator cg(radius);
	for (auto iter = cg.begin(); iter != cg.end(); ++iter) {
		result.push_back(*iter + center);
	}

	return result;
}

std::vector<vmath::ivec2> gen_diamond(const int radius, const vmath::ivec2 center) {
	std::vector<vmath::ivec2> result(2 * radius * radius + 2 * radius + 1); // always makes 2r^2 + 2r + 1 elements

	int total_idx = 0;
	for (int i = -radius; i <= radius; i++) {
		for (int j = -(radius - abs(i)); j <= radius - abs(i); j++) {
			result[total_idx++] = center + vmath::ivec2(i, j);
		}
	}
	return result;
}

// extract a piece of a texture atlas
// assuming uniform atlas -- i.e. every texture is the same size
//
// atlas_width		:	number of textures stored horizontally
// atlas_height		:	number of textures stored vertically
// tex_width		:	width  of each texture in pixels
// tex_height		:	height of each texture in pixels
// idx				:	index of texture piece in atlas
void extract_from_atlas(float* atlas, unsigned atlas_width, unsigned atlas_height, unsigned components, unsigned tex_width, unsigned tex_height, unsigned idx, float* result) {
	// coordinates of top-left texture texel in the atlas
	unsigned start_x = (idx % atlas_width) * tex_width;
	unsigned start_y = (idx / atlas_width) * tex_height;

	// boom baby!
	for (int y = 0; y < tex_height; y++) {
		for (int x = 0; x < tex_width; x++) {
			for (int i = 0; i < components; i++) {
				result[(y * tex_width + x) * components + i] = atlas[((start_x + x) + (start_y + y) * (atlas_width * tex_width)) * components + i];
			}
		}
	}
}

//////////////////////////////////////

bool CircleGenerator::iterator::is_end() const
{
	return xy[0] == -radius && xy[1] == radius + 1;
}

float CircleGenerator::iterator::distance_to_center() const
{
	const auto& x = xy[0];
	const auto& y = xy[1];

	return std::sqrt(x * x + y * y);
}

CircleGenerator::iterator::iterator(const int radius, const int x, const int y) : radius(radius), xy({ x, y })
{
	// if we start out of range, start by moving in range
	if (distance_to_center() > radius) {
		(*this)++;
	}
}

CircleGenerator::iterator& CircleGenerator::iterator::operator++() {
	while (!is_end()) {
		auto& x = xy[0];
		auto& y = xy[1];

		// if reach the end of row, go back to start
		if (x == radius) {
			x = -radius;
			y++;
		}
		// continue down row
		else {
			x++;
		}

		if (distance_to_center() <= radius) {
			return *this;
		}
	}


	return *this;
}

CircleGenerator::iterator CircleGenerator::iterator::operator++(int) {
	iterator retval = *this;
	++(*this);
	return retval;
}

bool CircleGenerator::iterator::operator==(iterator other) const {
	return xy == other.xy;
}

bool CircleGenerator::iterator::operator!=(iterator other) const { return !(*this == other); }

CircleGenerator::iterator::reference CircleGenerator::iterator::operator*() const { return xy; }

//////////////////////////////////////

CircleGenerator::CircleGenerator(const int radius) : radius(radius) {}

CircleGenerator::iterator CircleGenerator::begin() const {
	// start at top-left corner
	return iterator(radius, -radius, -radius);
}

CircleGenerator::iterator CircleGenerator::end() const {
	// end just past bottom-left corner
	// requires that we increment x then y
	return iterator(radius, -radius, radius + 1);
}
