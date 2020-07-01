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

bool App::draw_main_menu()
{
	// Clear background
	glClearBufferfv(GL_COLOR, 0, color_black);

	// Setup style
	ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, { 0.5f, 0.5f });

	// Create window in center of screen
	ImVec2 window_pos = { ImGui::GetIO().DisplaySize.x / 2, ImGui::GetIO().DisplaySize.y / 2 };
	ImVec2 window_pos_pivot = { 0.5f, 0.5f };
	ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
	auto flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse;
	if (ImGui::Begin("Main menu", nullptr, flags))
	{
		// Draw buttons
		if (ImGui::Button("Single Player"))
		{
			on_start_game();
		}
		if (ImGui::Button("Options"))
		{
		}
		if (ImGui::Button("Quit Game"))
		{
			on_quit_game();
		}
	}
	ImGui::End();

	// Clean up style
	ImGui::PopStyleVar();

	return true;
}

void App::draw_game()
{
	bool quitting;
	game->render_frame(quitting);
	if (quitting)
		state = AppState::Quitting;
}

void App::on_start_game()
{
	state = AppState::InGame;
	game = std::make_shared<Game>(ctx, window, windowInfo, glInfo);
	game->startup();
	glfwSetInputMode(window.get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSwapInterval(0); // disable vsync
}

void App::on_quit_game()
{
	state = AppState::Quitting;
	if (game)
	{
		game->shutdown();
		game.reset();
	}
}

void App::on_enter_main_menu()
{
	state = AppState::InMainMenu;
	glfwSetInputMode(window.get(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	glfwSwapInterval(1); // enable vsync
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
	while (state != AppState::Quitting && (!glfwWindowShouldClose(window.get()))) {
		// Dear ImGui frame setup
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Draw a frame to default framebuffer
		draw_frame();

		// Render ImGui output to default framebuffer
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// Swap buffers
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		glfwSwapBuffers(window.get());
		glfwPollEvents();
	}
	state = AppState::Quitting;

	// Send exit message
	bus.in.send(zmq::buffer(msg::EXIT));

	shutdown();
}

void App::draw_frame()
{
	switch (state)
	{
	case AppState::InGame:
		draw_game();
		break;
	case AppState::InMainMenu:
		draw_main_menu();
		break;
	default:
		assert(false);
		break;
	}
}

void App::setup_imgui()
{
	// One-time init
	IMGUI_CHECKVERSION();

	// Create context
	ImGui::CreateContext();
	//main_menu_ctx.reset();
	//ImGui::SetCurrentContext(main_menu_ctx.get());
	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->AddFontFromFileTTF("font/minecraft.otf", 7.0f * 4);
	ImGui::StyleColorsDark();

	// Init [current context?]
	ImGui_ImplOpenGL3_Init();
	ImGui_ImplGlfw_InitForOpenGL(window.get(), true);
}

void App::startup()
{
	// prepare opengl
	setup_opengl(windowInfo.get(), glInfo.get());

	// prepare imgui
	setup_imgui();

	// Enter main menu
	on_enter_main_menu();
}

void App::shutdown()
{
	// TODO: OpenGL shutdown? GLFW shutdown?
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
}

void App::onKey(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// ignore unknown keys
	if (key == GLFW_KEY_UNKNOWN)
		return;

	if (game && game->running)
		game->onKey(window, key, scancode, action, mods);
}

void App::onMouseMove(GLFWwindow* window, double x, double y)
{
	if (game && game->running)
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
	if (game && game->running)
		game->onMouseButton(button, action);
}

void App::onMouseWheel(double scroll_direction)
{
	if (game && game->running)
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
