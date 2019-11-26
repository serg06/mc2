// UTIL class filled with useful static functions
#ifndef __UTIL_H__
#define __UTIL_H__

#define NORTH_0 vmath::vec4(0.0f, 0.0f, -1.0f, 0.0f)
#define NORTH_1 vmath::vec4(0.0f, 0.0f, -1.0f, 1.0f)

#define SOUTH_0 vmath::vec4(0.0f, 0.0f, 1.0f, 0.0f)
#define SOUTH_1 vmath::vec4(0.0f, 0.0f, 1.0f, 1.0f)

#define EAST_0 vmath::vec4(1.0f, 0.0f, 0.0f, 0.0f)
#define EAST_1 vmath::vec4(1.0f, 0.0f, 0.0f, 1.0f)

#define WEST_0 vmath::vec4(-1.0f, 0.0f, 0.0f, 0.0f)
#define WEST_1 vmath::vec4(-1.0f, 0.0f, 0.0f, 1.0f)

#define UP_0 vmath::vec4(0.0f, 1.0f, 0.0f, 0.0f)
#define UP_1 vmath::vec4(0.0f, 1.0f, 0.0f, 1.0f)

#define DOWN_0 vmath::vec4(0.0f, -1.0f, 0.0f, 0.0f)
#define DOWN_1 vmath::vec4(0.0f, -1.0f, 0.0f, 1.0f)

#include "GL/gl3w.h"
#include "GLFW/glfw3.h"

#include <tuple>
#include <vector>
#include <vmath.h>

using namespace vmath;

// math sign function
template <typename T>
constexpr int sgn(T val) {
	return (T(0) < val) - (val < T(0));
}

void print_arr(const GLfloat*, int, int);
GLuint compile_shaders(std::vector <std::tuple<std::string, GLenum>>);
mat4 rotate_pitch_yaw(float pitch, float yaw);

#endif // __UTIL_H__
