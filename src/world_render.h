#pragma once

#include "messaging.h"
#include "minichunk.h"

#include "zmq.hpp"

#include <memory>
#include <unordered_map>

class WorldRenderPart
{
public:
	WorldRenderPart(zmq::context_t* const ctx_);

	std::unordered_map<vmath::ivec3, std::shared_ptr<MiniRender>, vecN_hash> mesh_map;

	// count how many times render() has been called
	int rendered = 0;

	// get mini render component or nullptr
	std::shared_ptr<MiniRender> get_mini_render_component(const int x, const int y, const int z);
	std::shared_ptr<MiniRender> get_mini_render_component(const vmath::ivec3& xyz);

	// get mini render component or nullptr
	std::shared_ptr<MiniRender> get_mini_render_component_or_generate(const int x, const int y, const int z);
	std::shared_ptr<MiniRender> get_mini_render_component_or_generate(const vmath::ivec3& xyz);

	void handle_messages();
	void render(OpenGLInfo* glInfo, GlfwInfo* windowInfo, const vmath::vec4(&planes)[6], const vmath::ivec3& staring_at);

	// check if a mini is visible in a frustum
	static inline bool mini_in_frustum(const MiniRender* mini, const vmath::vec4(&planes)[6]);

	static inline float intbound(const float s, const float ds);
	void highlight_block(const OpenGLInfo* glInfo, const GlfwInfo* windowInfo, const int x, const int y, const int z);
	void highlight_block(const OpenGLInfo* glInfo, const GlfwInfo* windowInfo, const vmath::ivec3& xyz);

private:
	BusNode bus;
};
