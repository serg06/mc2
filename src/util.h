// UTIL class filled with useful static functions
#ifndef __UTIL_H__
#define __UTIL_H__

// for types
#include "GL/gl3w.h"
#include "GLFW/glfw3.h"
#include <vector> 
#include <list> 
#include <iterator> 
#include <tuple> 

#include <fstream>

// math sign function
template <typename T>
constexpr int sgn(T val) {
	return (T(0) < val) - (val < T(0));
}

void print_arr(const GLfloat*, int, int);
GLuint compile_shaders(std::vector <std::tuple<std::string, GLenum>>);
vmath::mat4 rotate_pitch_yaw(float pitch, float yaw);

#endif // __UTIL_H__
