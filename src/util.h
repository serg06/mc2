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

#define INORTH_0 vmath::ivec4(0, 0, -1, 0)
#define ISOUTH_0 vmath::ivec4(0, 0, 1, 0)
#define IEAST_0 vmath::ivec4(1, 0, 0, 0)
#define IWEST_0 vmath::ivec4(-1, 0, 0, 0)
#define IUP_0 vmath::ivec4(0, 1, 0, 0)
#define IDOWN_0 vmath::ivec4(0, -1, 0, 0)

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
GLuint link_program(GLuint);

// Create rotation matrix given pitch and yaw
inline mat4 rotate_pitch_yaw(float pitch, float yaw) {
	return
		rotate(pitch, vec3(1.0f, 0.0f, 0.0f)) * // rotate pitch around X
		rotate(yaw, vec3(0.0f, 1.0f, 0.0f));    // rotate yaw   around Y
}

inline ivec4 vec2ivec(vec4 v) {
	ivec4 result;
	for (int i = 0; i < v.size(); i++) {
		result[i] = (int)v[i];
	}
	return result;
}

#endif // __UTIL_H__
