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

#define INORTH vmath::ivec3(0, 0, -1)
#define ISOUTH vmath::ivec3(0, 0, 1)
#define IEAST vmath::ivec3(1, 0, 0)
#define IWEST vmath::ivec3(-1, 0, 0)
#define IUP vmath::ivec3(0, 1, 0)
#define IDOWN vmath::ivec3(0, -1, 0)

#include "GL/gl3w.h"
#include "GLFW/glfw3.h"

#include <tuple>
#include <vector>
#include <vmath.h>

using namespace vmath;

// simple pair hash function
struct pair_hash
{
	template <class T1, class T2>
	std::size_t operator() (const std::pair<T1, T2> &pair) const
	{
		return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
	}
};

// simple vecN hash function
struct vecN_hash
{
	template <typename T, const int len>
	std::size_t operator() (const vmath::vecN<T, len> vec) const
	{
		int result = 0;

		for (int i = 0; i < len; i++) {
			result ^= std::hash<int>()(vec[i]);
		}

		return result;
	}
};


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
		rotate(pitch, vmath::vec3(1.0f, 0.0f, 0.0f)) * // rotate pitch around X
		rotate(yaw, vmath::vec3(0.0f, 1.0f, 0.0f));    // rotate yaw   around Y
}

inline vmath::ivec4 vec2ivec(vmath::vec4 v) {
	vmath::ivec4 result;
	for (int i = 0; i < v.size(); i++) {
		result[i] = (int)floorf(v[i]);
	}
	return result;
}

// given a vec3 index, generate the other 2 indices
static inline void gen_working_indices(int &layers_idx, int &working_idx_1, int &working_idx_2) {
	// working indices are always gonna be xy, xz, or yz.
	working_idx_1 = layers_idx == 0 ? 1 : 0;
	working_idx_2 = layers_idx == 2 ? 1 : 2;
}

template <typename T, const int len>
static inline bool in_range(vecN<T, len> vec, vecN<T, len> min, vecN<T, len> max) {
	for (int i = 0; i < len; i++) {
		if (min[i] > vec[i] || vec[i] > max[i]) {
			return false;
		}
	}
	return true;
}

// Memory leak, delete returned result.
template <typename T, const int len>
static inline char* vec2str(vecN<T, len> vec) {
	char* result = new char[vec.size() * 16 + 4];
	char* tmp = result;

	tmp += sprintf(tmp, "(");
	for (int i = 0; i < len; i++) {
		tmp += sprintf(tmp, "%d", vec[i]);
		if (i != (len - 1)) {
			tmp += sprintf(tmp, ", ");
		}
	}

	tmp += sprintf(tmp, ")");
	
	return result;
}

double noise2d(double x, double y);

#endif // __UTIL_H__
