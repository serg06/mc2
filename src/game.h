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

#include "chunks.h"

#include <math.h>
#include <cmath>

using namespace std;

struct AppInfo {
	string title = "OpenGL";
	bool debug = GL_TRUE;
	bool msaa = GL_FALSE;
	int width = 800;
	int height = 600;
	float vfov = 59.0f; // vertical fov -- 59.0 vfov = 90.0 hfov
	float mouseX_Sensitivity = 0.25f;
	float mouseY_Sensitivity = 0.25f;
};


class App {
protected:
public:
	// the one and only copy of this app
	AppInfo info;
	GLFWwindow *window;

	GLuint rendering_program;
	GLuint vao;
	GLuint trans_buf;
	GLuint vert_buf;
	double last_mouse_x;
	double last_mouse_y;
	double last_render_time;
	bool held_keys[GLFW_KEY_LAST + 1];

	vmath::vec4 char_position = vmath::vec4(8.0f, 66.0f, 8.0f, 1.0f);
	vmath::vec4 char_velocity = vmath::vec4(0.0f, 0.0f, 0.0f, 0.0f);
	float char_pitch = 0.0f; //   up/down  angle;    capped to [-90.0, 90.0]
	float char_yaw = 0.0f;   // left/right angle; un-capped (TODO: Reset it if it gets too high?)
	
	// store our chunk info in here for now
	Block* chunks[1024];
	int* chunk_coords[1024];

	App() {

	}

	void shutdown() {
		// TODO: Maybe some day.
	}

	void run();
	void startup();
	void render(double);

	void onKey(GLFWwindow* window, int key, int scancode, int action, int mods);
	void onMouseMove(GLFWwindow* window, double x, double y);
	void onResize(GLFWwindow* window, int width, int height);
	
	// glfw static functions which passthrough to real handle functions
	static void glfw_onKey(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void glfw_onMouseMove(GLFWwindow* window, double x, double y);
	static void glfw_onResize(GLFWwindow* window, int width, int height);
	//static void gl_onDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, GLvoid* userParam);

	static App* app;
};
// define/declare/de-whatever App's static variables
App* App::app; 

#endif /* __GAME_H__ */