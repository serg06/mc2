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
#include <assert.h>
#include <algorithm>

#include "game.h"
#include "util.h"
#include "shapes.h"

//#define MAX_VELOCITY 0.1f
//#define FORCE 1.0f
//#define MASS 1.0f

// 1. TODO: Apply C++11 features
// 2. TODO: Apply C++14 features
// 3. TODO: Apply C++17 features
// 4. TODO: Make everything more object-oriented.
//		That way, I can define functions without having to declare them first, and shit.
//		And more good shit comes of it too.
//		Then from WinMain(), just call MyApp a = new MyApp, a.run(); !!

using namespace std;

// Windows main
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	App::app = new App();
	App::app->run();
}

void App::run() {
	assert(glfwInit() && "Failed to initialize GLFW.");

	// OpenGL 4.5
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);

	// using OpenGL core profile
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// remove deprecated functionality (might as well, 'cause I'm using gl3w)
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	// disable MSAA
	glfwWindowHint(GLFW_SAMPLES, info.msaa);

	// debug mode
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, info.debug);

	// create window
	window = glfwCreateWindow(info.width, info.height, info.title.c_str(), nullptr, nullptr);

	assert(window && "Failed to open window.");

	// set this window as current window
	glfwMakeContextCurrent(window);

	// lock mouse into screen, for camera controls
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

	//// TODO: set callbacks
	//glfwSetWindowSizeCallback(window, glfw_onResize);
	glfwSetKeyCallback(window, glfw_onKey);
	//glfwSetMouseButtonCallback(window, glfw_onMouseButton);
	glfwSetCursorPosCallback(window, glfw_onMouseMove);
	//glfwSetScrollCallback(window, glfw_onMouseWheel);


	// finally init gl3w
	assert(!gl3wInit() && "Failed to initialize OpenGL.");

	//// TODO: set debug message callback
	//if (info.flags.debug) {
	//	if (gl3wIsSupported(4, 3))
	//	{
	//		glDebugMessageCallback((GLDEBUGPROC)debug_callback, this);
	//		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	//	}
	//}

	// Start up app
	startup();

	// run until user presses ESC or tries to close window
	last_render_time = glfwGetTime();
	while ((glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_RELEASE) && (!glfwWindowShouldClose(window))) {
		// run rendering function
		render(glfwGetTime());

		// display drawing buffer on screen
		glfwSwapBuffers(window);

		// poll window system for events
		glfwPollEvents();
	}

	shutdown();
}

void App::startup() {
	const GLfloat(&cube)[108] = shapes::cube;

	// set vars
	char_position = vmath::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	char_pitch = 0.0f;
	char_yaw = 0.0f;
	memset(held_keys, false, sizeof(held_keys));
	glfwGetCursorPos(window, &last_mouse_x, &last_mouse_y); // reset mouse position

	// placeholder
	glCreateVertexArrays(1, &vao);
	glBindVertexArray(vao);

	// list of shaders to create program with
	std::vector <std::tuple<std::string, GLenum>> shader_fnames = {
		{ "../src/simple.vs.glsl", GL_VERTEX_SHADER },
		//{"../src/simple.tcs.glsl", GL_TESS_CONTROL_SHADER },
		//{"../src/simple.tes.glsl", GL_TESS_EVALUATION_SHADER },
		//{"../src/simple.gs.glsl", GL_GEOMETRY_SHADER },
		{ "../src/simple.fs.glsl", GL_FRAGMENT_SHADER },
	};

	// create program
	rendering_program = compile_shaders(shader_fnames);

	/*
	* CUBE MOVEMENT
	*/

	// Simple projection matrix
	// TODO: Get width/height automatically
	vmath::mat4 proj_matrix = vmath::perspective(
		59.0f, // 59.0 vfov = 90.0 hfov
		info.width / info.height,  // aspect ratio - not sure if right
		0.1f,  // can't see behind 0.0 anyways
		-1000.0f // our object will be closer than 100.0
	);

	const GLuint uni_binding_idx = 0;
	const GLuint attrib_idx = 0;
	const GLuint vert_binding_idx = 0;

	// create buffers
	glCreateBuffers(1, &trans_buf);
	glCreateBuffers(1, &vert_buf);

	// bind them
	glBindBufferBase(GL_UNIFORM_BUFFER, uni_binding_idx, trans_buf); // bind transformation buffer to uniform buffer binding point
	glVertexArrayAttribBinding(vao, attrib_idx, vert_binding_idx); // connect vert -> position variable

	// allocate
	glNamedBufferStorage(trans_buf, 2 * sizeof(vmath::mat4), NULL, GL_DYNAMIC_STORAGE_BIT); // allocate 2 matrices of space for transforms, and allow editing
	glNamedBufferStorage(vert_buf, sizeof(cube), NULL, GL_DYNAMIC_STORAGE_BIT); // allocate enough for all vertices, and allow editing

	// insert data (skip model-view matrix; we'll update it in render())
	glNamedBufferSubData(trans_buf, sizeof(vmath::mat4), sizeof(vmath::mat4), proj_matrix); // proj matrix
	glNamedBufferSubData(vert_buf, 0, sizeof(cube), cube); // vertex positions

	// enable auto-filling of position
	glVertexArrayVertexBuffer(vao, vert_binding_idx, vert_buf, 0, sizeof(vmath::vec3));
	glVertexArrayAttribFormat(vao, attrib_idx, 3, GL_FLOAT, GL_FALSE, 0);
	glEnableVertexAttribArray(attrib_idx);


	/*
	* MINECRAFT TEXTURE
	*/

	// TODO, maybe - looks kinda hard tbh

	//// create texture
	//GLuint texGrass;
	//glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &texGrass);

	//// allocate 6 sides
	//glTexStorage2D(GL_TEXTURE_CUBE_MAP, 1, GL_RGBA32F, 16, 16);

	//// bind them
	//glBindTexture(GL_TEXTURE_CUBE_MAP, texGrass);


	/*
	* ETC
	*/

	glPointSize(5.0f);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CW);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	// use our program object for rendering
	glUseProgram(rendering_program);
}

void App::render(double time) {
	/* PLAYER MOVEMENT */
	char buf[256];

	// change in time
	const double dt = time - last_render_time;
	last_render_time = time;
	// character's rotation
	vmath::mat4 dir_rotation = rotate_pitch_yaw(char_pitch, char_yaw);

	// Update player velocity
	//char_velocity += dir_rotation * vmath::vec4(0.0f, 0.0f, -1.0f, 0.0f) * dt * 0.1f * (held_keys[GLFW_KEY_W] ? 1.0f : -1.0f);
	vmath::vec4 acceleration = vmath::vec4(0.0f);

	// calculate acceleration
	if (held_keys[GLFW_KEY_W]) {
		acceleration += dir_rotation * vmath::vec4(0.0f, 0.0f, -1.0f, 0.0f);
	}
	if (held_keys[GLFW_KEY_S]) {
		acceleration += dir_rotation * vmath::vec4(0.0f, 0.0f, 1.0f, 0.0f);
	}
	if (held_keys[GLFW_KEY_A]) {
		acceleration += dir_rotation * vmath::vec4(-1.0f, 0.0f, 0.0f, 0.0f);
	}
	if (held_keys[GLFW_KEY_D]) {
		acceleration += dir_rotation * vmath::vec4(1.0f, 0.0f, 0.0f, 0.0f);
	}
	if (held_keys[GLFW_KEY_SPACE]) {
		acceleration += vmath::vec4(0.0f, 1.0f, 0.0f, 0.0f);
	}
	if (held_keys[GLFW_KEY_LEFT_SHIFT]) {
		acceleration += dir_rotation * vmath::vec4(0.0f, -1.0f, 0.0f, 0.0f);
	}

	// TODO: Tweak velocity falloff values?

	// Velocity falloff (TODO: For blocks and shit too. Maybe based on friction.)
	char_velocity *= pow(0.5, dt);
	for (int i = 0; i < 4; i++) {
		if (char_velocity[i] > 0.0f) {
			char_velocity[i] = fmax(0.0f, char_velocity[i] - (10.0f * sgn(char_velocity[i]) * dt));
		}
		else if (char_velocity[i] < 0.0f) {
			char_velocity[i] = fmin(0.0f, char_velocity[i] - (10.0f * sgn(char_velocity[i]) * dt));
		}
	}

	// Velocity change (via acceleration)
	char_velocity += acceleration * dt * 50.0f;
	if (length(char_velocity) > 10.0f) {
		char_velocity = 10.0f * vmath::normalize(char_velocity);
	}
	char_velocity[3] = 0.0f; // Just in case

	// Update player position
	char_position += char_velocity * dt;

	/* DRAWING */

	// BACKGROUND COLOUR

	//const GLfloat color[] = { (float)sin(currentTime) * 0.5f + 0.5f, (float)cos(currentTime) * 0.5f + 0.5f, 0.0f, 1.0f };
	const GLfloat color[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	glClearBufferfv(GL_COLOR, 0, color);
	glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0); // used for depth test somehow

	// MOVEMENT CALCULATION

	// Create Model->World matrix, including our crazy cube movement
	float f = (float)time * (float)M_PI * 0.1f;
	vmath::mat4 model_world_matrix =
		vmath::translate(0.0f, 0.0f, -4.0f) * // move cube in front of us
		vmath::translate(sinf(2.1f * f) * 0.5f, cosf(1.7f * f) * 0.5f, sinf(1.3f * f) * cosf(1.5f * f) * 2.0f) * // cube movement: translations 1
		vmath::rotate((float)time * 45.0f, 0.0f, 1.0f, 0.0f) * // cube movement: rotations 2
		vmath::rotate((float)time * 81.0f, 1.0f, 0.0f, 0.0f);  // cube movement: rotations 1

	// Create World->View matrix
	vmath::mat4 world_view_matrix =
		rotate_pitch_yaw(char_pitch, char_yaw) *
		vmath::translate(-char_position[0], -char_position[1], -char_position[2]); // move relative to you

	// Combine them into Model->View matrix
	vmath::mat4 model_view_matrix = world_view_matrix * model_world_matrix;

	// Update transformation buffer with mv matrix
	glNamedBufferSubData(trans_buf, 0, sizeof(model_view_matrix), model_view_matrix);

	// DRAWING

	// Draw our cube
	glDrawArrays(GL_TRIANGLES, 0, 36);
}

// on key press
void App::glfw_onKey(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// ignore unknown keys
	if (key == GLFW_KEY_UNKNOWN) {
		return;
	}

	// handle key presses
	if (action == GLFW_PRESS) {
		App::app->held_keys[key] = true;
	}

	// handle key releases
	if (action == GLFW_RELEASE) {
		App::app->held_keys[key] = false;
	}
}

// on mouse movement
void App::glfw_onMouseMove(GLFWwindow* window, double x, double y)
{
	// bonus of using deltas for yaw/pitch:
	// - can cap easily -- if we cap without deltas, and we move 3000x past the cap, we'll have to move 3000x back before mouse moves!
	// - easy to do mouse sensitivity
	double delta_x = x - App::app->last_mouse_x;
	double delta_y = y - App::app->last_mouse_y;

	// update pitch/yaw
	App::app->char_yaw += App::app->info.mouseX_Sensitivity * delta_x;
	App::app->char_pitch += App::app->info.mouseY_Sensitivity * delta_y;

	// cap pitch
	App::app->char_pitch = clamp(App::app->char_pitch, -90.0f, 90.0f);

	// update old values
	App::app->last_mouse_x = x;
	App::app->last_mouse_y = y;
}
