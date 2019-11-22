#include <iostream>
#include "GL/gl3w.h"
#include "GLFW/glfw3.h"

using namespace std;

//int main() {
//	cout << "Hello world! (Press enter)" << endl;
//	getchar();
//}

int main() {
	if (!glfwInit()) {
		printf("failed to initialize GLFW.\n");
		return -1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	auto window = glfwCreateWindow(1000, 600, "awesome", nullptr, nullptr);
	if (!window) {
		return -1;
	}

	glfwMakeContextCurrent(window);
	if (gl3wInit()) {
		printf("failed to initialize OpenGL\n");
		return -1;
	}

	printf("OpenGL %s, GLSL %s\n", glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));

	Sleep(30000);

	// ...

	return 0;
}
