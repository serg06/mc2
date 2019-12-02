#include "render.h"
#include "shapes.h"
#include "util.h"

#include "GLFW/glfw3.h"

#include <string>
#include <tuple>
#include <vector>
#include <vmath.h>

using namespace vmath;

// setup glfw window
// windowInfo -> info for setting up this window
// window -> pointer to a window pointer, which we're gonna set up
void setup_glfw(GlfwInfo* windowInfo, GLFWwindow** window) {
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
	glfwWindowHint(GLFW_SAMPLES, windowInfo->msaa);

	// debug mode
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, windowInfo->debug);

	// create window
	*window = glfwCreateWindow(windowInfo->width, windowInfo->height, windowInfo->title.c_str(), nullptr, nullptr);

	if (!*window) {
		MessageBox(NULL, "Failed to create window.", "GLFW error", MB_OK);
		return;
	}

	// set this window as current window
	glfwMakeContextCurrent(*window);

	// lock mouse into screen, for camera controls
	glfwSetInputMode(*window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetInputMode(*window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

	// finally init gl3w
	if (gl3wInit()) {
		MessageBox(NULL, "Failed to initialize OpenGL.", "gl3w error", MB_OK);
		return;
	}

	// disable vsync
	glfwSwapInterval(0);
}

void setup_opengl(OpenGLInfo* glInfo) {
	const GLfloat(&cube)[108] = shapes::cube_full;

	// list of shaders to create program with
	// TODO: Embed these into binary somehow - maybe generate header file with cmake.
	std::vector <std::tuple<std::string, GLenum>> shader_fnames = {
		{ "../src/simple.vs.glsl", GL_VERTEX_SHADER },
		//{"../src/simple.tcs.glsl", GL_TESS_CONTROL_SHADER },
		//{"../src/simple.tes.glsl", GL_TESS_EVALUATION_SHADER },
		{ "../src/simple.gs.glsl", GL_GEOMETRY_SHADER },
		{ "../src/simple.fs.glsl", GL_FRAGMENT_SHADER },
	};

	// create program
	glInfo->rendering_program = compile_shaders(shader_fnames);

	/* OPENGL SETUP */

	/* HANDLE CUBES FIRST */

	// vao: create VAO for cube[s], so we can tell OpenGL how to use it when it's bound
	glCreateVertexArrays(1, &glInfo->vao_cube);

	// buffers: create
	glCreateBuffers(1, &glInfo->vert_buf);

	// buffers: allocate space
	glNamedBufferStorage(glInfo->vert_buf, sizeof(cube), NULL, GL_DYNAMIC_STORAGE_BIT); // allocate enough for all vertices, and allow editing

	// buffers: insert static data
	glNamedBufferSubData(glInfo->vert_buf, 0, sizeof(cube), cube); // vertex positions

	// vao: enable all cube's attributes, 1 at a time
	glEnableVertexArrayAttrib(glInfo->vao_cube, glInfo->position_attr_idx);
	glEnableVertexArrayAttrib(glInfo->vao_cube, glInfo->chunk_types_attr_idx);

	// vao: set up formats for cube's attributes, 1 at a time
	glVertexArrayAttribFormat(glInfo->vao_cube, glInfo->position_attr_idx, 3, GL_FLOAT, GL_FALSE, 0);
	glVertexArrayAttribIFormat(glInfo->vao_cube, glInfo->chunk_types_attr_idx, 1, GL_BYTE, 0);

	// vao: set binding points for all attributes, 1 at a time
	//      - 1 buffer per binding point; for clarity, to keep it clear, I should only bind 1 attr per binding point
	glVertexArrayAttribBinding(glInfo->vao_cube, glInfo->position_attr_idx, glInfo->vert_buf_bidx);
	glVertexArrayAttribBinding(glInfo->vao_cube, glInfo->chunk_types_attr_idx, glInfo->chunk_types_bidx);

	// vao: bind buffers to their binding points, 1 at a time
	glVertexArrayVertexBuffer(glInfo->vao_cube, glInfo->vert_buf_bidx, glInfo->vert_buf, 0, sizeof(vec3));

	// vao: extra properties
	glBindVertexArray(glInfo->vao_cube);
	glVertexAttribDivisor(glInfo->chunk_types_attr_idx, 1);
	glBindVertexArray(0);

	/* HANDLE UNIFORM NOW */

	// create buffers
	glCreateBuffers(1, &glInfo->trans_buf);

	// bind them
	glBindBufferBase(GL_UNIFORM_BUFFER, glInfo->trans_buf_uni_bidx, glInfo->trans_buf); // bind transformation buffer to uniform buffer binding point

	// allocate
	glNamedBufferStorage(glInfo->trans_buf, sizeof(mat4) * 2 + sizeof(ivec3), NULL, GL_DYNAMIC_STORAGE_BIT); // allocate 2 matrices of space for transforms, and allow editing

	/* ETC */

	glPointSize(5.0f);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CW);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);


	// use our program object for rendering
	glUseProgram(glInfo->rendering_program);
}
