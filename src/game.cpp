#include "game.h"

#include "chunk.h"
#include "chunkdata.h"
#include "messaging.h"
#include "render.h"
#include "shapes.h"
#include "unique_priority_queue.h"
#include "unique_queue.h"
#include "util.h"
#include "world_meshing.h"
#include "world_render.h"

#include "examples/imgui_impl_opengl3.h"
#include "examples/imgui_impl_glfw.h"
#include "imgui.h"
#include "zmq.hpp"

#include <algorithm>
#include <assert.h>
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

void run_game(zmq::context_t* const ctx)
{
	glfwSetErrorCallback(glfw_onError);
	App app(ctx);
	app.run();
}

// thread for generating new chunk meshes
void MeshingThread(zmq::context_t* const ctx, msg::on_ready_fn on_ready) {
	// Connect to bus
	BusNode bus(ctx);
#ifdef _DEBUG
	bus.out.setsockopt(ZMQ_SUBSCRIBE, "", 0);
#else
	for (const auto& m : msg::meshing_thread_incoming)
	{
		// TODO: Upgrade zmq and replace this with .set()
		bus.out.setsockopt(ZMQ_SUBSCRIBE, m.c_str(), m.size());
	}
#endif // _DEBUG

	// Prove you're connected
	on_ready();

	// Player's current chunk coords, so that we can always generate the closest meshes first. (TODO in future.)
	vmath::ivec2 player_chunk_coords = { 0, 0 };

	// Keep queue of incoming requests
	unique_queue<vmath::ivec3, MeshGenRequest*, vecN_hash> request_queue;

	// TODO: Wait for "START" then unsubscribe and start

	// run thread until stopped
	bool stop = false;
	while (!stop)
	{
		// read all messages
		std::vector<zmq::message_t> msg;
		auto ret = zmq::recv_multipart(bus.out, std::back_inserter(msg), zmq::recv_flags::dontwait);
		while (ret)
		{
			if (msg[0].to_string_view() == msg::EXIT)
			{
				OutputDebugString("MeshGen: Received EXIT signal\n");
				stop = true;
				break;
			}
			else if (msg[0].to_string_view() == msg::MESH_GEN_REQUEST)
			{
				MeshGenRequest* req = *(msg[1].data<MeshGenRequest*>());
				assert(req);
#ifdef _DEBUG
				std::stringstream out;
				out << "MeshGen: Received req for " << vec2str(req->coords) << "\n";
				OutputDebugString(out.str().c_str());
#endif // _DEBUG
				request_queue.push_back({ req->coords, req });
			}
			else if (msg[0].to_string_view() == msg::EVENT_PLAYER_MOVED_CHUNKS)
			{
				player_chunk_coords = *(msg[1].data<vmath::ivec2>());
			}
#ifdef _DEBUG
			else if (msg[0].to_string_view() == msg::TEST)
			{
				std::stringstream s;
				s << "MeshGen: " << msg::multi_to_str(msg) << "\n";
				OutputDebugString(s.str().c_str());
			}
			else
			{
				std::stringstream s;
				s << "MeshGen: Unknown msg [" << msg[0].to_string_view() << "]" << "\n";
				OutputDebugString(s.str().c_str());
			}
#endif // _DEBUG

			msg.clear();
			ret = zmq::recv_multipart(bus.out, std::back_inserter(msg), zmq::recv_flags::dontwait);
		}

		if (stop) break;

		// if requests in queue
		if (request_queue.size())
		{
			// handle one
			MeshGenRequest* req_ = request_queue.front().second;
			std::shared_ptr<MeshGenRequest> req(req_);

			request_queue.pop();

			assert(req);

			// generate a mesh if possible
			MeshGenResult* mesh = gen_minichunk_mesh_from_req(req);
			if (mesh != nullptr)
			{
				// send it
				std::vector<zmq::const_buffer> result({
					zmq::buffer(msg::MESH_GEN_RESPONSE),
					zmq::buffer(&mesh, sizeof(mesh))
					});
				auto ret = zmq::send_multipart(bus.in, result, zmq::send_flags::dontwait);
				assert(ret);

#ifdef SLEEPS
				// JUST IN CASE
				std::this_thread::sleep_for(std::chrono::microseconds(1));
#endif // SLEEPS
			}
		}
		// otherwise wait to try again
		else
		{
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
	}
}


// thread for generating new world chunks
void ChunkGenThread(zmq::context_t* const ctx, msg::on_ready_fn on_ready) {
	// Connect to bus
	BusNode bus(ctx);
#ifdef _DEBUG
	bus.out.setsockopt(ZMQ_SUBSCRIBE, "", 0);
#else
	for (const auto& m : msg::chunk_gen_thread_incoming)
	{
		// TODO: Upgrade zmq and replace this with .set()
		bus.out.setsockopt(ZMQ_SUBSCRIBE, m.c_str(), m.size());
	}
#endif // _DEBUG

	// Prove you're connected
	on_ready();

	// Keep some data on player (namely their chunk coords) so that we can always generate the closest chunks
	// TODO: When game starts, have it send player chunk changed.
	vmath::ivec2 player_chunk_coords = { 0, 0 };

	// Keep queue of incoming requests
	using key_t = vmath::ivec2;
	using priority_t = float;
	using pq_T = std::pair<priority_t, ChunkGenRequest*>;
	unique_priority_queue<key_t, pq_T, vecN_hash, std::equal_to<key_t>, FirstComparator<priority_t, ChunkGenRequest*>> request_queue;

	// TODO: Wait for "START" then unsubscribe and start

	// run thread until stopped
	bool stop = false;
	while (!stop)
	{
		// read all messages
		std::vector<zmq::message_t> msg;
		auto ret = zmq::recv_multipart(bus.out, std::back_inserter(msg), zmq::recv_flags::dontwait);
		while (ret)
		{
			if (msg[0].to_string_view() == msg::EXIT)
			{
				OutputDebugString("ChunkGen: Received EXIT signal\n");
				stop = true;
				break;
			}
			else if (msg[0].to_string_view() == msg::CHUNK_GEN_REQUEST)
			{
				ChunkGenRequest* req = *(msg[1].data<ChunkGenRequest*>());
				assert(req);
#ifdef _DEBUG
				std::stringstream out;
				out << "ChunkGen: Received req for " << vec2str(req->coords) << "\n";
				OutputDebugString(out.str().c_str());
#endif // _DEBUG
				float priority = vmath::distance(req->coords, player_chunk_coords);
				decltype(request_queue)::value_type v = { req->coords, { priority, req } };
				request_queue.push(v);
			}
			else if (msg[0].to_string_view() == msg::EVENT_PLAYER_MOVED_CHUNKS)
			{
				vmath::ivec2 new_player_chunk_coords = *(msg[1].data<vmath::ivec2>());
				if (new_player_chunk_coords != player_chunk_coords)
				{
					// Adjust priority queue priorities
					// TODO: Speed this up by making a vector of values and passing that to priority_queue's constructor
					decltype(request_queue) pq2;
					for (const decltype(request_queue)::value_type& v : request_queue)
					{
						std::remove_cv_t<std::remove_reference_t<decltype(v)>> v2 = v;
						auto& key = v2.first;
						auto& priority = v2.second.first;
						priority = vmath::distance(key, player_chunk_coords);
						pq2.push(v2);
					}
					request_queue.swap(pq2);
				}
			}
#ifdef _DEBUG
			else if (msg[0].to_string_view() == msg::TEST)
			{
				std::stringstream s;
				s << "ChunkGen: " << msg::multi_to_str(msg) << "\n";
				OutputDebugString(s.str().c_str());
			}
			else
			{
				std::stringstream s;
				s << "ChunkGen: Unknown msg [" << msg[0].to_string_view() << "]" << "\n";
				OutputDebugString(s.str().c_str());
			}
#endif // _DEBUG

			msg.clear();
			ret = zmq::recv_multipart(bus.out, std::back_inserter(msg), zmq::recv_flags::dontwait);
		}

		if (stop) break;

		// if requests in queue
		if (request_queue.size())
		{
			// handle one
			ChunkGenRequest* req_2 = request_queue.top().second.second;
			assert(req_2);
			request_queue.pop();

			std::shared_ptr<ChunkGenRequest> req(req_2);

			// generate a chunk
			ChunkGenResponse* response = new ChunkGenResponse;
			response->coords = req->coords;
			response->chunk = std::make_unique<Chunk>(req->coords);
			response->chunk->generate();

			// send it
			std::vector<zmq::const_buffer> result({
				zmq::buffer(msg::CHUNK_GEN_RESPONSE),
				zmq::buffer(&response, sizeof(response))
				});
			auto ret = zmq::send_multipart(bus.in, result, zmq::send_flags::dontwait);
			assert(ret);

#ifdef SLEEPS
			// JUST IN CASE
			std::this_thread::sleep_for(std::chrono::microseconds(1));
#endif // SLEEPS
		}
		// otherwise wait to try again
		else
		{
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
	}
}

void App::run()
{
	// create glfw window
	setup_glfw(&windowInfo, &window);

	// point window to app for glfw callbacks
	glfwSetWindowUserPointer(window, this);

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
			glDebugMessageCallback((GLDEBUGPROC)gl_onDebugMessage, this);
			glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
		}
	}

	// Start up app
	startup();

	// Spawn mesh generation threads
	vector<std::future<void>> chunk_gen_futures;

	// run until user presses ESC or tries to close window
	last_render_time = static_cast<float>(glfwGetTime()); // updated in render()
	while ((glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_RELEASE) && (!glfwWindowShouldClose(window))) {
		// run rendering function
		float time = static_cast<float>(glfwGetTime());
		updateWorld(time);
		render(time);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		glfwSwapBuffers(window);

		// poll window system for events
		glfwPollEvents();
	}

	// Stop all other threads
	// TODO: Have app run on separate thread, keep sending EXIT until they all exit.
	for (auto& fut : chunk_gen_futures) {
		fut.wait_for(std::chrono::seconds(1));
		OutputDebugString("Still waiting...\n");
	}

	// Send exit message
	bus.in.send(zmq::buffer(msg::EXIT));

	shutdown();
}

void App::startup()
{
	// set vars
	memset(held_keys, false, sizeof(held_keys));
	glfwGetCursorPos(window, &last_mouse_x, &last_mouse_y); // reset mouse position
	world_data = std::make_unique<WorldDataPart>(ctx);
	world_render = std::make_unique<WorldRenderPart>(ctx);

	// prepare opengl
	setup_opengl(&windowInfo, &glInfo);

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
	ImGui_ImplGlfw_InitForOpenGL(window, true);
}

void App::shutdown()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
}

void App::render(float time)
{
	// FBO: BIND
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, glInfo.fbo_out.get_fbo());

	const auto start_of_fn = std::chrono::high_resolution_clock::now();

	// change in time
	const float dt = time - last_render_time;
	last_render_time = time;
	fps = (1 - 5 * dt) * fps + 5;

	const auto direction = player.staring_direction();

	/* TRANSFORMATION MATRICES */

	// Create Model->World matrix
	const mat4 model_world_matrix =
		translate(0.0f, -CAMERA_HEIGHT, 0.0f);

	// Create World->View matrix
	const mat4 world_view_matrix =
		rotate_pitch_yaw(player.pitch, player.yaw) *
		translate(-player.position[0], -player.position[1], -player.position[2]); // move relative to you

	// Combine them into Model->View matrix
	const mat4 model_view_matrix = world_view_matrix * model_world_matrix;

	// Update projection matrix too, in case if width/height changed
	// NOTE: Careful, if (nearplane/farplane) ratio is too small, things get fucky.
	const mat4 proj_matrix = perspective(
		static_cast<float>(windowInfo.vfov), // virtual fov
		static_cast<float>(windowInfo.width) / static_cast<float>(windowInfo.height), // aspect ratio
		(PLAYER_HEIGHT - CAMERA_HEIGHT) * 1 / sqrtf(2.0f), // see blocks no matter how close they are
		64 * CHUNK_WIDTH // only support 64 chunks for now
	);

	/* BACKGROUND / SKYBOX */

	// Clear color/depth buffers
	const GLfloat one = 1.0f;
	glClearBufferfv(GL_COLOR, 0, color_sky_blue);
	glClearBufferfv(GL_DEPTH, 0, &one);

	// check if in water
	const BlockType face_block = world_data->get_type(vec2ivec(player.position + vec4(0, CAMERA_HEIGHT, 0, 0)));
	player.in_water = face_block == BlockType::StillWater || face_block == BlockType::FlowingWater;
	const GLuint in_water_gluint = static_cast<GLuint>(player.in_water);

	// Update transformation buffer with matrices
	glNamedBufferSubData(glInfo.trans_uni_buf, 0, sizeof(model_view_matrix), model_view_matrix);
	glNamedBufferSubData(glInfo.trans_uni_buf, sizeof(model_view_matrix), sizeof(proj_matrix), proj_matrix); // proj matrix
	glNamedBufferSubData(glInfo.trans_uni_buf, sizeof(model_view_matrix) + sizeof(proj_matrix), sizeof(GLuint), &in_water_gluint); // proj matrix

	// extract projection matrix planes
	vec4 planes[6];
	extract_planes_from_projmat(proj_matrix, model_view_matrix, planes);

	// Draw ALL our chunks!
	world_render->update_meshes();
	world_render->render(&glInfo, &windowInfo, planes, player.staring_at);

	// display debug info
	if (show_debug_info) {
		render_debug_info(dt);
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
#ifdef _DEBUG
	if (result_total / 1000.0f > 50) {
		std::stringstream buf;
		buf << "TOTAL GAME::render TIME: " << result_total / 1000.0f << "ms\n";
		OutputDebugString(buf.str().c_str());
	}
#endif // _DEBUG
}

void App::render_debug_info(float dt)
{
	// Prep debug info string
	std::string debugInfo;
	debugInfo.reserve(500);

	char lineBuf[256];
	const auto direction = player.staring_direction();

	sprintf(lineBuf, "FPS: %-4.1f (%d ms) (%d distance)\n", fps, (int)(dt * 1000), min_render_distance);
	debugInfo += lineBuf;

	sprintf(lineBuf, "Position: (%6.1f, %6.1f, %6.1f)\n", player.position[0], player.position[1], player.position[2]);
	debugInfo += lineBuf;

	sprintf(lineBuf, "Facing:   (%6.1f, %6.1f, %6.1f)\n", direction[0], direction[1], direction[2]);
	debugInfo += lineBuf;

	sprintf(lineBuf, "Looking at block: (%d, %d, %d)\n", player.staring_at[0], player.staring_at[1], player.staring_at[2]);
	debugInfo += lineBuf;

	sprintf(lineBuf, "Face in water: %s\n", player.in_water ? "true" : "false");
	debugInfo += lineBuf;

	sprintf(lineBuf, "Held block: %d (%s)\n", (int)held_block, held_block.side_texture().c_str());
	debugInfo += lineBuf;

	// Start the Dear ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

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

	// Rendering
	ImGui::Render();
	int display_w, display_h;
	glfwGetFramebufferSize(window, &display_w, &display_h);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}


void App::render_main_menu()
{
	// Start the Dear ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// Create window in center of screen
	ImVec2 window_pos = { ImGui::GetIO().DisplaySize.x/2, ImGui::GetIO().DisplaySize.y/2 };
	ImVec2 window_pos_pivot = {0.5f, 0.5f};
	ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
	if (ImGui::Begin("Main menu", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing))
	{
		// Calculate button sizes/spacing
		float xSpacing = ImGui::GetStyle().ItemSpacing.x + 10.0f;
		float bottomButtonWidth = 140;
		float topButtonWidth = 2 * bottomButtonWidth + xSpacing;
		float buttonHeight = 44;

#ifdef _DEBUG
		float minBottomButtonWidth = ImGui::CalcTextSize("Options\nQuit Game").x + ImGui::GetStyle().ItemInnerSpacing.x * 2;
		float minTopButtonWidth = ImGui::CalcTextSize("Single Player").x + ImGui::GetStyle().ItemInnerSpacing.x * 2;
		float minButtonHeight = ImGui::CalcTextSize("Single Player").y + ImGui::GetStyle().ItemInnerSpacing.y * 2;
		assert(bottomButtonWidth >= minBottomButtonWidth);
		assert(topButtonWidth >= minTopButtonWidth);
		assert(buttonHeight >= minButtonHeight);
#endif

		// Draw buttons
		ImGui::Button("Single Player", { topButtonWidth, buttonHeight });
		ImGui::Dummy(ImVec2(0.0f, 16.0f));
		ImGui::Button("Options", { bottomButtonWidth, buttonHeight });
		ImGui::SameLine(0, xSpacing);
		ImGui::Button("Quit Game", { bottomButtonWidth, buttonHeight });
	}
	ImGui::End();

	// Rendering
	ImGui::Render();
	int display_w, display_h;
	glfwGetFramebufferSize(window, &display_w, &display_h);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void App::updateWorld(float time) {
	const auto start_of_fn = std::chrono::high_resolution_clock::now();

	// change in time
	const float dt = time - last_render_time;
	world_data->update_tick((int)floorf(time * 20));

	/* CHANGES IN WORLD */

	world_data->handle_messages();

	// update player movement
	update_player_movement(dt);

	// update last chunk coords
	const auto chunk_coords = get_chunk_coords((int)floorf(player.position[0]), (int)floorf(player.position[2]));
	if (chunk_coords != get_last_chunk_coords()) {
		set_last_chunk_coords(chunk_coords);
	}

	// generate nearby chunks if required
	if (should_check_for_nearby_chunks) {
		world_data->gen_nearby_chunks(player.position, min_render_distance);
		should_check_for_nearby_chunks = false;
	}

	// update block that player is staring at
	const auto direction = player.staring_direction();
	raycast(player.position + vec4(0, CAMERA_HEIGHT, 0, 0), direction, 40, &player.staring_at, &player.staring_at_face, [this](const ivec3& coords, const ivec3& face) {
		const auto block = this->world_data->get_type(coords);
		return block.is_solid();
		});

	// make sure rendering didn't take too long
	const auto end_of_fn = std::chrono::high_resolution_clock::now();
	const long result_total = std::chrono::duration_cast<std::chrono::microseconds>(end_of_fn - start_of_fn).count();
#ifdef _DEBUG
	if (result_total / 1000.0f > 50) {
		std::stringstream buf;
		buf << "TOTAL GAME::updateWorld TIME: " << result_total / 1000.0f << "ms\n";
		OutputDebugString(buf.str().c_str());
	}
#endif // _DEBUG
}

// update player's movement based on how much time has passed since we last did it
void App::update_player_movement(const float dt) {
	/* VELOCITY FALLOFF */

	//   TODO: Handle walking on blocks, in water, etc. Maybe do it based on friction.
	//   TODO: Tweak values.
	player.velocity *= static_cast<float>(pow(0.5, dt));
	vec4 norm = normalize(player.velocity);
	for (int i = 0; i < 4; i++) {
		if (player.velocity[i] > 0.0f) {
			player.velocity[i] = static_cast<float>(fmaxf(0.0f, player.velocity[i] - (10.0f * norm[i] * dt)));
		}
		else if (player.velocity[i] < 0.0f) {
			player.velocity[i] = static_cast<float>(fmin(0.0f, player.velocity[i] - (10.0f * norm[i] * dt)));
		}
	}

	/* ACCELERATION */

	// character's horizontal rotation
	mat4 dir_rotation = rotate_pitch_yaw(0.0f, player.yaw);

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

	player.velocity += acceleration * dt * 50.0f;
	if (length(player.velocity) > 10.0f) {
		player.velocity = 10.0f * normalize(player.velocity);
	}
	player.velocity[3] = 0.0f; // Just in case

	/* POSITION CHANGE */

	// Calculate our change-in-position
	vec4 position_change = player.velocity * dt;

	// Adjust it to avoid collisions
	vec4 fixed_position_change = position_change;
	if (!noclip) {
		fixed_position_change = prevent_collisions(position_change);
	}

	/* SNAP TO WALLS */

	ivec4 ipos = vec2ivec(player.position);

	// if removed east, snap to east wall
	if (position_change[0] > fixed_position_change[0]) {
		player.velocity[0] = 0;
		player.position[0] = fmin(player.position[0], ipos[0] + 1.0f - PLAYER_RADIUS); // RESET EAST
	}
	// west
	if (position_change[0] < fixed_position_change[0]) {
		player.velocity[0] = 0;
		player.position[0] = fmaxf(player.position[0], ipos[0] + PLAYER_RADIUS); // RESET WEST
	}
	// north
	if (position_change[2] < fixed_position_change[2]) {
		player.velocity[2] = 0;
		player.position[2] = fmaxf(player.position[2], ipos[2] + PLAYER_RADIUS); // RESET NORTH
	}
	// south
	if (position_change[2] > fixed_position_change[2]) {
		player.velocity[2] = 0;
		player.position[2] = fmin(player.position[2], ipos[2] + 1.0f - PLAYER_RADIUS); // RESET SOUTH
	}
	// up
	if (position_change[1] > fixed_position_change[1]) {
		player.velocity[1] = 0;
		player.position[1] = fmin(player.position[1], ipos[1] + 2.0f - PLAYER_HEIGHT); // RESET UP
	}
	// down
	if (position_change[1] < fixed_position_change[1]) {
		player.velocity[1] = 0;
		player.position[1] = fmaxf(player.position[1], static_cast<float>(ipos[1])); // RESET DOWN
	}

	// Update position
	player.position += fixed_position_change;
}

// given a player's change-in-position, modify the change to optimally prevent collisions
vec4 App::prevent_collisions(const vec4 position_change) {
	// TODO: prioritize removing velocity that won't change our position when snapping.

	// Get all blocks we might be intersecting with
	auto blocks = get_intersecting_blocks(player.position + position_change);

	// if all blocks are non-solid, we done
	if (all_of(begin(blocks), end(blocks), [this](const auto& block_coords) { auto block = world_data->get_type(block_coords); return block.is_nonsolid(); })) {
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
		blocks = get_intersecting_blocks(player.position + position_change_fixed);

		// if all blocks are non-solid, we done
		if (all_of(begin(blocks), end(blocks), [this](const auto& block_coords) { auto block = world_data->get_type(block_coords); return block.is_nonsolid(); })) {
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
		blocks = get_intersecting_blocks(player.position + position_change_fixed);

		// if all blocks are air, we done
		if (all_of(begin(blocks), end(blocks), [this](const auto& block_coords) { auto block = world_data->get_type(block_coords); return block.is_nonsolid(); })) {
			return position_change_fixed;
		}
	}

	// after all this we still can't fix it? Frick, just don't move player then.
	OutputDebugString("Holy fuck it's literally unfixable.\n");
	return { 0 };
}

const vmath::ivec2& App::get_last_chunk_coords() const {
	return last_chunk_coords;
}

void App::set_last_chunk_coords(const vmath::ivec2& last_chunk_coords_) {
	if (last_chunk_coords_ != last_chunk_coords) {
		last_chunk_coords = last_chunk_coords_;
		should_check_for_nearby_chunks = true;

		// Notify listeners that last chunk coords have changed
		std::vector<zmq::const_buffer> result({
			zmq::buffer(msg::EVENT_PLAYER_MOVED_CHUNKS),
			zmq::buffer(&last_chunk_coords, sizeof(last_chunk_coords))
			});
		auto ret = zmq::send_multipart(bus.in, result, zmq::send_flags::dontwait);
		assert(ret);
	}
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

		// M = toggle mouse cursor
		if (key == GLFW_KEY_M)
		{
			if (capture_mouse)
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			else
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			capture_mouse = !capture_mouse;
		}
	}

	// handle optionally-repeatable key presses
	if (action == GLFW_PRESS || action == GLFW_REPEAT) {
		// + = increase render distance
		if (key == GLFW_KEY_KP_ADD || key == GLFW_KEY_EQUAL) {
			min_render_distance++;
			should_check_for_nearby_chunks = true;
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
	if (capture_mouse)
	{
		// bonus of using deltas for yaw/pitch:
		// - can cap easily -- if we cap without deltas, and we move 3000x past the cap, we'll have to move 3000x back before mouse moves!
		// - easy to do mouse sensitivity
		double delta_x = x - last_mouse_x;
		double delta_y = y - last_mouse_y;

		// update pitch/yaw
		player.yaw += static_cast<float>(windowInfo.mouseX_Sensitivity * delta_x);
		player.pitch += static_cast<float>(windowInfo.mouseY_Sensitivity * delta_y);
		
		// wrap yaw
		player.yaw = posmod(player.yaw, 360.0f);

		// cap pitch
		player.pitch = clamp(player.pitch, -90.0f, 90.0f);

		// update old values
		last_mouse_x = x;
		last_mouse_y = y;
	}
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

void App::onDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message) {
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

void App::onMouseButton(int button, int action) {
	if (capture_mouse)
	{
		// left click
		if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
			// if staring at valid block
			if (player.staring_at[1] >= 0) {
				world_data->destroy_block(player.staring_at);
			}
		}

		// right click
		if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
			// if staring at valid block
			if (player.staring_at[1] >= 0) {
				// position we wanna place block at
				ivec3 desired_position = player.staring_at + player.staring_at_face;

				// check if we're in the way
				vector<ivec4> intersecting_blocks = get_intersecting_blocks(player.position);
				auto result = find_if(begin(intersecting_blocks), end(intersecting_blocks), [desired_position](const auto& ipos) {
					return desired_position == ivec3(ipos[0], ipos[1], ipos[2]);
					});

				// if we're not in the way, place it
				if (result == end(intersecting_blocks)) {
					world_data->add_block(desired_position, held_block);
				}
			}
		}
	}
}

void App::onMouseWheel(double scroll_direction) {
	if (capture_mouse)
	{
		// scrolled up   => scroll_direction > 0
		// scrolled down => scroll_direction < 0
		assert(scroll_direction != 0 && "onMouseWheel called with invalid scroll direction");

		// increment/decrement block type
		int scroll_offset = scroll_direction > 0 ? 1 : -1;
		held_block = BlockType((int)held_block + scroll_offset);
	}
}

namespace {
	/* GLFW/GL callback functions */

	void glfw_onError(int error, const char* description) {
		MessageBox(NULL, description, "GLFW error", MB_OK);
	}

	void glfw_onKey(GLFWwindow* window, int key, int scancode, int action, int mods) {
		App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
		app->onKey(window, key, scancode, action, mods);
	}

	void glfw_onMouseMove(GLFWwindow* window, double x, double y) {
		App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
		app->onMouseMove(window, x, y);
	}

	void glfw_onResize(GLFWwindow* window, int width, int height) {
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

	void APIENTRY gl_onDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, GLvoid* userParam) {
		App* app = static_cast<App*>(userParam);
		app->onDebugMessage(source, type, id, severity, length, message);
	}
}
