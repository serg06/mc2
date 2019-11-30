#include "game.h"

#include "chunks.h"
#include "shapes.h"
#include "util.h"

#include "GL/gl3w.h"
#include "GLFW/glfw3.h"

#include <algorithm> // howbig?
#include <assert.h>
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
		time = glfwGetTime();
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
	const GLfloat(&cube)[108] = shapes::cube_full;

	// set vars
	memset(held_keys, false, sizeof(held_keys));
	glfwGetCursorPos(window, &last_mouse_x, &last_mouse_y); // reset mouse position

	// list of shaders to create program with
	// TODO: Embed these into binary somehow - maybe generate header file with cmake.
	std::vector <std::tuple<std::string, GLenum>> shader_fnames = {
		{ "../src/simple.vs.glsl", GL_VERTEX_SHADER },
		//{"../src/simple.tcs.glsl", GL_TESS_CONTROL_SHADER },
		//{"../src/simple.tes.glsl", GL_TESS_EVALUATION_SHADER },
		{"../src/simple.gs.glsl", GL_GEOMETRY_SHADER },
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
	glNamedBufferStorage(chunk_types_buf, CHUNK_SIZE * sizeof(uint8_t), NULL, GL_DYNAMIC_STORAGE_BIT); // allocate enough for all vertices, and allow editing

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


	/* HANDLE UNIFORM NOW */

	// create buffers
	glCreateBuffers(1, &trans_buf);

	// bind them
	glBindBufferBase(GL_UNIFORM_BUFFER, trans_buf_uni_bidx, trans_buf); // bind transformation buffer to uniform buffer binding point

	// allocate
	glNamedBufferStorage(trans_buf, sizeof(mat4) * 2 + sizeof(vec2), NULL, GL_DYNAMIC_STORAGE_BIT); // allocate 2 matrices of space for transforms, and allow editing


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

	//// generate a chunk
	//for (int x = 0; x < 5; x++) {
	//	for (int z = 0; z < 5; z++) {
	//		Chunk* chunk = gen_chunk(x, z);
	//		add_chunk(x, z, chunk);
	//	}
	//}


	// load into memory pog
	//glNamedBufferSubData(chunk_types_buf, 0, CHUNK_SIZE * sizeof(uint8_t), chunks[0]); // proj matrix
	glNamedBufferSubData(chunk_types_buf, 0, CHUNK_SIZE * sizeof(uint8_t), get_chunk(0, 0)->data); // proj matrix
}

void App::render(double time) {
	// change in time
	const double dt = time - last_render_time;
	last_render_time = time;

	// update player movement
	update_player_movement(dt);

	// generate nearby chunks
	gen_nearby_chunks();

	// Create Model->World matrix
	float f = (float)time * (float)M_PI * 0.1f;
	mat4 model_world_matrix =
		translate(0.0f, -PLAYER_HEIGHT * 0.9f, 0.0f);

	// Create World->View matrix
	mat4 world_view_matrix =
		rotate_pitch_yaw(char_pitch, char_yaw) *
		translate(-char_position[0], -char_position[1], -char_position[2]); // move relative to you

	// Combine them into Model->View matrix
	mat4 model_view_matrix = world_view_matrix * model_world_matrix;

	// Update projection matrix too, in case if width/height changed
	// NOTE: Careful, if (nearplane/farplane) ratio is too small, things get fucky.
	mat4 proj_matrix = perspective(
		(float)info.vfov, // virtual fov
		(float)info.width / (float)info.height, // aspect ratio
		PLAYER_RADIUS,  // blocks are always at least PLAYER_RADIUS away from camera
		16 * CHUNK_WIDTH // only support 32 chunks for now
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
	//OutputDebugString(buf);
	vec4 direction = rotate_pitch_yaw(char_pitch, char_yaw) * NORTH_0;
	sprintf(buf, "Position: (%.1f, %.1f, %.1f) | Facing: (%.1f, %.1f, %.1f)\n", char_position[0], char_position[1], char_position[2], direction[0], direction[1], direction[2]);
	//OutputDebugString(buf);

	//// Draw our chunks!
	//glBindVertexArray(vao_cube);
	//glDrawArraysInstanced(GL_TRIANGLES, 0, 36, CHUNK_SIZE);

	// Draw ALL our chunks!
	glBindVertexArray(vao_cube);
	for (auto &[coords_p, chunk] : chunk_map) {
		ivec2 coords = { coords_p.first , coords_p.second };

		glNamedBufferSubData(chunk_types_buf, 0, CHUNK_SIZE * sizeof(uint8_t), chunk->data); // proj matrix
		glNamedBufferSubData(trans_buf, sizeof(model_view_matrix) + sizeof(proj_matrix), sizeof(ivec2), coords); // Add base chunk coordinates to transformation data (temporary solution)
		glDrawArraysInstanced(GL_TRIANGLES, 0, 36, CHUNK_SIZE);

		sprintf(buf, "Drawing at (%d, %d)\n", coords[0], coords[1]);
		//OutputDebugString(buf);
	}
}

void App::update_player_movement(const double dt) {
	char buf[256];
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

	sprintf(buf, "Accel: (%.3f, %.3f, %.3f) | Velocity: (%.3f, %.3f, %.3f)\n", acceleration[0], acceleration[1], acceleration[2], char_velocity[0], char_velocity[1], char_velocity[2]);
	//OutputDebugString(buf);


	// Calculate our change-in-position
	vec4 position_change = char_velocity * dt;

	// Fix it to avoid collisions, if noclip is not on
	vec4 fixed_position_change = position_change;
	if (!noclip) {
		fixed_position_change = velocity_prevent_collisions(dt, position_change);
	}

	// Snap to walls to cancel velocity and to stay at a constant value while moving along wall
	ivec4 ipos = vec2ivec(char_position);
	float remainder;
	float integer;
	// if removed east, snap to east wall
	if (position_change[0] > fixed_position_change[0]) {
		char_velocity[0] = 0;
		char_position[0] = fmin(char_position[0], ipos[0] + 1.0f - PLAYER_RADIUS); // RESET EAST
	}
	// west
	if (position_change[0] < fixed_position_change[0]) {
		char_velocity[0] = 0;
		char_position[0] = fmax(char_position[0], ipos[0] + PLAYER_RADIUS); // RESET WEST
	}
	// north
	if (position_change[2] < fixed_position_change[2]) {
		char_velocity[2] = 0;
		char_position[2] = fmax(char_position[2], ipos[2] + PLAYER_RADIUS); // RESET NORTH
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
		char_position[1] = fmax(char_position[1], ipos[1]); // RESET DOWN
	}

	// Update position
	char_position += fixed_position_change;

	vec4 below = char_position + DOWN_0;
	auto type = get_type(below[0], below[1], below[2]);
	auto name = block_name(type);
	sprintf(buf, "Block below: %s\n", name.c_str());
	//OutputDebugString(buf);

	sprintf(buf, "Velocity: (%.2f, %.2f, %.2f)\n", char_velocity[0], char_velocity[1], char_velocity[2]);
	//OutputDebugString(buf);
}

vec4 App::velocity_prevent_collisions(const double dt, const vec4 position_change) {
	// Given a change-in-position, check if it's possible; if not, fix it optimally.
	char buf[4096];
	char *bufp = buf;

	vec4 pos = char_position;
	ivec4 ipos = vec2ivec(pos);

	vec4 new_pos = char_position + position_change;
	ivec4 inew_pos = vec2ivec(new_pos);

	// TODO: if iminXYZ and imaxXYZ haven't changed, nothing to worry about

	// 2. Check blocks

	// TODO: Remove duplicates? Or someth.

	// Get all blocks we might be intersecting with
	auto blocks = get_intersecting_blocks(position_change);

	// if all blocks are air, we done
	if (all_of(begin(blocks), end(blocks), [this](const auto &block) { return get_type(block[0], block[1], block[2]) == Block::Air; })) {
		return position_change;
	}

	// values of velocities, sorted smallest to largest, along with their indices
	pair<float, int> vals_and_indices[3] = {
		{ position_change[0], 0 },
		{ position_change[1], 1 },
		{ position_change[2], 2 }
	};
	sort(begin(vals_and_indices), end(vals_and_indices), [](const auto &vi1, const auto &vi2) { return vi1.first < vi2.first; }); // sort velocities smallest to largest

	// just in case
	for (int i = 0; i < 3; i++) {
		assert(count_if(begin(vals_and_indices), end(vals_and_indices), [i](const auto &vi) { return vi.second == i; }) == 1 && "Invalid indices array.");
	}

	// TODO: Instead of removing 1 or 2 separately, group them together, and remove the ones with smallest length.
	// E.g. if velocity is (2, 2, 10), and have to either remove (2,2) or (10), remove (2,2) because sqrt(2^2+2^2) = sqrt(8) < 10.

	// try removing just one velocity
	for (int i = 0; i < 3; i++) {
		// TODO: Adjust player's position along with velocity cancellation?
		vec4 position_change_fixed = position_change;
		position_change_fixed[vals_and_indices[i].second] = 0.0f;
		blocks = get_intersecting_blocks(position_change_fixed);

		if (all_of(begin(blocks), end(blocks), [this](const auto &block) { return get_type(block[0], block[1], block[2]) == Block::Air; })) {
			// success!
			return position_change_fixed;
		}
	}

	// try removing two velocities
	int v1, v2;
	for (int i = 0; i < 3; i++) {
		// hacked-together permutations of (0, 1, 2), (0, 1, 2)
		// TODO: what if we have velocities (0, 1000, 1), and we're removing 0 and 1000? :(
		if (i == 0) {
			v1 = vals_and_indices[0].second;
			v2 = vals_and_indices[1].second;
		}
		if (i == 1) {
			v1 = vals_and_indices[0].second;
			v2 = vals_and_indices[2].second;
		}
		if (i == 2) {
			v1 = vals_and_indices[2].second;
			v2 = vals_and_indices[2].second;
		}

		// TODO: Adjust player's position along with velocity cancellation?
		vec4 position_change_fixed = position_change;
		position_change_fixed[v1] = 0.0f;
		position_change_fixed[v2] = 0.0f;
		blocks = get_intersecting_blocks(position_change_fixed);

		if (all_of(begin(blocks), end(blocks), [this](const auto &block) { return get_type(block[0], block[1], block[2]) == Block::Air; })) {
			// success!
			return position_change_fixed;
		}
	}

	// If, after all this, we still can't fix it? Frick.
	OutputDebugString("Holy fuck it's literally unfixable.\n");
	return { 0 };
}

vector<ivec4> App::get_intersecting_blocks(vec4 velocity, vec4 direction) {
	// make sure exactly one entry is 1, rest are 0 (TODO: Support any direction vector)
	int num_nonzero = count_if(direction.begin(), direction.end(), [](int n) { return n != 0; });
	assert((num_nonzero - direction[3]) <= 1 && "Not a direction vector.");

	// get x/y/z min/max
	ivec3 xyzMin = { (int)floor(char_position[0] - PLAYER_RADIUS + velocity[0]), (int)floor(char_position[1] + velocity[1]), (int)floor(char_position[2] - PLAYER_RADIUS + velocity[2]) };
	ivec3 xyzMax = { (int)floor(char_position[0] + PLAYER_RADIUS + velocity[0]), (int)floor(char_position[1] + PLAYER_HEIGHT + velocity[1]), (int)floor(char_position[2] + PLAYER_RADIUS + velocity[2]) };

	if (char_position[0] < 0 && char_position[2] < 0 && velocity[0] == 0) {
		OutputDebugString("");
	}

	// apply direction vector if needed
	for (int i = 0; i < xyzMin.size(); i++) {
		if (direction[i] < 0) {
			OutputDebugString("Updating...\n");
			xyzMax[i] = xyzMin[i];
			break;
		}
		if (direction[i] > 0) {
			OutputDebugString("Updating...\n");
			xyzMin[i] = xyzMax[i];
			break;
		}
	}

	// get all blocks that our player intersects with
	vector<ivec4> blocks;
	for (int x = xyzMin[0]; x <= xyzMax[0]; x++) {
		for (int y = xyzMin[1]; y <= xyzMax[1]; y++) {
			for (int z = xyzMin[2]; z <= xyzMax[2]; z++)
			{
				blocks.push_back({ x, y, z, 0 });
			}
		}
	}

	char buf[256];
	char *bufp = buf;
	bufp += sprintf(bufp, "Potential blocks: ");
	for (auto block : blocks) {
		auto type = get_type(block);
		if (block[2] == -4) {
			OutputDebugString("");
		}
		bufp += sprintf(bufp, "(%d, %d, %d), ", block[0], block[1], block[2]);
	}
	//sprintf(buf, "Num intersecting blocks: %d\n", count_if(begin(blocks), end(blocks), [this](const auto& block) { return get_type(block[0], block[1], block[2]) != Block::Air; }));
	bufp += sprintf(bufp, "\n");
	//OutputDebugString(buf);

	if (char_position[0] < 0 && char_position[2] < 0) {
		OutputDebugString("");
	}

	return blocks;
}

// check if a direction (n/e/s/w) is clear for the player to fit through
bool App::is_dir_clear(vec4 direction) {
	// make sure there's exactly one non-zero value, and it's 1
	int num_nonzero = count_if(direction.begin(), direction.end(), [](int n) { return n != 0; });
	assert(num_nonzero == 1 && "Not a direction vector.");
	assert(length(direction) == 1 && "Not a direction vector.");

	// get x/y/z min/max
	int xMin = char_position[0] - PLAYER_RADIUS;
	int xMax = char_position[0] + PLAYER_RADIUS;

	int yMin = char_position[1];
	int yMax = char_position[1] + PLAYER_HEIGHT;

	int zMin = char_position[2] - PLAYER_RADIUS;
	int zMax = char_position[2] + PLAYER_RADIUS;

	// if going east
	if (direction[0] > 0) {
		xMin = xMax;
	}
	// if going west
	else if (direction[0] < 0) {
		xMax = xMin;
	}
	// if going north
	else if (direction[2] < 0) {
		yMax = yMin;
	}
	// if going south
	else if (direction[2] > 0) {
		yMin = yMax;
	}
	// if going up
	else if (direction[1] > 0) {
		zMin = zMax;
	}
	// if going down
	else if (direction[1] < 0) {
		zMax = zMin;
	}
	// uh oh
	else {
		throw "Uh oh...";
	}

	// integerize
	int ixMin = (int)floor(xMin);
	int ixMax = (int)floor(xMax);
	int iyMin = (int)floor(yMin);
	int iyMax = (int)floor(yMax);
	int izMin = (int)floor(zMin);
	int izMax = (int)floor(zMax);


	// get all blocks that our player intersects with, but only in the direction they're moving in!
	for (int x = ixMin; x < ixMax; x++) {
		for (int y = iyMin; y < iyMax; y++) {
			for (int z = izMin; z < izMax; z++) {
				if (get_type(x, y, z) != Block::Air) {
					return false;
				}
			}
		}
	}

	return true;
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

		// N = toggle noclip
		if (key == GLFW_KEY_N) {
			noclip = !noclip;
		}
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

