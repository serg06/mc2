#pragma once

#include "messaging.h"
#include "minichunk.h" // renderer part

#include "zmq.hpp"

#include <memory>
#include <unordered_map>

class WorldRenderPart
{
public:
	WorldRenderPart(std::shared_ptr<zmq::context_t> ctx_);

	// get mini render component or nullptr
	std::shared_ptr<MiniRender> get_mini_render_component(const int x, const int y, const int z);
	std::shared_ptr<MiniRender> get_mini_render_component(const vmath::ivec3& xyz);

	// get mini render component or nullptr
	std::shared_ptr<MiniRender> get_mini_render_component_or_generate(const int x, const int y, const int z);
	std::shared_ptr<MiniRender> get_mini_render_component_or_generate(const vmath::ivec3& xyz);

	void handle_messages();
	void render(OpenGLInfo* glInfo, GlfwInfo* windowInfo, const vmath::vec4(&planes)[6], const vmath::ivec3& staring_at);

	void highlight_block(const OpenGLInfo* glInfo, const GlfwInfo* windowInfo, const int x, const int y, const int z);
	void highlight_block(const OpenGLInfo* glInfo, const GlfwInfo* windowInfo, const vmath::ivec3& xyz);

private:
	BusNode bus;
	std::unordered_map<vmath::ivec3, std::shared_ptr<MiniRender>, vecN_hash> mesh_map;
	int rendered = 0; // how many times render() was called
};
