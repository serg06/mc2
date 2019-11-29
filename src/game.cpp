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
	Chunk* chunk = gen_chunk();
	chunk->data[1] = Block::Grass;
	add_chunk(0, 0, chunk);

	// load into memory pog
	//glNamedBufferSubData(chunk_types_buf, 0, CHUNK_SIZE * sizeof(uint8_t), chunks[0]); // proj matrix
	glNamedBufferSubData(chunk_types_buf, 0, CHUNK_SIZE * sizeof(uint8_t), get_chunk(0, 0)->data); // proj matrix
}

void App::render(double time) {
	// change in time
	const double dt = time - last_render_time;
	last_render_time = time;

	// update player movement
	App::update_player_movement(dt);

	// generate nearby chunks

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
	mat4 proj_matrix = perspective(
		(float)info.vfov, // virtual fov
		(float)info.width / (float)info.height, // aspect ratio
		0.0001f,  // can't see behind 0.0 anyways
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
	vec4 direction = rotate_pitch_yaw(char_pitch, char_yaw) * NORTH_0;
	sprintf(buf, "Position: (%.1f, %.1f, %.1f) | Facing: (%.1f, %.1f, %.1f)\n", char_position[0], char_position[1], char_position[2], direction[0], direction[1], direction[2]);
	//OutputDebugString(buf);

	// Draw our chunks!
	glBindVertexArray(vao_cube);
	glDrawArraysInstanced(GL_TRIANGLES, 0, 36, CHUNK_SIZE);
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



	// Check for collision and remove necessary velocity
	//velocity_prevent_collisions(dt);
	velocity_prevent_collisions2(dt);

	// Update player position
	char_position += char_velocity * dt;
}

ivec4 vec2ivec(vec4 v) {
	ivec4 result;
	for (int i = 0; i < v.size(); i++) {
		result[i] = (int)v[i];
	}
	return result;
}

void App::velocity_prevent_collisions(const double dt) {
	// Update character's velocity so it doesn't cause any collisions in the next dt time
	vec4 new_pos = char_position + char_velocity * dt;
	ivec4 inew_pos = vec2ivec(new_pos);
	Block* chunk = chunks[0];


	// all possible corners of player's collision model
	vec4 corners[8] = {
		new_pos + PLAYER_NORTH_0 + PLAYER_EAST_0 + PLAYER_DOWN_0,
		new_pos + PLAYER_NORTH_0 + PLAYER_EAST_0 + PLAYER_UP_0,
		new_pos + PLAYER_NORTH_0 + PLAYER_WEST_0 + PLAYER_DOWN_0,
		new_pos + PLAYER_NORTH_0 + PLAYER_WEST_0 + PLAYER_UP_0,
		new_pos + PLAYER_SOUTH_0 + PLAYER_EAST_0 + PLAYER_DOWN_0,
		new_pos + PLAYER_SOUTH_0 + PLAYER_EAST_0 + PLAYER_UP_0,
		new_pos + PLAYER_SOUTH_0 + PLAYER_WEST_0 + PLAYER_DOWN_0,
		new_pos + PLAYER_SOUTH_0 + PLAYER_WEST_0 + PLAYER_UP_0,
	};

	// bottom corners of players collision model
	vec4 bottom_corners[4] = {
		new_pos + PLAYER_NORTH_0 + PLAYER_EAST_0 + PLAYER_DOWN_0,
		new_pos + PLAYER_NORTH_0 + PLAYER_WEST_0 + PLAYER_DOWN_0,
		new_pos + PLAYER_SOUTH_0 + PLAYER_EAST_0 + PLAYER_DOWN_0,
		new_pos + PLAYER_SOUTH_0 + PLAYER_WEST_0 + PLAYER_DOWN_0,
	};
	ivec4 ibottom_corners[4] = {
		vec2ivec(new_pos + PLAYER_NORTH_0 + PLAYER_EAST_0 + PLAYER_DOWN_0),
		vec2ivec(new_pos + PLAYER_NORTH_0 + PLAYER_WEST_0 + PLAYER_DOWN_0),
		vec2ivec(new_pos + PLAYER_SOUTH_0 + PLAYER_EAST_0 + PLAYER_DOWN_0),
		vec2ivec(new_pos + PLAYER_SOUTH_0 + PLAYER_WEST_0 + PLAYER_DOWN_0),
	};

	// ...
	vec4 top_corners[4] = {
		new_pos + PLAYER_NORTH_0 + PLAYER_EAST_0 + PLAYER_UP_0,
		new_pos + PLAYER_NORTH_0 + PLAYER_WEST_0 + PLAYER_UP_0,
		new_pos + PLAYER_SOUTH_0 + PLAYER_EAST_0 + PLAYER_UP_0,
		new_pos + PLAYER_SOUTH_0 + PLAYER_WEST_0 + PLAYER_UP_0,
	};
	ivec4 itop_corners[4] = {
		vec2ivec(new_pos + PLAYER_NORTH_0 + PLAYER_EAST_0 + PLAYER_UP_0),
		vec2ivec(new_pos + PLAYER_NORTH_0 + PLAYER_WEST_0 + PLAYER_UP_0),
		vec2ivec(new_pos + PLAYER_SOUTH_0 + PLAYER_EAST_0 + PLAYER_UP_0),
		vec2ivec(new_pos + PLAYER_SOUTH_0 + PLAYER_WEST_0 + PLAYER_UP_0),
	};

	vec4 east_corners[4] = {
		new_pos + PLAYER_NORTH_0 + PLAYER_EAST_0 + PLAYER_UP_0,
		new_pos + PLAYER_NORTH_0 + PLAYER_EAST_0 + PLAYER_DOWN_0,
		new_pos + PLAYER_SOUTH_0 + PLAYER_EAST_0 + PLAYER_UP_0,
		new_pos + PLAYER_SOUTH_0 + PLAYER_EAST_0 + PLAYER_DOWN_0,
	};
	ivec4 ieast_corners[4] = {
		vec2ivec(new_pos + PLAYER_NORTH_0 + PLAYER_EAST_0 + PLAYER_UP_0),
		vec2ivec(new_pos + PLAYER_NORTH_0 + PLAYER_EAST_0 + PLAYER_DOWN_0),
		vec2ivec(new_pos + PLAYER_SOUTH_0 + PLAYER_EAST_0 + PLAYER_UP_0),
		vec2ivec(new_pos + PLAYER_SOUTH_0 + PLAYER_EAST_0 + PLAYER_DOWN_0),
	};

	vec4 west_corners[4] = {
		new_pos + PLAYER_NORTH_0 + PLAYER_WEST_0 + PLAYER_UP_0,
		new_pos + PLAYER_NORTH_0 + PLAYER_WEST_0 + PLAYER_DOWN_0,
		new_pos + PLAYER_SOUTH_0 + PLAYER_WEST_0 + PLAYER_UP_0,
		new_pos + PLAYER_SOUTH_0 + PLAYER_WEST_0 + PLAYER_DOWN_0,
	};
	ivec4 iwest_corners[4] = {
		vec2ivec(new_pos + PLAYER_NORTH_0 + PLAYER_WEST_0 + PLAYER_UP_0),
		vec2ivec(new_pos + PLAYER_NORTH_0 + PLAYER_WEST_0 + PLAYER_DOWN_0),
		vec2ivec(new_pos + PLAYER_SOUTH_0 + PLAYER_WEST_0 + PLAYER_UP_0),
		vec2ivec(new_pos + PLAYER_SOUTH_0 + PLAYER_WEST_0 + PLAYER_DOWN_0),
	};

	vec4 north_corners[4] = {
		new_pos + PLAYER_NORTH_0 + PLAYER_EAST_0 + PLAYER_UP_0,
		new_pos + PLAYER_NORTH_0 + PLAYER_EAST_0 + PLAYER_DOWN_0,
		new_pos + PLAYER_NORTH_0 + PLAYER_WEST_0 + PLAYER_UP_0,
		new_pos + PLAYER_NORTH_0 + PLAYER_WEST_0 + PLAYER_DOWN_0,
	};
	ivec4 inorth_corners[4] = {
		vec2ivec(new_pos + PLAYER_NORTH_0 + PLAYER_EAST_0 + PLAYER_UP_0),
		vec2ivec(new_pos + PLAYER_NORTH_0 + PLAYER_EAST_0 + PLAYER_DOWN_0),
		vec2ivec(new_pos + PLAYER_NORTH_0 + PLAYER_WEST_0 + PLAYER_UP_0),
		vec2ivec(new_pos + PLAYER_NORTH_0 + PLAYER_WEST_0 + PLAYER_DOWN_0),
	};

	vec4 south_corners[4] = {
		new_pos + PLAYER_SOUTH_0 + PLAYER_EAST_0 + PLAYER_UP_0,
		new_pos + PLAYER_SOUTH_0 + PLAYER_EAST_0 + PLAYER_DOWN_0,
		new_pos + PLAYER_SOUTH_0 + PLAYER_WEST_0 + PLAYER_UP_0,
		new_pos + PLAYER_SOUTH_0 + PLAYER_WEST_0 + PLAYER_DOWN_0,
	};
	ivec4 isouth_corners[4] = {
		vec2ivec(new_pos + PLAYER_SOUTH_0 + PLAYER_EAST_0 + PLAYER_UP_0),
		vec2ivec(new_pos + PLAYER_SOUTH_0 + PLAYER_EAST_0 + PLAYER_DOWN_0),
		vec2ivec(new_pos + PLAYER_SOUTH_0 + PLAYER_WEST_0 + PLAYER_UP_0),
		vec2ivec(new_pos + PLAYER_SOUTH_0 + PLAYER_WEST_0 + PLAYER_DOWN_0),
	};

	// STEP 1: If new bottom corners are below old position, and any intersect with a block, kill down velocity
	if (bottom_corners[0][1] < char_position[1]) { // implies down velocity
		for (ivec4 corner : ibottom_corners) {
			if ((uint8_t)chunk_get(chunk, corner[0], corner[1], corner[2]) != 0) {
				char_velocity[1] = 0;
				break;
			}
		}
	}

	// STEP 2: If new top corners are below above position+UP_0, and any intersect with a block, kill up velocity
	if (top_corners[0][1] > (char_position + UP_0)[1]) { // implies up velocity
		for (ivec4 corner : itop_corners) {
			if ((uint8_t)chunk_get(chunk, corner[0], corner[1], corner[2]) != 0) {
				char_velocity[1] = 0;
				break;
			}
		}
	}

	// STEP 3: ...
	if (east_corners[0][0] > char_position[0]) { // implies up velocity
		for (ivec4 corner : ieast_corners) {
			if ((uint8_t)chunk_get(chunk, corner[0], corner[1], corner[2]) != 0) {
				char_velocity[0] = 0;
				break;
			}
		}
	}

	if (west_corners[0][0] < char_position[0]) { // implies up velocity
		for (ivec4 corner : iwest_corners) {
			if ((uint8_t)chunk_get(chunk, corner[0], corner[1], corner[2]) != 0) {
				char_velocity[0] = 0;
				break;
			}
		}
	}

	if (north_corners[0][2] < char_position[2]) { // implies up velocity
		for (ivec4 corner : inorth_corners) {
			if ((uint8_t)chunk_get(chunk, corner[0], corner[1], corner[2]) != 0) {
				char_velocity[2] = 0;
				break;
			}
		}
	}

	if (south_corners[0][2] > char_position[2]) { // implies up velocity
		for (ivec4 corner : isouth_corners) {
			if ((uint8_t)chunk_get(chunk, corner[0], corner[1], corner[2]) != 0) {
				char_velocity[2] = 0;
				break;
			}
		}
	}

	// SOLVED: HOW TO DO DIAGONAL COLLISION:
	// - WHEN YOU HIT BLOCK
	// - CHECK WHAT SIDE OF YOU THAT BLOCK IS ON
	//   - right/left => remove X velocity
	//   - north/south => remove Z velocity
	//   - etc.
	// - THEN
	//   - north-east => if closer to north side of current player position, then remove east velocity, else vice versa!


}

void App::velocity_prevent_collisions2(const double dt) {
	char buf[256];
	char *bufp = buf;

	vec4 pos = char_position;
	ivec4 ipos = vec2ivec(pos);

	vec4 new_pos = char_position + char_velocity * dt;
	ivec4 inew_pos = vec2ivec(new_pos);

	// our corner locations
	vec4 corners[12] = {
		// 8 corners around us
		new_pos + PLAYER_NORTH_0 + PLAYER_EAST_0 + PLAYER_DOWN_0,
		new_pos + PLAYER_NORTH_0 + PLAYER_EAST_0 + PLAYER_UP_0,
		new_pos + PLAYER_NORTH_0 + PLAYER_WEST_0 + PLAYER_DOWN_0,
		new_pos + PLAYER_NORTH_0 + PLAYER_WEST_0 + PLAYER_UP_0,
		new_pos + PLAYER_SOUTH_0 + PLAYER_EAST_0 + PLAYER_DOWN_0,
		new_pos + PLAYER_SOUTH_0 + PLAYER_EAST_0 + PLAYER_UP_0,
		new_pos + PLAYER_SOUTH_0 + PLAYER_WEST_0 + PLAYER_DOWN_0,
		new_pos + PLAYER_SOUTH_0 + PLAYER_WEST_0 + PLAYER_UP_0,

		// 4 corners in our middle
		new_pos + PLAYER_NORTH_0 + PLAYER_EAST_0 + (PLAYER_UP_0 / 2),
		new_pos + PLAYER_NORTH_0 + PLAYER_WEST_0 + (PLAYER_UP_0 / 2),
		new_pos + PLAYER_SOUTH_0 + PLAYER_EAST_0 + (PLAYER_UP_0 / 2),
		new_pos + PLAYER_SOUTH_0 + PLAYER_WEST_0 + (PLAYER_UP_0 / 2),
	};

	// 1. Get all blocks we intersects with

	// our corner locations, changed to coordinates
	ivec4 icorners[12];
	for (int i = 0; i < sizeof(corners) / sizeof(corners[0]); i++) {
		icorners[i] = vec2ivec(corners[i]);
	}

	bufp += sprintf(bufp, "blocks: (");
	for (auto &block : icorners) {
		Block type = get_type(block[0], block[1], block[2]);
		bufp += sprintf(bufp, "%d", (uint8_t)type);
		if (block != icorners[7]) {
			bufp += sprintf(bufp, ", ");
		}
	}
	bufp += sprintf(bufp, ")\n");
	//OutputDebugString(buf);

	// 2. Check blocks

	// for every block that we're intersecting
	bufp = buf;
	for (auto block : icorners) {
		Block type = get_type(block[0], block[1], block[2]);

		// air => no collision
		if (type == Block::Air) {
			continue;
		}

		block[3] = ipos[3]; // just in case

		// TODO: Combine NORTH/SOUTH/EAST/WEST into one loop, then multiply by 1-(itself) to set that component to zero
		// TODO: Remove inner if.

		//sprintf(buf, "block: (%d, %d, %d) | ipos + IEAST: (%d, %d, %d)\n", block[0], block[1], block[2], ipos[0], ipos[1], ipos[2]);
		//OutputDebugString(buf);


		// 2.1. If blocks in is n/e/s/w from us, easy

		// East/West
		if (block == ipos + IEAST_0 || block == ipos + IEAST_0 + IUP_0) {
			bufp += sprintf(bufp, "EAST ");
			char_velocity[0] = 0;
		}

		if (block == ipos + IWEST_0 || block == ipos + IWEST_0 + IUP_0) {
			bufp += sprintf(bufp, "WEST ");
			char_velocity[0] = 0;
		}

		// North/South
		if (block == ipos + INORTH_0 || block == ipos + INORTH_0 + IUP_0) {
			bufp += sprintf(bufp, "NORTH ");
			char_velocity[2] = 0;
		}

		if (block == ipos + ISOUTH_0 || block == ipos + ISOUTH_0 + IUP_0) {
			bufp += sprintf(bufp, "SOUTH ");
			char_velocity[2] = 0;
		}

		// Up/Down
		if (block == ipos + IUP_0 * 2) {
			bufp += sprintf(bufp, "UP ");
			char_velocity[1] = 0;
		}

		if (block == ipos + IDOWN_0) {
			bufp += sprintf(bufp, "DOWN ");
			char_velocity[1] = 0;
		}

		// 2.2. If block is ne/nw/any double-combination, doable

		/* SO FAR DONE horizontal directions; NOT BAD! */

		float dist1, dist2, dist3; // distances to 3 sides of the block you're on
		float tmp;
		ivec4 ivtmp;

		float corner_allowance = 0.05f;


		if ((char_velocity[2] < 0 || char_velocity[0]) > 0 && (block == ipos + INORTH_0 + IEAST_0 || block == ipos + INORTH_0 + IEAST_0 + IUP_0)) {
			bufp += sprintf(bufp, "NORTH-EAST ");

			dist1 = abs(modf(block[2] - char_position[2], &tmp)); // -z
			dist2 = abs(modf(block[0] - char_position[0], &tmp)); // +x

			//bufp += sprintf(bufp, "(dist1 = %.2f, dist2 = %.2f) ", dist1, dist2);

			// if right on the corner, which way should we go?
			if (abs(dist1 - dist2) < corner_allowance) {
				// if north and east are clear, kill a direction based on velocity
				if (is_dir_clear(NORTH_0) && is_dir_clear(EAST_0)) {
					// if going more east, kill north
					if (char_velocity[0] > 0 && char_velocity[0] > (-char_velocity[2])) {
						char_velocity[2] = 0;
						OutputDebugString("RESET NORTH\n");
						char_position[2] = fmax(char_position[2], ipos[2] + PLAYER_RADIUS); // RESET NORTH
					}
					// if going more north, kill east
					else if ((-char_velocity[2]) > 0) {
						char_velocity[0] = 0;
						OutputDebugString("RESET EAST\n");
						char_position[0] = fmin(char_position[0], ipos[0] + 1.0f - PLAYER_RADIUS); // RESET EAST
					}
				}
			}
			else {
				bool check = dist1 < dist2;
				if (check && char_velocity[0] > 0) {
					char_velocity[0] = 0;
				}
				if (!check && char_velocity[2] < 0) {
					char_velocity[2] = 0;
				}
			}
		}

		if ((char_velocity[2] < 0 || char_velocity[0] < 0) && (block == ipos + INORTH_0 + IWEST_0 || block == ipos + INORTH_0 + IWEST_0 + IUP_0)) {
			bufp += sprintf(bufp, "NORTH-WEST ");

			dist1 = abs(modf(block[2] - char_position[2], &tmp)); // -z
			dist2 = abs(modf(block[0] - char_position[0], &tmp)); // -x

			bufp += sprintf(bufp, "(dist1 = %.2f, dist2 = %.2f) ", dist1, dist2);

			// if right on the corner, which way should we go?
			if (abs(dist1 - dist2) < corner_allowance) {
				// if north and west are clear, kill a direction based on velocity
				if (is_dir_clear(NORTH_0) && is_dir_clear(WEST_0)) {
					// if going more west, kill north
					if ((-char_velocity[0]) > 0 && (-char_velocity[0]) > (-char_velocity[2])) {
						char_velocity[2] = 0;
						OutputDebugString("RESET NORTH\n");
						char_position[2] = fmax(char_position[2], ipos[2] + PLAYER_RADIUS); // RESET NORTH
					}
					// if going more north, kill west
					else if ((-char_velocity[2]) > 0) {
						char_velocity[0] = 0;
						OutputDebugString("RESET WEST\n");
						char_position[0] = fmax(char_position[0], ipos[0] + PLAYER_RADIUS); // RESET WEST
					}
				}
			}
			else {
				bool check = dist1 < dist2;
				if (check && char_velocity[0] < 0) {
					char_velocity[0] = 0;
				}
				if (!check && char_velocity[2] < 0) {
					char_velocity[2] = 0;
				}
			}
		}

		if ((char_velocity[2] > 0 || char_velocity[0] > 0) && (block == ipos + ISOUTH_0 + IEAST_0 || block == ipos + ISOUTH_0 + IEAST_0 + IUP_0)) {
			bufp += sprintf(bufp, "SOUTH-EAST ");

			dist1 = abs(modf(block[2] - char_position[2], &tmp)); // +z
			dist2 = abs(modf(block[0] - char_position[0], &tmp)); // +x

			// if right on the corner, which way should we go?
			if (abs(dist1 - dist2) < corner_allowance) {
				// if south and east are clear, kill a direction based on velocity
				if (is_dir_clear(SOUTH_0) && is_dir_clear(EAST_0)) {
					// if going more east, kill south
					if (char_velocity[0] > 0 && char_velocity[0] > char_velocity[2]) {
						char_velocity[2] = 0;
						OutputDebugString("RESET SOUTH\n");
						char_position[2] = fmin(char_position[2], ipos[2] + 1.0f - PLAYER_RADIUS); // RESET SOUTH
					}
					// if going more south, kill east
					else if (char_velocity[2] > 0) {
						char_velocity[0] = 0;
						OutputDebugString("RESET EAST\n");
						char_position[0] = fmin(char_position[0], ipos[0] + 1.0f - PLAYER_RADIUS); // RESET EAST
					}
				}
			}
			else {
				bool check = dist1 < dist2;
				if (check && char_velocity[0] > 0) {
					char_velocity[0] = 0;
				}
				if (!check && char_velocity[2] > 0) {
					char_velocity[2] = 0;
				}
			}
		}

		if ((char_velocity[2] > 0 || char_velocity[0] < 0) && (block == ipos + ISOUTH_0 + IWEST_0 || block == ipos + ISOUTH_0 + IWEST_0 + IUP_0)) {
			bufp += sprintf(bufp, "SOUTH-WEST ");

			dist1 = abs(modf(block[2] - char_position[2], &tmp)); // +z
			dist2 = abs(modf(block[0] - char_position[0], &tmp)); // -x

			// if right on the corner, which way should we go?
			if (abs(dist1 - dist2) < corner_allowance) {
				// if south and west are clear, kill a direction based on velocity
				if (is_dir_clear(SOUTH_0) && is_dir_clear(WEST_0)) {
					// if going more west, kill south
					if ((-char_velocity[0]) > 0 && (-char_velocity[0]) > char_velocity[2]) {
						char_velocity[2] = 0;
						OutputDebugString("RESET SOUTH\n");
						char_position[2] = fmin(char_position[2], ipos[2] + 1.0f - PLAYER_RADIUS); // RESET SOUTH
					}
					// if going more south, kill west
					else if (char_velocity[2] > 0) {
						char_velocity[0] = 0;
						OutputDebugString("RESET WEST\n");
						char_position[0] = fmax(char_position[0], ipos[0] + PLAYER_RADIUS); // RESET WEST
					}
				}
			}
			else {
				bool check = dist1 < dist2;
				if (check && char_velocity[0] < 0) {
					char_velocity[0] = 0;
				}
				if (!check && char_velocity[2] > 0) {
					char_velocity[2] = 0;
				}
			}
		}
	}
	if (bufp > buf) {
		bufp += sprintf(bufp, "\n");
		//OutputDebugString(buf);
	}

	// COLLISIONS TODO:
	// - Get collisions working for up/down
	// - Get collisions working for up/down + n/s/e/w/
	// - Finally get collisions working for up/down + n/s + e/w (or maybe not lmao, seems really hard)

	// 2.3. If block is neu/etc, difficult!
}

// check if a direction (n/e/s/w) is clear for the player to fit through
// TODO: Currently only checking that center of player can move through; instead, should check all corners on that side
bool App::is_dir_clear(vec4 direction) {
	int yMin = char_position[1];
	int yMax = char_position[1] + PLAYER_HEIGHT;

	for (int yOffset = 0; yOffset <= (yMax - yMin); yOffset++) {
		vec4 checkme = char_position + direction;
		checkme[1] += yOffset;

		if (get_type((int)checkme[0], (int)checkme[1], (int)checkme[2]) != Block::Air) {
			return false;
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

