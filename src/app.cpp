#include "app.h"

#include "chunk.h"
#include "chunkdata.h"
#include "messaging.h"
#include "render.h"
#include "shapes.h"
#include "unique_queue.h"
#include "util.h"
#include "world_meshing.h"
#include "world_render.h"

#include "examples/imgui_impl_opengl3.h"
#include "examples/imgui_impl_glfw.h"
#include "imgui.h"
#include "zmq_addon.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <future>
#include <memory>
#include <numeric>
#include <string>

// 1. TODO: Apply C++11 features
// 2. TODO: Apply C++14 features
// 3. TODO: Apply C++17 features

using namespace std;
using namespace vmath;

void run_game(std::shared_ptr<zmq::context_t> ctx)
{
	glfwSetErrorCallback(glfw_onError);
	App app(ctx);
	app.run();
}

App::App(std::shared_ptr<zmq::context_t> ctx_)
	: ctx(ctx_), bus(ctx_), windowInfo(std::make_shared<GlfwInfo>()),
	glInfo(std::make_shared<OpenGLInfo>())
{
	bus.out.setsockopt(ZMQ_SUBSCRIBE, "", 0);
}

App::~App()
{
	// Send exit message
	std::vector<zmq::const_buffer> message({
		zmq::buffer(msg::EXIT)
		});

	auto ret = zmq::send_multipart(bus.in, message, zmq::send_flags::dontwait);
	assert(ret);
}

void App::run()
{
	// create glfw window
	GLFWwindow* tmp;
	setup_glfw(windowInfo.get(), &tmp);
	window.reset(tmp);

	// point window to app for glfw callbacks
	glfwSetWindowUserPointer(window.get(), this);

	// set callbacks
	glfwSetWindowSizeCallback(window.get(), glfw_onResize);
	glfwSetKeyCallback(window.get(), glfw_onKey);
	glfwSetMouseButtonCallback(window.get(), glfw_onMouseButton);
	glfwSetCursorPosCallback(window.get(), glfw_onMouseMove);
	glfwSetScrollCallback(window.get(), glfw_onMouseWheel);

	// set debug message callback
	if (windowInfo->debug) {
		if (gl3wIsSupported(4, 3))
		{
			glEnable(GL_DEBUG_OUTPUT);
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			glDebugMessageCallback((GLDEBUGPROC)gl_onDebugMessage, this);
			glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
		}
	}

	// Start up app
	startup();

	// run until user presses ESC or tries to close window
	game->startup();
	while ((glfwGetKey(window.get(), GLFW_KEY_ESCAPE) == GLFW_RELEASE) && (!glfwWindowShouldClose(window.get()))) {
		game->run_loop();
	}
	game->shutdown();

	// Send exit message
	bus.in.send(zmq::buffer(msg::EXIT));

	shutdown();
}

void App::startup()
{
	// prepare opengl
	setup_opengl(windowInfo.get(), glInfo.get());

	/* IMGUI */

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;
	colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 0.94f);
	//style.Colors[ImGuiCol_PopupBg] = ImVec4(1, 0, 0, 0.94f);
	//style.Colors[ImGuiCol_FrameBg] = ImVec4(1, 0, 0, 0.54f);
	colors[ImGuiCol_Border] = ImVec4(0, 0, 0, 0);
	style.WindowRounding = 0.0f;
	//style.ChildRounding = 0.0f;
	//style.FrameRounding = 0.0f;
	//style.GrabRounding = 0.0f;
	//style.PopupRounding = 0.0f;
	//style.ScrollbarRounding = 0.0f;
	//ImGui::StyleColorsClassic();

	colors[ImGuiCol_Button] = ImVec4(111/255.0f, 111/255.0f, 111/255.0f, 1);

	// TODO: instead of this, write custom button renderer that changes border on highlight (like MC)
	//   (Need to override ButtonBehaviour or something?)
	if constexpr (true)
	{
		colors[ImGuiCol_ButtonHovered] = ImVec4(131 / 255.0f, 131 / 255.0f, 131 / 255.0f, 1);
		colors[ImGuiCol_ButtonActive] = ImVec4(151 / 255.0f, 151 / 255.0f, 151 / 255.0f, 1);
	}

	// Load Fonts
	io.Fonts->AddFontFromFileTTF("font/minecraft.otf", 7.0f * 4);

	// Setup Platform/Renderer bindings
	ImGui_ImplOpenGL3_Init();
	ImGui_ImplGlfw_InitForOpenGL(window.get(), true);

	// Game
	game = std::make_shared<Game>(ctx, window, windowInfo, glInfo);
}

void App::shutdown()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
}

void App::onKey(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// ignore unknown keys
	if (key == GLFW_KEY_UNKNOWN) {
		return;
	}

	if (game->running)
		game->onKey(window, key, scancode, action, mods);
}

void App::onMouseMove(GLFWwindow* window, double x, double y)
{
	if (game->running)
		game->onMouseMove(window, x, y);
}

void App::onResize(GLFWwindow* window, int width, int height)
{
	// if minimized, don't wanna break stuff
	if (width == 0 || height == 0) {
		return;
	}

	windowInfo->width = width;
	windowInfo->height = height;

	opengl_on_resize(*glInfo, width, height);
}

void App::onMouseButton(int button, int action)
{
	if (game->running)
		game->onMouseButton(button, action);
}

void App::onMouseWheel(double scroll_direction)
{
	if (game->running)
		game->onMouseWheel(scroll_direction);
}

void App::onDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message)
{
	char buf[4096];
	char* bufp = buf;

	// ignore non-significant error/warning codes (e.g. 131185 = "buffer created")
	if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return;

	bufp += sprintf(bufp, "---------------\n");
	bufp += sprintf(bufp, "OpenGL debug message (%d): %s\n", id, message);

	switch (source)
	{
	case GL_DEBUG_SOURCE_API:             bufp += sprintf(bufp, "Source: API"); break;
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   bufp += sprintf(bufp, "Source: Window System"); break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER: bufp += sprintf(bufp, "Source: Shader Compiler"); break;
	case GL_DEBUG_SOURCE_THIRD_PARTY:     bufp += sprintf(bufp, "Source: Third Party"); break;
	case GL_DEBUG_SOURCE_APPLICATION:     bufp += sprintf(bufp, "Source: Application"); break;
	case GL_DEBUG_SOURCE_OTHER:           bufp += sprintf(bufp, "Source: Other"); break;
	} bufp += sprintf(bufp, "\n");

	switch (type)
	{
	case GL_DEBUG_TYPE_ERROR:               bufp += sprintf(bufp, "Type: Error"); break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: bufp += sprintf(bufp, "Type: Deprecated Behaviour"); break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  bufp += sprintf(bufp, "Type: Undefined Behaviour"); break;
	case GL_DEBUG_TYPE_PORTABILITY:         bufp += sprintf(bufp, "Type: Portability"); break;
	case GL_DEBUG_TYPE_PERFORMANCE:         bufp += sprintf(bufp, "Type: Performance"); break;
	case GL_DEBUG_TYPE_MARKER:              bufp += sprintf(bufp, "Type: Marker"); break;
	case GL_DEBUG_TYPE_PUSH_GROUP:          bufp += sprintf(bufp, "Type: Push Group"); break;
	case GL_DEBUG_TYPE_POP_GROUP:           bufp += sprintf(bufp, "Type: Pop Group"); break;
	case GL_DEBUG_TYPE_OTHER:               bufp += sprintf(bufp, "Type: Other"); break;
	} bufp += sprintf(bufp, "\n");

	switch (severity)
	{
	case GL_DEBUG_SEVERITY_HIGH:         bufp += sprintf(bufp, "Severity: high"); break;
	case GL_DEBUG_SEVERITY_MEDIUM:       bufp += sprintf(bufp, "Severity: medium"); break;
	case GL_DEBUG_SEVERITY_LOW:          bufp += sprintf(bufp, "Severity: low"); break;
	case GL_DEBUG_SEVERITY_NOTIFICATION: bufp += sprintf(bufp, "Severity: notification"); break;
	} bufp += sprintf(bufp, "\n");
	bufp += sprintf(bufp, "\n");

	OutputDebugString(buf);
	exit(-1);
}

namespace {
	/* GLFW/GL callback functions */

	void glfw_onError(int error, const char* description)
	{
		MessageBox(NULL, description, "GLFW error", MB_OK);
	}

	void glfw_onKey(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
		app->onKey(window, key, scancode, action, mods);
	}

	void glfw_onMouseMove(GLFWwindow* window, double x, double y)
	{
		App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
		app->onMouseMove(window, x, y);
	}

	void glfw_onResize(GLFWwindow* window, int width, int height)
	{
		App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
		app->onResize(window, width, height);
	}

	void glfw_onMouseButton(GLFWwindow* window, int button, int action, int mods)
	{
		App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
		app->onMouseButton(button, action);
	}

	static void glfw_onMouseWheel(GLFWwindow* window, double xoffset, double yoffset)
	{
		App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
		app->onMouseWheel(yoffset);
	}

	void APIENTRY gl_onDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, GLvoid* userParam)
	{
		App* app = static_cast<App*>(userParam);
		app->onDebugMessage(source, type, id, severity, length, message);
	}
}
