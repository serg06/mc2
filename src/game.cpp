#include "game.h"

#include "chunks.h"
#include "shapes.h"
#include "util.h"

#include "GL/gl3w.h"
#include "GLFW/glfw3.h"

#include <algorithm> // howbig?
#include <cmath>
#include <math.h>
#include <string>
#include <vmath.h> // TODO: Upgrade version, or use better library?
#include <windows.h>



// 1. TODO: Apply C++11 features
// 2. TODO: Apply C++14 features
// 3. TODO: Apply C++17 features
// 4. TODO: Make everything more object-oriented.
//		That way, I can define functions without having to declare them first, and shit.
//		And more good shit comes of it too.
//		Then from WinMain(), just call MyApp a = new MyApp, a.run(); !!

using namespace std;
using namespace vmath;

void glfw_onError(int error, const char* description)
{
	MessageBox(NULL, description, "GLFW error", MB_OK);
}

// Windows main
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	glfwSetErrorCallback(glfw_onError);
	App::app = new App();
	App::app->run();
}

void App::run() {
	if (!glfwInit()) {
		MessageBox(NULL, "Failed to initialize GLFW.", "GLFW error", MB_OK);
		return;
	}

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

	if (!window) {
		MessageBox(NULL, "Failed to create window.", "GLFW error", MB_OK);
		return;
	}

	// set this window as current window
	glfwMakeContextCurrent(window);

	// lock mouse into screen, for camera controls
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

	//// TODO: set callbacks
	glfwSetWindowSizeCallback(window, glfw_onResize);
	glfwSetKeyCallback(window, glfw_onKey);
	//glfwSetMouseButtonCallback(window, glfw_onMouseButton);
	glfwSetCursorPosCallback(window, glfw_onMouseMove);
	//glfwSetScrollCallback(window, glfw_onMouseWheel);

	// finally init gl3w
	if (gl3wInit()) {
		MessageBox(NULL, "Failed to initialize OpenGL.", "gl3w error", MB_OK);
		return;
	}

	//// TODO: set debug message callback
	//if (info.flags.debug) {
	//	if (gl3wIsSupported(4, 3))
	//	{
	//		glDebugMessageCallback((GLDEBUGPROC)debug_callback, this);
	//		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	//	}
	//}

	char buf[256];
	GLint tmpi;

	auto props = {
		GL_MAX_VERTEX_UNIFORM_COMPONENTS,
		GL_MAX_UNIFORM_LOCATIONS
	};


	for (auto prop : props) {
		glGetIntegerv(prop, &tmpi);
		sprintf(buf, "%x:\t%d\n", prop, tmpi);
		OutputDebugString(buf);
	}

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
	const GLfloat(&cube)[108] = shapes::cube_centered_half;

	// set vars
	memset(held_keys, false, sizeof(held_keys));
	glfwGetCursorPos(window, &last_mouse_x, &last_mouse_y); // reset mouse position

	// list of shaders to create program with
	// TODO: Embed these into binary somehow - maybe generate header file with cmake.
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

	// Set up our binding indices

	const GLuint trans_buf_uni_bidx = 0; // transformation buffer's uniform binding-point index
	const GLuint vert_buf_bidx = 0; // vertex buffer's binding-point index
	const GLuint position_attr_idx = 0; // index of 'position' attribute
	const GLuint chunk_types_bidx = 1; // chunk types buffer's binding-point index
	const GLuint chunk_types_attr_idx = 1; // index of 'chunk_type' attribute

	/* HANDLE CUBES FIRST */

	// vao: create VAO for cube[s], so we can tell OpenGL how to use it when it's bound
	glCreateVertexArrays(1, &vao_cube);

	// buffers: create
	glCreateBuffers(1, &vert_buf);
	glCreateBuffers(1, &chunk_types_buf);

	// buffers: allocate space
	glNamedBufferStorage(vert_buf, sizeof(cube), NULL, GL_DYNAMIC_STORAGE_BIT); // allocate enough for all vertices, and allow editing
	glNamedBufferStorage(chunk_types_buf, CHUNK_SIZE*sizeof(uint8_t), NULL, GL_DYNAMIC_STORAGE_BIT); // allocate enough for all vertices, and allow editing

	// buffers: insert static data
	glNamedBufferSubData(vert_buf, 0, sizeof(cube), cube); // vertex positions

	// vao: enable all cube's attributes, 1 at a time
	glEnableVertexArrayAttrib(vao_cube, position_attr_idx);
	glEnableVertexArrayAttrib(vao_cube, chunk_types_attr_idx);

	// vao: set up formats for cube's attributes, 1 at a time
	glVertexArrayAttribFormat(vao_cube, position_attr_idx, 3, GL_FLOAT, GL_FALSE, 0);
	glVertexArrayAttribIFormat(vao_cube, chunk_types_attr_idx, 1, GL_BYTE, 0);

	// vao: set binding points for all attributes, 1 at a time
	//      - 1 buffer per binding point; for clarity, to keep it clear, I should only bind 1 attr per binding point
	glVertexArrayAttribBinding(vao_cube, position_attr_idx, vert_buf_bidx);
	glVertexArrayAttribBinding(vao_cube, chunk_types_attr_idx, chunk_types_bidx);

	// vao: bind buffers to their binding points, 1 at a time
	glVertexArrayVertexBuffer(vao_cube, vert_buf_bidx, vert_buf, 0, sizeof(vec3));
	glVertexArrayVertexBuffer(vao_cube, chunk_types_bidx, chunk_types_buf, 0, sizeof(uint8_t));

	// vao: extra properties
	glBindVertexArray(vao_cube);
	glVertexAttribDivisor(chunk_types_attr_idx, 1);
	glBindVertexArray(0);

	glEnable(GL_BLEND); //Enable blending.
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //Set blending function.



	/* HANDLE UNIFORM NOW */

	// create buffers
	glCreateBuffers(1, &trans_buf);

	// bind them
	glBindBufferBase(GL_UNIFORM_BUFFER, trans_buf_uni_bidx, trans_buf); // bind transformation buffer to uniform buffer binding point

	// allocate
	glNamedBufferStorage(trans_buf, sizeof(mat4) * 2, NULL, GL_DYNAMIC_STORAGE_BIT); // allocate 2 matrices of space for transforms, and allow editing


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

	// generate a chunk
	chunks[0] = gen_chunk();
	chunks[0][1] = Block::Grass;
	chunk_coords[0] = { 0, 0 };

	// load into memory pog
	glNamedBufferSubData(chunk_types_buf, 0, CHUNK_SIZE * sizeof(uint8_t), chunks[0]); // proj matrix

}

void App::render(double time) {
	// change in time
	const double dt = time - last_render_time;
	last_render_time = time;

	// update player movement
	App::update_player_movement(dt);

	// Create Model->World matrix
	float f = (float)time * (float)M_PI * 0.1f;
	mat4 model_world_matrix =
		translate(0.0f, 0.0f, 0.0f);

	// Create World->View matrix
	mat4 world_view_matrix =
		rotate_pitch_yaw(char_pitch, char_yaw) *
		translate(-char_position[0], -char_position[1], -char_position[2]); // move relative to you

	// Combine them into Model->View matrix
	mat4 model_view_matrix = world_view_matrix * model_world_matrix;

	// Update projection matrix too, in case if width/height changed
	mat4 proj_matrix = perspective(
		(float)info.vfov, // virtual fov
		(float)info.width / (float)info.height, // aspect ratio
		0.1f,  // can't see behind 0.0 anyways
		-1000.0f // our object will be closer than 100.0
	);

	// Draw background color
	const GLfloat color[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	glClearBufferfv(GL_COLOR, 0, color);
	glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0); // used for depth test somehow

	// Update transformation buffer with matrices
	glNamedBufferSubData(trans_buf, 0, sizeof(model_view_matrix), model_view_matrix);
	glNamedBufferSubData(trans_buf, sizeof(model_view_matrix), sizeof(proj_matrix), proj_matrix); // proj matrix

	// Update chunk types buffer with chunk types!
	//glNamedBufferSubData(chunk_types_buf, 0, CHUNK_SIZE * sizeof(uint8_t), chunks[0]); // proj matrix

	char buf[256];
	sprintf(buf, "Drawing (took %d ms)\n", (int)(dt * 1000));
	OutputDebugString(buf);
	sprintf(buf, "Position: (%.1f, %.1f, %.1f)\n", char_position[0], char_position[1], char_position[2]);
	OutputDebugString(buf);

	// Draw our chunks!
	glBindVertexArray(vao_cube);
	glDrawArraysInstanced(GL_TRIANGLES, 0, 36, CHUNK_SIZE);
}

void App::update_player_movement(const double dt) {
	// update player's movement based on how much time has passed since we last did it

	// Velocity falloff
	//   TODO: Handle walking on blocks, in water, etc. Maybe do it based on friction.
	//   TODO: Tweak values.
	char_velocity *= (float)pow(0.5, dt);
	vec4 norm = normalize(char_velocity);
	for (int i = 0; i < 4; i++) {
		if (char_velocity[i] > 0.0f) {
			char_velocity[i] = (float)fmax(0.0f, char_velocity[i] - (10.0f * norm[i] * dt));
		}
		else if (char_velocity[i] < 0.0f) {
			char_velocity[i] = (float)fmin(0.0f, char_velocity[i] - (10.0f * norm[i] * dt));
		}
	}


	// Calculate char's yaw rotation direction
	mat4 dir_rotation = rotate_pitch_yaw(0.0f, char_yaw);

	// calculate acceleration
	vec4 acceleration = { 0.0f };

	if (held_keys[GLFW_KEY_W]) {
		acceleration += dir_rotation * vec4(0.0f, 0.0f, -1.0f, 0.0f);
	}
	if (held_keys[GLFW_KEY_S]) {
		acceleration += dir_rotation * vec4(0.0f, 0.0f, 1.0f, 0.0f);
	}
	if (held_keys[GLFW_KEY_A]) {
		acceleration += dir_rotation * vec4(-1.0f, 0.0f, 0.0f, 0.0f);
	}
	if (held_keys[GLFW_KEY_D]) {
		acceleration += dir_rotation * vec4(1.0f, 0.0f, 0.0f, 0.0f);
	}
	if (held_keys[GLFW_KEY_SPACE]) {
		acceleration += vec4(0.0f, 1.0f, 0.0f, 0.0f);
	}
	if (held_keys[GLFW_KEY_LEFT_SHIFT]) {
		acceleration += dir_rotation * vec4(0.0f, -1.0f, 0.0f, 0.0f);
	}

	// Velocity change via acceleration
	char_velocity += acceleration * dt * 50.0f;
	if (length(char_velocity) > 10.0f) {
		char_velocity = 10.0f * normalize(char_velocity);
	}
	char_velocity[3] = 0.0f; // Just in case

							 // Update player position
	char_position += char_velocity * dt;
}

void App::glfw_onKey(GLFWwindow* window, int key, int scancode, int action, int mods) {
	App::app->onKey(window, key, scancode, action, mods);
}

// on key press
void App::onKey(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// ignore unknown keys
	if (key == GLFW_KEY_UNKNOWN) {
		return;
	}

	// handle key presses
	if (action == GLFW_PRESS) {
		held_keys[key] = true;
	}

	// handle key releases
	if (action == GLFW_RELEASE) {
		held_keys[key] = false;
	}
}

void App::glfw_onMouseMove(GLFWwindow* window, double x, double y) {
	App::app->onMouseMove(window, x, y);
}

// on mouse movement
void App::onMouseMove(GLFWwindow* window, double x, double y)
{
	// bonus of using deltas for yaw/pitch:
	// - can cap easily -- if we cap without deltas, and we move 3000x past the cap, we'll have to move 3000x back before mouse moves!
	// - easy to do mouse sensitivity
	double delta_x = x - last_mouse_x;
	double delta_y = y - last_mouse_y;

	// update pitch/yaw
	char_yaw += (float)(info.mouseX_Sensitivity * delta_x);
	char_pitch += (float)(info.mouseY_Sensitivity * delta_y);

	// cap pitch
	char_pitch = clamp(char_pitch, -90.0f, 90.0f);

	// update old values
	last_mouse_x = x;
	last_mouse_y = y;
}

void App::glfw_onResize(GLFWwindow* window, int width, int height) {
	App::app->onResize(window, width, height);
}

// on window resize
void App::onResize(GLFWwindow* window, int width, int height) {
	info.width = width;
	info.height = height;

	int px_width, px_height;
	glfwGetFramebufferSize(window, &px_width, &px_height);
	glViewport(0, 0, px_width, px_height);
}

