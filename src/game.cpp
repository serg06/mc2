#include "game.h"

#include "chunk.h"
#include "chunkdata.h"
#include "render.h"
#include "shapes.h"
#include "util.h"

#include "cmake_pch.hxx"

#include <algorithm>
#include <assert.h>
#include <chrono>
#include <cmath>
#include <future>
#include <math.h>
#include <memory>
#include <mutex>          // std::mutex
#include <numeric>
#include <string>

// 1. TODO: Apply C++11 features
// 2. TODO: Apply C++14 features
// 3. TODO: Apply C++17 features

using namespace std;
using namespace vmath;

namespace {
	static bool stop = false;
}

// Windows main
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	glfwSetErrorCallback(glfw_onError);
	App::app = make_unique<App>();
	App::app->run();
}

// thread for generating new chunk meshes
void ChunkGenThread() {
	if (App::app == nullptr) {
		throw "No global app.";
	}

	// get global app
	App* app = App::app.get();

	// run thread until stopped
	while (!stop)
	{
		// wait until queue is non-empty
		while (app->world->mesh_gen_queue.size() == 0)
		{
			// acquire lock
			std::unique_lock<std::mutex> mesh_lock(app->world->mesh_gen_mutex);

			// wait
			app->world->mesh_gen_cv.wait(mesh_lock);

			// if woken up to end thread, exit
			if (stop) return;
		}

		// generate a mesh if possible
		app->world->gen_minichunk_mesh_from_queue(vec3(app->char_position[0], app->char_position[1], app->char_position[2]));
	}
}

void App::run() {
	// create glfw window
	setup_glfw(&windowInfo, &window);

	// set callbacks
	glfwSetWindowSizeCallback(window, glfw_onResize);
	glfwSetKeyCallback(window, glfw_onKey);
	glfwSetMouseButtonCallback(window, glfw_onMouseButton);
	glfwSetCursorPosCallback(window, glfw_onMouseMove);
	glfwSetScrollCallback(window, glfw_onMouseWheel);

	// set debug message callback
	if (windowInfo.debug) {
		if (gl3wIsSupported(4, 3))
		{
			glEnable(GL_DEBUG_OUTPUT);
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			glDebugMessageCallback((GLDEBUGPROC)gl_onDebugMessage, nullptr);
			glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
		}
	}

	// Start up app
	startup();

	// Spawn mesh generation threads
	vector<std::future<void>> chunk_gen_futures;
	for (int i = 0; i < NUM_MESH_GEN_THREADS; i++) {
		chunk_gen_futures.push_back(std::async(std::launch::async, ChunkGenThread));
	}

	// run until user presses ESC or tries to close window
	last_render_time = (float)glfwGetTime(); // updated in render()
	while ((glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_RELEASE) && (!glfwWindowShouldClose(window))) {
		// run rendering function
		render((float)glfwGetTime());

		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		glfwSwapBuffers(window);

		// poll window system for events
		glfwPollEvents();
	}

	// Stop all other threads
	stop = true;
	world->mesh_gen_cv.notify_all(); // wake up any sleeping threads so they can exit
	for (auto &fut : chunk_gen_futures) {
		// keep waking them up until they get their lazy butts up and shut down
		while (fut.wait_for(std::chrono::milliseconds(100)) == future_status::timeout) {
			world->mesh_gen_cv.notify_all();
		}
	}

	shutdown();
}

void App::startup() {
	// set vars
	memset(held_keys, false, sizeof(held_keys));
	glfwGetCursorPos(window, &last_mouse_x, &last_mouse_y); // reset mouse position
	world = new World();

	// prepare opengl
	setup_opengl(&windowInfo, &glInfo);

	// run tests
	WorldTests::run_all_tests(&glInfo);
}

void App::render(float time) {
	// FBO: BIND
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, glInfo.fbo_out.get_fbo());

	char buf[256];

	const auto start_of_fn = std::chrono::high_resolution_clock::now();

	// change in time
	const float dt = time - last_render_time;
	last_render_time = time;
	fps = (1 - 5 * dt) * fps + 5;
	world->update_tick((int)floorf(time * 20));

	/* CHANGES IN WORLD */

	// update player movement
	update_player_movement(dt);

	// generate nearby chunks
	world->gen_nearby_chunks(char_position, min_render_distance);

	// update block that player is staring at
	const auto direction = staring_direction();
	world->raycast(char_position + vec4(0, CAMERA_HEIGHT, 0, 0), direction, 40, &staring_at, &staring_at_face, [this](const ivec3 &coords, const ivec3& face) {
		const auto block = this->world->get_type(coords);
		return block.is_solid();
	});

	/* TRANSFORMATION MATRICES */

	// Create Model->World matrix
	const mat4 model_world_matrix =
		translate(0.0f, -CAMERA_HEIGHT, 0.0f);

	// Create World->View matrix
	const mat4 world_view_matrix =
		rotate_pitch_yaw(char_pitch, char_yaw) *
		translate(-char_position[0], -char_position[1], -char_position[2]); // move relative to you

	// Combine them into Model->View matrix
	const mat4 model_view_matrix = world_view_matrix * model_world_matrix;

	// Update projection matrix too, in case if width/height changed
	// NOTE: Careful, if (nearplane/farplane) ratio is too small, things get fucky.
	const mat4 proj_matrix = perspective(
		(float)windowInfo.vfov, // virtual fov
		(float)windowInfo.width / (float)windowInfo.height, // aspect ratio
		(PLAYER_HEIGHT - CAMERA_HEIGHT) * 1 / sqrtf(2.0f), // see blocks no matter how close they are
		64 * CHUNK_WIDTH // only support 64 chunks for now
	);

	/* BACKGROUND / SKYBOX */

	// Clear color/depth buffers
	const GLfloat one = 1.0f;
	glClearBufferfv(GL_COLOR, 0, color_sky_blue);
	glClearBufferfv(GL_DEPTH, 0, &one);

	// check if in water
	const BlockType face_block = world->get_type(vec2ivec(char_position + vec4(0, CAMERA_HEIGHT, 0, 0)));
	const GLuint in_water = face_block == BlockType::StillWater || face_block == BlockType::FlowingWater;

	// Update transformation buffer with matrices
	glNamedBufferSubData(glInfo.trans_uni_buf, 0, sizeof(model_view_matrix), model_view_matrix);
	glNamedBufferSubData(glInfo.trans_uni_buf, sizeof(model_view_matrix), sizeof(proj_matrix), proj_matrix); // proj matrix
	glNamedBufferSubData(glInfo.trans_uni_buf, sizeof(model_view_matrix) + sizeof(proj_matrix), sizeof(GLuint), &in_water); // proj matrix

	// extract projection matrix planes
	vec4 planes[6];
	extract_planes_from_projmat(proj_matrix, model_view_matrix, planes);

	// Draw ALL our chunks!
	world->render(&glInfo, planes);

	// highlight block we're staring at, if it's valid
	if (staring_at[1] >= 0) {
		world->highlight_block(&glInfo, &windowInfo, staring_at);
	}

	// display debug info
	if (show_debug_info) {
		char buf[256];
		const ivec2 screen_dimensions = { windowInfo.width, windowInfo.height };
		int y_offset = 0;

		sprintf(buf, "FPS: %-4.1f (%d ms) (%d distance)", fps, (int)(dt * 1000), min_render_distance);
		render_text(&glInfo, { 0, y_offset++ }, screen_dimensions, buf, strlen(buf));

		sprintf(buf, "Position: (%6.1f, %6.1f, %6.1f)", char_position[0], char_position[1], char_position[2]);
		render_text(&glInfo, { 0, y_offset++ }, screen_dimensions, buf, strlen(buf));

		sprintf(buf, "Facing:   (%6.1f, %6.1f, %6.1f)", direction[0], direction[1], direction[2]);
		render_text(&glInfo, { 0, y_offset++ }, screen_dimensions, buf, strlen(buf));

		sprintf(buf, "Looking at block: (%d, %d, %d)", staring_at[0], staring_at[1], staring_at[2]);
		render_text(&glInfo, { 0, y_offset++ }, screen_dimensions, buf, strlen(buf));

		sprintf(buf, "Face in water: %d", (int)in_water);
		render_text(&glInfo, { 0, y_offset++ }, screen_dimensions, buf, strlen(buf));

		sprintf(buf, "Held block: %d (%s)", (int)held_block, held_block.side_texture().c_str());
		render_text(&glInfo, { 0, y_offset++ }, screen_dimensions, buf, strlen(buf));
	}

	// get polygon mode
	GLint polygon_mode;
	glGetIntegerv(GL_POLYGON_MODE, &polygon_mode);

	if (polygon_mode == GL_FILL && should_fix_tjunctions) {
		// FBO: FIX TJUNCTIONS AND OUTPUT TO DEFAULT FRAMEBUFFER
		fix_tjunctions(&glInfo, &windowInfo, 0, glInfo.fbo_out);
	}
	else {
		// FBO: COPY TO DEFAULT FRAMEBUFFER
		glBlitNamedFramebuffer(glInfo.fbo_out.get_fbo(), 0, 0, 0, windowInfo.width, windowInfo.height, 0, 0, windowInfo.width, windowInfo.height, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	}

	// make sure rendering didn't take too long
	const auto end_of_fn = std::chrono::high_resolution_clock::now();
	const long result_total = std::chrono::duration_cast<std::chrono::microseconds>(end_of_fn - start_of_fn).count();
	if (result_total / 1000.0f > 50) {
		sprintf(buf, "TOTAL GAME::RENDER TIME: %.2fms\n", result_total / 1000.0f);
		OutputDebugString(buf);
	}
}

// update player's movement based on how much time has passed since we last did it
void App::update_player_movement(const float dt) {
	/* VELOCITY FALLOFF */

	//   TODO: Handle walking on blocks, in water, etc. Maybe do it based on friction.
	//   TODO: Tweak values.
	char_velocity *= (float)pow(0.5, dt);
	vec4 norm = normalize(char_velocity);
	for (int i = 0; i < 4; i++) {
		if (char_velocity[i] > 0.0f) {
			char_velocity[i] = (float)fmaxf(0.0f, char_velocity[i] - (10.0f * norm[i] * dt));
		}
		else if (char_velocity[i] < 0.0f) {
			char_velocity[i] = (float)fmin(0.0f, char_velocity[i] - (10.0f * norm[i] * dt));
		}
	}

	/* ACCELERATION */

	// character's horizontal rotation
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

	/* VELOCITY INCREASE */

	char_velocity += acceleration * dt * 50.0f;
	if (length(char_velocity) > 10.0f) {
		char_velocity = 10.0f * normalize(char_velocity);
	}
	char_velocity[3] = 0.0f; // Just in case

	/* POSITION CHANGE */

	// Calculate our change-in-position
	vec4 position_change = char_velocity * dt;

	// Adjust it to avoid collisions
	vec4 fixed_position_change = position_change;
	if (!noclip) {
		fixed_position_change = prevent_collisions(position_change);
	}

	/* SNAP TO WALLS */

	ivec4 ipos = vec2ivec(char_position);

	// if removed east, snap to east wall
	if (position_change[0] > fixed_position_change[0]) {
		char_velocity[0] = 0;
		char_position[0] = fmin(char_position[0], ipos[0] + 1.0f - PLAYER_RADIUS); // RESET EAST
	}
	// west
	if (position_change[0] < fixed_position_change[0]) {
		char_velocity[0] = 0;
		char_position[0] = fmaxf(char_position[0], ipos[0] + PLAYER_RADIUS); // RESET WEST
	}
	// north
	if (position_change[2] < fixed_position_change[2]) {
		char_velocity[2] = 0;
		char_position[2] = fmaxf(char_position[2], ipos[2] + PLAYER_RADIUS); // RESET NORTH
	}
	// south
	if (position_change[2] > fixed_position_change[2]) {
		char_velocity[2] = 0;
		char_position[2] = fmin(char_position[2], ipos[2] + 1.0f - PLAYER_RADIUS); // RESET SOUTH
	}
	// up
	if (position_change[1] > fixed_position_change[1]) {
		char_velocity[1] = 0;
		char_position[1] = fmin(char_position[1], ipos[1] + 2.0f - PLAYER_HEIGHT); // RESET UP
	}
	// down
	if (position_change[1] < fixed_position_change[1]) {
		char_velocity[1] = 0;
		char_position[1] = fmaxf(char_position[1], (float)ipos[1]); // RESET DOWN
	}

	// Update position
	char_position += fixed_position_change;
}

// given a player's change-in-position, modify the change to optimally prevent collisions
vec4 App::prevent_collisions(const vec4 position_change) {
	// TODO: prioritize removing velocity that won't change our position when snapping.

	// Get all blocks we might be intersecting with
	auto blocks = get_intersecting_blocks(char_position + position_change);

	// if all blocks are non-solid, we done
	if (all_of(begin(blocks), end(blocks), [this](const auto &block_coords) { auto block = world->get_type(block_coords); return block.is_nonsolid(); })) {
		return position_change;
	}

	// indices of position-change array
	vector<int> indices = argsort(3, &position_change[0]);

	// TODO: Instead of removing 1 or 2 separately, group them together, and remove the ones with smallest length.
	// E.g. if velocity is (2, 2, 10), and have to either remove (2,2) or (10), remove (2,2) because sqrt(2^2+2^2) = sqrt(8) < 10.

	assert(indices[0] + indices[1] + indices[2] == 3);

	// try removing just one velocity
	for (int i = 0; i < 3; i++) {
		vec4 position_change_fixed = position_change;
		position_change_fixed[indices[i]] = 0.0f;
		blocks = get_intersecting_blocks(char_position + position_change_fixed);

		// if all blocks are non-solid, we done
		if (all_of(begin(blocks), end(blocks), [this](const auto &block_coords) { auto block = world->get_type(block_coords); return block.is_nonsolid(); })) {
			return position_change_fixed;
		}
	}

	// indices for pairs of velocities
	ivec2 pair_indices[3] = {
		{0, 1},
		{0, 2},
		{1, 2},
	};

	// sort again, this time based on 2d-vector length
	sort(begin(pair_indices), end(pair_indices), [position_change](const auto pair1, const auto pair2) {
		return length(vec2(position_change[pair1[0]], position_change[pair1[1]])) < length(vec2(position_change[pair2[0]], position_change[pair2[1]]));
	});

	// try removing two velocities
	for (int i = 0; i < 3; i++) {
		vec4 position_change_fixed = position_change;
		position_change_fixed[pair_indices[i][0]] = 0.0f;
		position_change_fixed[pair_indices[i][1]] = 0.0f;
		blocks = get_intersecting_blocks(char_position + position_change_fixed);

		// if all blocks are air, we done
		if (all_of(begin(blocks), end(blocks), [this](const auto &block_coords) { auto block = world->get_type(block_coords); return block.is_nonsolid(); })) {
			return position_change_fixed;
		}
	}

	// after all this we still can't fix it? Frick, just don't move player then.
	OutputDebugString("Holy fuck it's literally unfixable.\n");
	return { 0 };
}

void App::onKey(GLFWwindow* window, int key, int scancode, int action, int mods)
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
			noclip = !noclip;
		}

		// p = cycle poylgon mode
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

		// c = toggle face culling
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
					windowInfo.xpos_before_fullscreen, windowInfo.ypos_before_fullscreen,
					windowInfo.width_before_fullscreen, windowInfo.height_before_fullscreen,
					0);
			}

			// if not fullscreen
			else {
				GLFWmonitor* monitor = glfwGetPrimaryMonitor();
				if (monitor) {
					// save current window position/location
					glfwGetWindowPos(window, &windowInfo.xpos_before_fullscreen, &windowInfo.ypos_before_fullscreen);
					glfwGetWindowSize(window, &windowInfo.width_before_fullscreen, &windowInfo.height_before_fullscreen);

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
	}

	// handle optionally-repeatable key presses
	if (action == GLFW_PRESS || action == GLFW_REPEAT) {
		// + = increase render distance
		if (key == GLFW_KEY_KP_ADD || key == GLFW_KEY_EQUAL) {
			min_render_distance++;
		}

		// - = decrease render distance
		if (key == GLFW_KEY_KP_SUBTRACT || key == GLFW_KEY_MINUS) {
			if (min_render_distance > 0) {
				min_render_distance--;
			}
		}
	}

	// handle key releases
	if (action == GLFW_RELEASE) {
		held_keys[key] = false;
	}
}

void App::onMouseMove(GLFWwindow* window, double x, double y)
{
	// bonus of using deltas for yaw/pitch:
	// - can cap easily -- if we cap without deltas, and we move 3000x past the cap, we'll have to move 3000x back before mouse moves!
	// - easy to do mouse sensitivity
	double delta_x = x - last_mouse_x;
	double delta_y = y - last_mouse_y;

	// update pitch/yaw
	char_yaw += (float)(windowInfo.mouseX_Sensitivity * delta_x);
	char_pitch += (float)(windowInfo.mouseY_Sensitivity * delta_y);

	// cap pitch
	char_pitch = clamp(char_pitch, -90.0f, 90.0f);

	// update old values
	last_mouse_x = x;
	last_mouse_y = y;
}

void App::onResize(GLFWwindow* window, int width, int height) {
	// if minimized, don't wanna break stuff
	if (width == 0 || height == 0) {
		return;
	}

	windowInfo.width = width;
	windowInfo.height = height;

	opengl_on_resize(glInfo, width, height);
}

void App::onDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message) {
	char buf[4096];
	char *bufp = buf;

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

void App::onMouseButton(int button, int action) {
	// left click
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
		// if staring at valid block
		if (staring_at[1] >= 0) {
			world->destroy_block(staring_at);
		}
	}

	// right click
	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
		// if staring at valid block
		if (staring_at[1] >= 0) {
			// position we wanna place block at
			ivec3 desired_position = staring_at + staring_at_face;

			// check if we're in the way
			vector<ivec4> intersecting_blocks = get_intersecting_blocks(char_position);
			auto result = find_if(begin(intersecting_blocks), end(intersecting_blocks), [desired_position](const auto &ipos) {
				return desired_position == ivec3(ipos[0], ipos[1], ipos[2]);
			});

			// if we're not in the way, place it
			if (result == end(intersecting_blocks)) {
				world->add_block(desired_position, held_block);
			}
		}
	}
}

void App::onMouseWheel(double scroll_direction) {
	// scrolled up   => scroll_direction > 0
	// scrolled down => scroll_direction < 0
	assert(scroll_direction != 0 && "onMouseWheel called with invalid scroll direction");

	// increment/decrement block type
	int scroll_offset = scroll_direction > 0 ? 1 : -1;
	held_block = BlockType((int)held_block + scroll_offset);
}

namespace {
	/* GLFW/GL callback functions */

	void glfw_onError(int error, const char* description) {
		MessageBox(NULL, description, "GLFW error", MB_OK);
	}

	void glfw_onKey(GLFWwindow* window, int key, int scancode, int action, int mods) {
		App::app->onKey(window, key, scancode, action, mods);
	}

	void glfw_onMouseMove(GLFWwindow* window, double x, double y) {
		App::app->onMouseMove(window, x, y);
	}

	void glfw_onResize(GLFWwindow* window, int width, int height) {
		App::app->onResize(window, width, height);
	}

	void glfw_onMouseButton(GLFWwindow* window, int button, int action, int mods)
	{
		App::app->onMouseButton(button, action);
	}

	static void glfw_onMouseWheel(GLFWwindow* window, double xoffset, double yoffset)
	{
		App::app->onMouseWheel(yoffset);
	}

	void APIENTRY gl_onDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, GLvoid* userParam) {
		App::app->onDebugMessage(source, type, id, severity, length, message);
	}
}
