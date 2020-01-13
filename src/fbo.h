#ifndef __FBO_H__
#define __FBO_H__

#include "util.h"

#include <utility>

// can only run after glfw (and maybe opengl) have been initialized
// TODO: maybe cache?
static inline GLenum get_default_framebuffer_depth_attachment_type() {
	GLint result;
	glGetNamedFramebufferAttachmentParameteriv(0, GL_DEPTH, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &result);

	switch (result) {
	case 16:
		return GL_DEPTH_COMPONENT16;
	case 24:
		return GL_DEPTH_COMPONENT24;
	case 32:
		return GL_DEPTH_COMPONENT32;
	default:
		char buf[256];
		sprintf(buf, "Unable to retrieve default framebuffer attachment size. (Got %d.)", result);
		WindowsException(buf);
		exit(-1);
	}

	return result;
}

class FBO {
private:
	GLsizei width;
	GLsizei height;

	GLuint color_buf = 0;
	GLuint depth_buf = 0;
	GLuint fbo = 0;

public:
	FBO(){}

	FBO(const GLsizei width, const GLsizei height) : width(width), height(height)
	{
		// Create FBO
		glCreateFramebuffers(1, &fbo);

		// Fill it in
		update_fbo();

		// Tell FBO to draw into its one color buffer
		glNamedFramebufferDrawBuffer(fbo, GL_COLOR_ATTACHMENT0);
	}

	~FBO()
	{
		// delete textures
		glDeleteTextures(1, &color_buf);
		color_buf = 0;

		glDeleteTextures(1, &depth_buf);
		depth_buf = 0;

		// delete fbo
		glDeleteFramebuffers(1, &fbo);
		fbo = 0;
	}

	// copy assignment operator
	// disallow copying FBO, as we'd have to recreate FBOs and textures and copy everything over, way inefficient
	FBO& operator=(const FBO& rhs) = delete;

	// move assignment operator
	FBO& operator=(FBO&& rhs) noexcept {
		// swap everything so that we own their textures/fbos and they clean up our textures/fbos
		std::swap(width, rhs.width);
		std::swap(height, rhs.height);

		std::swap(color_buf, rhs.color_buf);
		std::swap(depth_buf, rhs.depth_buf);
		std::swap(fbo, rhs.fbo);

		return *this;
	}

	inline auto get_width() const {
		return width;
	}

	inline auto get_height() const {
		return height;
	}

	inline void set_dimensions(const GLsizei width, const GLsizei height) {
		this->width = width;
		this->height = height;
	}

	inline GLuint get_color_buf() const {
		return color_buf;
	}

	inline void set_color_buf(GLuint color_buf) {
		this->color_buf = color_buf;
	}

	inline GLuint get_depth_buf() const {
		return depth_buf;
	}

	inline void set_depth_buf(GLuint depth_buf) {
		this->depth_buf = depth_buf;
	}

	inline GLuint get_fbo() const {
		return fbo;
	}

	// update our OpenGL FBO to match this object
	void update_fbo() {
		// delete textures
		glDeleteTextures(1, &color_buf);
		glDeleteTextures(1, &depth_buf);

		// Create color texture, allocate
		glCreateTextures(GL_TEXTURE_2D, 1, &color_buf);
		glTextureStorage2D(color_buf, 1, GL_RGBA32F, width, height);
		glTextureParameteri(color_buf, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // TODO: if stuff breaks, switch to GL_NEAREST
		glTextureParameteri(color_buf, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // TODO: if stuff breaks, switch to GL_NEAREST
		glTextureParameteri(color_buf, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(color_buf, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// Create depth texture, allocate
		glCreateTextures(GL_TEXTURE_2D, 1, &depth_buf);
		glTextureStorage2D(depth_buf, 1, get_default_framebuffer_depth_attachment_type(), width, height);
		glTextureParameteri(depth_buf, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(depth_buf, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTextureParameteri(depth_buf, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(depth_buf, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// Bind color / depth textures to FBO
		glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, color_buf, 0);
		glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, depth_buf, 0);
	}
};

#endif /* __FBO_H__ */
