#ifndef __GAME_H__
#define __GAME_H__

#include <iostream>
#include "GL/gl3w.h"
#include "GLFW/glfw3.h"

#include <string>
#include <fstream>
#include <streambuf>
#include <list>
#include <vector>
#include <iterator>
#include <tuple>

#include <vmath.h> // TODO: Upgrade version, or use better library?

#include <math.h>
#include <cmath>

using namespace std;

struct appInfo {
	const string title = "OpenGL";
	const bool debug = GL_TRUE;
	const bool msaa = GL_FALSE;
	const int width = 800;
	const int height = 600;
};


class App {
protected:
	static App* app;
public:
	// the one and only copy of this app

	const appInfo info;
	GLFWwindow *window;

	GLuint rendering_program;
	GLuint vao;
	GLuint trans_buf;
	GLuint vert_buf;
	double last_mouse_x;
	double last_mouse_y;
	double last_render_time;
	bool held_keys[GLFW_KEY_LAST + 1];

	vmath::vec4 char_position;
	vmath::vec4 char_velocity = vmath::vec4(0.0f);
	float char_pitch = 0.0f; //   up/down  angle;    capped to [-90.0, 90.0]
	float char_yaw = 0.0f;   // left/right angle; un-capped

	App();
	void startup();
	void render(double);
	void shutdown();

	// callback functions must be static
	static void glfw_onKey(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void glfw_onMouseMove(GLFWwindow* window, double x, double y);
	static void gl_onDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, GLvoid* userParam);
};
App* App::app;
static App* app2;



#endif /* __GAME_H__ */