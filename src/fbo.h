#ifndef __FBO_H__
#define __FBO_H__

#include "util.h"

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
		reload();

		// Tell FBO to draw into its one color buffer
		glNamedFramebufferDrawBuffer(fbo, GL_COLOR_ATTACHMENT0);
	}

	inline void set_dimensions(const GLsizei width, const GLsizei height) {
		this->width = width;
		this->height = height;
	}

	inline GLuint get_fbo() const {
		return fbo;
	}

	inline GLuint get_color_buf() const {
		return color_buf;
	}

	inline GLuint get_depth_buf() const {
		return depth_buf;
	}

	// recreate actual opengl fbo to object's properties
	void reload() {
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