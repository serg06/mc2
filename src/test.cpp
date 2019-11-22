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

// TODO: Make everything more object-oriented.
// That way, I can define functions without having to declare them first, and shit.
// And more good shit comes of it too.
// Then from WinMain(), just call MyApp a = new MyApp, a.run(); !!

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	return main();
}

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
	//glfwSetKeyCallback(window, glfw_onKey);
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

}

void render(double time) {

}

void shutdown() {

}


