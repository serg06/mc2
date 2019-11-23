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

using namespace std;

void startup();
void render(double);
void shutdown();
int main();
int main();
GLuint compile_shaders();
static void glfw_onKey(GLFWwindow* window, int key, int scancode, int action, int mods);

// TODO: Make everything more object-oriented.
// That way, I can define functions without having to declare them first, and shit.
// And more good shit comes of it too.
// Then from WinMain(), just call MyApp a = new MyApp, a.run(); !!

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	return main();
}

GLuint rendering_program;
GLuint vao;
GLuint trans_buf;
GLuint vert_buf;
vmath::vec3 char_position;
vmath::vec3 char_rotation; // rotation around x axis, y axis, and z axis (z stays 0 I think)

int main() {
	static GLFWwindow *window;
	static const struct appInfo {
		const string title = "OpenGL";
		const bool debug = GL_TRUE;
		const bool msaa = GL_FALSE;
		const int width = 800;
		const int height = 600;
	} info;


	if (!glfwInit()) {
		printf("failed to initialize GLFW.\n");
		return -1;
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
	if (!window)
	{
		fprintf(stderr, "Failed to open window\n");
		return -1;
	}

	// set this window as current window
	glfwMakeContextCurrent(window);

	//// TODO: set callbacks
	//glfwSetWindowSizeCallback(window, glfw_onResize);
	glfwSetKeyCallback(window, glfw_onKey);
	//glfwSetMouseButtonCallback(window, glfw_onMouseButton);
	//glfwSetCursorPosCallback(window, glfw_onMouseMove);
	//glfwSetScrollCallback(window, glfw_onMouseWheel);


	// finally init gl3w
	if (gl3wInit()) {
		printf("failed to initialize OpenGL\n");
		return -1;
	}

	//// TODO: set debug message callback
	//if (info.flags.debug) {
	//	if (gl3wIsSupported(4, 3))
	//	{
	//		glDebugMessageCallback((GLDEBUGPROC)debug_callback, this);
	//		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	//	}
	//}

	startup();

	// run until user presses ESC or tries to close window
	while ((glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_RELEASE) && (!glfwWindowShouldClose(window))) {
		// run rendering function
		render(glfwGetTime());

		// display drawing buffer on screen
		glfwSwapBuffers(window);

		// poll window system for events
		glfwPollEvents();
	}

	shutdown();


	// ...

	return 0;
}

void startup() {
	// Cube at origin
	static const GLfloat vertex_positions[] =
	{
		-0.25f, 0.25f, -0.25f,
		-0.25f, -0.25f, -0.25f,
		0.25f, -0.25f, -0.25f,

		0.25f, -0.25f, -0.25f,
		0.25f, 0.25f, -0.25f,
		-0.25f, 0.25f, -0.25f,

		0.25f, -0.25f, -0.25f,
		0.25f, -0.25f, 0.25f,
		0.25f, 0.25f, -0.25f,

		0.25f, -0.25f, 0.25f,
		0.25f, 0.25f, 0.25f,
		0.25f, 0.25f, -0.25f,

		0.25f, -0.25f, 0.25f,
		-0.25f, -0.25f, 0.25f,
		0.25f, 0.25f, 0.25f,

		-0.25f, -0.25f, 0.25f,
		-0.25f, 0.25f, 0.25f,
		0.25f, 0.25f, 0.25f,

		-0.25f, -0.25f, 0.25f,
		-0.25f, -0.25f, -0.25f,
		-0.25f, 0.25f, 0.25f,

		-0.25f, -0.25f, -0.25f,
		-0.25f, 0.25f, -0.25f,
		-0.25f, 0.25f, 0.25f,

		-0.25f, -0.25f, 0.25f,
		0.25f, -0.25f, 0.25f,
		0.25f, -0.25f, -0.25f,

		0.25f, -0.25f, -0.25f,
		-0.25f, -0.25f, -0.25f,
		-0.25f, -0.25f, 0.25f,

		-0.25f, 0.25f, -0.25f,
		0.25f, 0.25f, -0.25f,
		0.25f, 0.25f, 0.25f,

		0.25f, 0.25f, 0.25f,
		-0.25f, 0.25f, 0.25f,
		-0.25f, 0.25f, -0.25f
	};

	// set global vars
	char_position = vmath::vec3(0.0f, 0.0f, 0.0f);
	char_rotation = vmath::vec3(0.0f, 0.0f, 0.0f); // rotation around x axis, y axis, and z axis (z stays 0 I think)

	// placeholder
	glCreateVertexArrays(1, &vao);
	glBindVertexArray(vao);

	// compile shaders
	rendering_program = compile_shaders();


	/*
	* CUBE MOVEMENT
	*/

	// Simple projection matrix
	// TODO: Get width/height automatically
	vmath::mat4 proj_matrix = vmath::perspective(
		40.0f, // 59.0 vfov = 90.0 hfov
		800.0f / 600.0f,  // aspect ratio - not sure if right
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
	glBindBufferBase(GL_UNIFORM_BUFFER, uni_binding_idx, trans_buf); // bind trans buf to uniform buffer binding point
	glBindBuffer(GL_ARRAY_BUFFER, vert_buf); // why?
	glVertexArrayAttribBinding(vao, attrib_idx, vert_binding_idx); // connect vert -> position variable

	// allocate
	glNamedBufferStorage(trans_buf, 2 * sizeof(vmath::mat4), NULL, GL_DYNAMIC_STORAGE_BIT); // allocate 2 matrices of space for transforms, and allow editing
	glNamedBufferStorage(vert_buf, sizeof(vertex_positions), NULL, GL_DYNAMIC_STORAGE_BIT); // allocate enough for all vertices, and allow editing

	// insert data (skip model-view matrix; we'll update it in render())
	glNamedBufferSubData(trans_buf, sizeof(vmath::mat4), sizeof(vmath::mat4), proj_matrix); // proj matrix
	glNamedBufferSubData(vert_buf, 0, sizeof(vertex_positions), vertex_positions); // vertex positions

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

void render(double time) {
	// BACKGROUND COLOUR

	//const GLfloat color[] = { (float)sin(currentTime) * 0.5f + 0.5f, (float)cos(currentTime) * 0.5f + 0.5f, 0.0f, 1.0f };
	const GLfloat color[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	glClearBufferfv(GL_COLOR, 0, color);
	glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0); // used for depth test somehow

	// MOVEMENT CALCULATION

	float f = (float)time * (float)M_PI * 0.1f;
	vmath::mat4 model_world_matrix =
		vmath::translate(0.0f, 0.0f, -4.0f) *
		vmath::translate(sinf(2.1f * f) * 0.5f,
			cosf(1.7f * f) * 0.5f,
			sinf(1.3f * f) * cosf(1.5f * f) * 2.0f) *
		vmath::rotate((float)time * 45.0f, 0.0f, 1.0f, 0.0f) *
		vmath::rotate((float)time * 81.0f, 1.0f, 0.0f, 0.0f);


	// WORLD->VIEW MATRIX:
	//   - move it relative to you (e.g. if you're at (3,3), subtract (3,3) from world coordinates)
	//       - now it's relative to you positionally, but need to rotate it to the angle you're looking
	//   - rotate it around you
	vmath::mat4 world_view_matrix =
		vmath::rotate(-char_rotation[1], 0.0f, 1.0f, 0.0f) * // look right/left (rotate around y)
		vmath::rotate(-char_rotation[0], 1.0f, 0.0f, 0.0f) * // look    up/down (rotate around x)
		vmath::translate(-char_position) * // move relative to you
		vmath::mat4::identity();

	vmath::mat4 model_view_matrix = world_view_matrix * model_world_matrix;

	// Update transformation buffer with mv matrix
	glNamedBufferSubData(trans_buf, 0, sizeof(model_view_matrix), model_view_matrix);

	// DRAWING

	glDrawArrays(GL_TRIANGLES, 0, 36); // draw triangle using 3 VAOs, starting at the 0th one (our only one!)
}

void shutdown() {
	// TODO: Maybe some day.
}

GLuint compile_shaders() {
	GLuint program;
	std::list <std::tuple<std::string, GLenum>> shader_fnames;
	std::list <GLuint> shaders; // store compiled shaders

								// list of shader names to include in program
	shader_fnames.push_back(std::make_tuple("../src/simple.vs.glsl", GL_VERTEX_SHADER));
	//shader_fnames.push_back(std::make_tuple("../src/simple.tcs.glsl", GL_TESS_CONTROL_SHADER));
	//shader_fnames.push_back(std::make_tuple("../src/simple.tes.glsl", GL_TESS_EVALUATION_SHADER));
	//shader_fnames.push_back(std::make_tuple("../src/simple.gs.glsl", GL_GEOMETRY_SHADER));
	shader_fnames.push_back(std::make_tuple("../src/simple.fs.glsl", GL_FRAGMENT_SHADER));

	// for each input shader
	for (const std::tuple <std::string, GLenum> &shader_fname : shader_fnames)
	{
		// extract shader info
		const std::string fname = std::get<0>(shader_fname);
		const GLenum shadertype = std::get<1>(shader_fname);

		// load shader src
		std::ifstream shader_file(fname);

		if (!shader_file.is_open()) {
			OutputDebugString("could not open shader file: ");
			OutputDebugString(fname.c_str());
			OutputDebugString("\n");
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

	glLinkProgram(program); // link together all attached shaders

							// CHECK IF LINKING SUCCESSFUL
	GLint status = GL_TRUE;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)
	{
		GLint logLen;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
		std::vector <char> log(logLen);
		GLsizei written;
		glGetProgramInfoLog(program, logLen, &written, log.data());

		OutputDebugString("linking error with program:\n\n");
		OutputDebugString(log.data());
		OutputDebugString("\n");
		exit(1);
	}

	// Delete the shaders as the program has them now
	for (const GLuint &shader : shaders) {
		glDeleteShader(shader);
	}

	return program;
}

// Don't need this yet, but gonna add it to utils.
// TODO: Move to utils.
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

// on key press
static void glfw_onKey(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// ?
	if (!action) {
		return;
	}


	switch (key) {
	case GLFW_KEY_W:
		char_position += vmath::vec3(0.0f, 0.0f, -1.0f);
		break;
	case GLFW_KEY_S:
		char_position += vmath::vec3(0.0f, 0.0f, 1.0f);
		break;
	case GLFW_KEY_A:
		char_position += vmath::vec3(-1.0f, 0.0f, 0.0f);
		break;
	case GLFW_KEY_D:
		char_position += vmath::vec3(1.0f, 0.0f, 0.0f);
		break;
	case GLFW_KEY_SPACE:
		char_position += vmath::vec3(0.0f, 1.0f, 0.0f);
		break;
	case GLFW_KEY_LEFT_SHIFT:
		char_position += vmath::vec3(0.0f, -1.0f, 0.0f);
		break;
	case GLFW_KEY_UP:
		char_rotation += vmath::vec3(5.0f, 0.0f, 0.0f);
		break;
	case GLFW_KEY_DOWN:
		char_rotation += vmath::vec3(-5.0f, 0.0f, 0.0f);
		break;
	case GLFW_KEY_LEFT:
		char_rotation += vmath::vec3(0.0f, 5.0f, 0.0f);
		break;
	case GLFW_KEY_RIGHT:
		char_rotation += vmath::vec3(0.0f, -5.0f, 0.0f);
		break;
	}

	char buf[256];
	sprintf(buf, "Position: (%.1f, %.1f, %.1f)\nRotation: (%.1f, %.1f, %.1f)\n\n", char_position[0], char_position[1], char_position[2], char_rotation[0], char_rotation[1], char_rotation[2]);
	OutputDebugString(buf);
}
