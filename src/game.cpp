#include "game.h"

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

Game::Game(std::shared_ptr<zmq::context_t> ctx_, std::shared_ptr<GLFWwindow> window_, std::shared_ptr<GlfwInfo> windowInfo_, std::shared_ptr<OpenGLInfo> glInfo_)
	: ctx(ctx_), bus(ctx_), window(window_), windowInfo(windowInfo_), glInfo(glInfo_)
{
	bus.out.setsockopt(ZMQ_SUBSCRIBE, "", 0);
}

Game::~Game()
{
	// Send exit message
	std::vector<zmq::const_buffer> message({
		zmq::buffer(msg::EXIT)
		});

	auto ret = zmq::send_multipart(bus.in, message, zmq::send_flags::dontwait);
	assert(ret);
}

void Game::startup()
{
	// TODO: Launch other threads first

	// set vars
	std::fill(held_keys.begin(), held_keys.end(), false);
	world = std::make_unique<World>(ctx);
	world_render = std::make_unique<WorldRenderPart>(ctx);
	glfwGetCursorPos(window.get(), &last_mouse_x, &last_mouse_y); // reset mouse position

	running = true;
}

void Game::shutdown()
{
	running = false;
}

void Game::render_frame(bool& quit)
{
	float time = static_cast<float>(glfwGetTime());
	switch (state)
	{
	case GameState::InGame:
		update_player_actions();
		world->update_world(time);
		render(time);
		break;
	case GameState::InEscMenu:
		render_esc_menu(quit);
		break;
	default:
		assert(false);
		break;
	}
}

void Game::render_esc_menu(bool& quit)
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
		if (ImGui::Button("Quit Game"))
		{
			quit = true;
		}
	}
	ImGui::End();

	// Clean up style
	ImGui::PopStyleVar();
}

void Game::render(float time)
{
	// FBO: BIND
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, glInfo->fbo_out.get_fbo());

	const auto start_of_fn = std::chrono::high_resolution_clock::now();

	// change in time
	const float dt = time - last_render_time;
	last_render_time = time;
	fps = (1 - 5 * dt) * fps + 5;

	const auto direction = get_player().staring_direction();

	/* TRANSFORMATION MATRICES */

	// Create Model->World matrix
	const mat4 model_world_matrix =
		translate(0.0f, -CAMERA_HEIGHT, 0.0f);

	// Create World->View matrix
	const mat4 world_view_matrix =
		rotate_pitch_yaw(get_player().pitch, get_player().yaw) *
		translate(-get_player().coords[0], -get_player().coords[1], -get_player().coords[2]); // move relative to you

	// Combine them into Model->View matrix
	const mat4 model_view_matrix = world_view_matrix * model_world_matrix;

	// Update projection matrix too, in case if width/height changed
	// NOTE: Careful, if (nearplane/farplane) ratio is too small, things get fucky.
	const mat4 proj_matrix = perspective(
		static_cast<float>(windowInfo->vfov), // virtual fov
		static_cast<float>(windowInfo->width) / static_cast<float>(windowInfo->height), // aspect ratio
		(PLAYER_HEIGHT - CAMERA_HEIGHT) * 1 / sqrtf(2.0f), // see blocks no matter how close they are
		64 * CHUNK_WIDTH // only support 64 chunks for now
	);

	/* BACKGROUND / SKYBOX */

	// Clear color/depth buffers
	const GLfloat one = 1.0f;
	glClearBufferfv(GL_COLOR, 0, color_sky_blue);
	glClearBufferfv(GL_DEPTH, 0, &one);

	// check if in water
	const GLuint in_water_gluint = static_cast<GLuint>(get_player().in_water);

	// Update transformation buffer with matrices
	glNamedBufferSubData(glInfo->trans_uni_buf, 0, sizeof(model_view_matrix), model_view_matrix);
	glNamedBufferSubData(glInfo->trans_uni_buf, sizeof(model_view_matrix), sizeof(proj_matrix), proj_matrix); // proj matrix
	glNamedBufferSubData(glInfo->trans_uni_buf, sizeof(model_view_matrix) + sizeof(proj_matrix), sizeof(GLuint), &in_water_gluint); // proj matrix

	// extract projection matrix planes
	vec4 planes[6];
	extract_planes_from_projmat(proj_matrix, model_view_matrix, planes);

	// Draw ALL our chunks!
	world_render->handle_messages();
	world_render->render(glInfo.get(), windowInfo.get(), planes, get_player().staring_at);

	// get polygon mode
	GLint polygon_mode;
	glGetIntegerv(GL_POLYGON_MODE, &polygon_mode);

	if (polygon_mode == GL_FILL && should_fix_tjunctions) {
		// FBO: FIX TJUNCTIONS AND OUTPUT TO DEFAULT FRAMEBUFFER
		fix_tjunctions(glInfo.get(), windowInfo.get(), 0, glInfo->fbo_out);
	}
	else {
		// FBO: COPY TO DEFAULT FRAMEBUFFER
		glBlitNamedFramebuffer(glInfo->fbo_out.get_fbo(), 0, 0, 0, windowInfo->width, windowInfo->height, 0, 0, windowInfo->width, windowInfo->height, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	}

	// display debug info
	if (show_debug_info)
		render_debug_info(dt);

	// make sure rendering didn't take too long
	const auto end_of_fn = std::chrono::high_resolution_clock::now();
	const long result_total = std::chrono::duration_cast<std::chrono::microseconds>(end_of_fn - start_of_fn).count();
#ifdef _DEBUG
	if (result_total / 1000.0f > 50) {
		std::stringstream buf;
		buf << "TOTAL GAME::render TIME: " << result_total / 1000.0f << "ms\n";
		OutputDebugString(buf.str().c_str());
	}
#endif // _DEBUG
}

void Game::render_debug_info(float dt)
{
	// Prep debug info string
	std::string debugInfo;
	debugInfo.reserve(500);

	char lineBuf[256];
	const auto direction = get_player().staring_direction();

	sprintf(lineBuf, "FPS: %-4.1f (%d ms) (%d distance)\n", fps, static_cast<int>(dt * 1000), get_player().render_distance);
	debugInfo += lineBuf;

	sprintf(lineBuf, "Position: (%6.1f, %6.1f, %6.1f)\n", get_player().coords[0], get_player().coords[1], get_player().coords[2]);
	debugInfo += lineBuf;

	sprintf(lineBuf, "Facing:   (%6.1f, %6.1f, %6.1f)\n", direction[0], direction[1], direction[2]);
	debugInfo += lineBuf;

	sprintf(lineBuf, "Looking at block: (%d, %d, %d)\n", get_player().staring_at[0], get_player().staring_at[1], get_player().staring_at[2]);
	debugInfo += lineBuf;

	sprintf(lineBuf, "Face in water: %s\n", get_player().in_water ? "true" : "false");
	debugInfo += lineBuf;

	sprintf(lineBuf, "Held block: %d (%s)\n", static_cast<int>(get_player().held_block), get_player().held_block.side_texture().c_str());
	debugInfo += lineBuf;

	// Show debug info
	const float DISTANCE = 10.0f;
	static int corner = 0;
	ImGuiIO& io = ImGui::GetIO();
	if (corner != -1)
	{
		ImVec2 window_pos = ImVec2((corner & 1) ? io.DisplaySize.x - DISTANCE : DISTANCE, (corner & 2) ? io.DisplaySize.y - DISTANCE : DISTANCE);
		ImVec2 window_pos_pivot = ImVec2((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);
		ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
	}
	ImGui::SetNextWindowBgAlpha(0.0f); // Transparent background
	if (ImGui::Begin("Debug info", nullptr, (corner != -1 ? ImGuiWindowFlags_NoMove : 0) | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
	{
		ImGui::Text(debugInfo.c_str());
	}
	ImGui::End();
}

void Game::update_player_actions()
{
	get_player().actions.forwards = held_keys[GLFW_KEY_W];
	get_player().actions.backwards = held_keys[GLFW_KEY_S];
	get_player().actions.left = held_keys[GLFW_KEY_A];
	get_player().actions.right = held_keys[GLFW_KEY_D];
	get_player().actions.jumping = held_keys[GLFW_KEY_SPACE];
	get_player().actions.shifting = held_keys[GLFW_KEY_LEFT_SHIFT];
	// TODO: Mining
}

Player& Game::get_player()
{
	return world->player;
}

void Game::onKey(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// ignore unknown keys
	if (key == GLFW_KEY_UNKNOWN) {
		return;
	}

	// handle key presses
	if (action == GLFW_PRESS) {
		held_keys[key] = true;

		// N = toggle noclip
		if (key == GLFW_KEY_N) {
			get_player().noclip = !get_player().noclip;
		}

		// P = cycle poylgon mode
		if (key == GLFW_KEY_P) {
			GLint polygon_mode;
			glGetIntegerv(GL_POLYGON_MODE, &polygon_mode);
			switch (polygon_mode) {
			case GL_FILL:
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				break;
			case GL_LINE:
				glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
				break;
			case GL_POINT:
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
				break;
			}
		}

		// C = toggle face culling
		if (key == GLFW_KEY_C) {
			GLboolean is_enabled = glIsEnabled(GL_CULL_FACE);
			if (is_enabled) {
				glDisable(GL_CULL_FACE);
			}
			else {
				glEnable(GL_CULL_FACE);
			}
		}

		// F3 = toggle debug info
		if (key == GLFW_KEY_F3) {
			show_debug_info = !show_debug_info;
		}

		// [F11 | ALT+ENTER] = toggle fullscreen
		if (key == GLFW_KEY_F11 || (mods == GLFW_MOD_ALT && key == GLFW_KEY_ENTER)) {
			// if fullscreen
			if (glfwGetWindowMonitor(window)) {
				// remove fullscreen
				glfwSetWindowMonitor(window, NULL,
					windowInfo->xpos_before_fullscreen, windowInfo->ypos_before_fullscreen,
					windowInfo->width_before_fullscreen, windowInfo->height_before_fullscreen,
					0);
			}

			// if not fullscreen
			else {
				GLFWmonitor* monitor = glfwGetPrimaryMonitor();
				if (monitor) {
					// save current window position/location
					glfwGetWindowPos(window, &windowInfo->xpos_before_fullscreen, &windowInfo->ypos_before_fullscreen);
					glfwGetWindowSize(window, &windowInfo->width_before_fullscreen, &windowInfo->height_before_fullscreen);

					// enable fullscreen
					const GLFWvidmode* mode = glfwGetVideoMode(monitor);
					glfwSetWindowMonitor(window, monitor,
						0, 0,
						mode->width, mode->height,
						GLFW_DONT_CARE); // TODO: test that GLFWL_DONT_CARE means uncapped framerate (with Windows game bar)
				}
			}
		}

		// T = toggle t-junction fixing
		if (key == GLFW_KEY_T) {
			should_fix_tjunctions = !should_fix_tjunctions;
		}

		// R = toggle raw input
		if (key == GLFW_KEY_R) {
			if (glfwRawMouseMotionSupported()) {
				const bool raw_motion = glfwGetInputMode(window, GLFW_RAW_MOUSE_MOTION);
				glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, !raw_motion);
			}
		}

		// M = toggle mouse cursor
		if (key == GLFW_KEY_M)
		{
			if (capture_mouse)
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			else
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			capture_mouse = !capture_mouse;
		}

		// ESC = toggle ESC menu
		if (key == GLFW_KEY_ESCAPE)
		{
			switch (state)
			{
			case GameState::InGame:
				state = GameState::InEscMenu;
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
				glfwSwapInterval(1); // vsync on
				break;
			case GameState::InEscMenu:
				state = GameState::InGame;
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
				glfwSwapInterval(0); // vsync off
				break;
			default:
				assert(false);
				break;
			}
		}
	}

	// handle optionally-repeatable key presses
	if (action == GLFW_PRESS || action == GLFW_REPEAT) {
		// + = increase render distance
		if (key == GLFW_KEY_KP_ADD || key == GLFW_KEY_EQUAL) {
			get_player().render_distance++;
			get_player().should_check_for_nearby_chunks = true;
		}

		// - = decrease render distance
		if (key == GLFW_KEY_KP_SUBTRACT || key == GLFW_KEY_MINUS) {
			if (get_player().render_distance > 0) {
				get_player().render_distance--;
			}
		}
	}

	// handle key releases
	if (action == GLFW_RELEASE) {
		held_keys[key] = false;
	}
}

void Game::onMouseMove(GLFWwindow* window, double x, double y)
{
	if (capture_mouse && state == GameState::InGame)
	{
		// bonus of using deltas for yaw/pitch:
		// - can cap easily -- if we cap without deltas, and we move 3000x past the cap, we'll have to move 3000x back before mouse moves!
		// - easy to do mouse sensitivity
		double delta_x = x - last_mouse_x;
		double delta_y = y - last_mouse_y;

		// update pitch/yaw
		get_player().yaw += static_cast<float>(windowInfo->mouseX_Sensitivity * delta_x);
		get_player().pitch += static_cast<float>(windowInfo->mouseY_Sensitivity * delta_y);
		
		// wrap yaw
		get_player().yaw = posmod(get_player().yaw, 360.0f);

		// cap pitch
		get_player().pitch = clamp(get_player().pitch, -90.0f, 90.0f);

		// update old values
		last_mouse_x = x;
		last_mouse_y = y;
	}
}

void Game::onResize(GLFWwindow* window, int width, int height)
{
}

void Game::onMouseButton(int button, int action) {
	if (capture_mouse)
	{
		// left click
		if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
			// if staring at valid block
			if (get_player().staring_at[1] >= 0) {
				world->data.destroy_block(get_player().staring_at);
			}
		}

		// right click
		if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
			// if staring at valid block
			if (get_player().staring_at[1] >= 0) {
				// position we wanna place block at
				ivec3 desired_position = get_player().staring_at + get_player().staring_at_face;

				// check if we're in the way
				vector<ivec4> intersecting_blocks = get_player_intersecting_blocks(get_player().coords);
				auto result = find_if(begin(intersecting_blocks), end(intersecting_blocks), [desired_position](const auto& ipos) {
					return desired_position == ivec3(ipos[0], ipos[1], ipos[2]);
					});

				// if we're not in the way, place it
				if (result == end(intersecting_blocks)) {
					world->data.add_block(desired_position, get_player().held_block);
				}
			}
		}
	}
}

void Game::onMouseWheel(double scroll_direction) {
	if (capture_mouse)
	{
		// scrolled up   => scroll_direction > 0
		// scrolled down => scroll_direction < 0
		assert(scroll_direction != 0 && "onMouseWheel called with invalid scroll direction");

		// increment/decrement block type
		int scroll_offset = scroll_direction > 0 ? 1 : -1;
		get_player().held_block = BlockType(static_cast<int>(get_player().held_block) + scroll_offset);
	}
}
