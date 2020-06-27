#pragma once

#include "GL/glcorearb.h"


// can only run after glfw (and maybe opengl) have been initialized
GLenum get_default_framebuffer_depth_attachment_type();
void assert_fbo_not_incomplete(GLuint fbo);

class FBO {
public:
	FBO();
	FBO(const GLsizei width, const GLsizei height);
	~FBO();

	FBO& operator=(const FBO& rhs) = delete;
	FBO& operator=(FBO&& rhs) noexcept;

	GLsizei get_width() const;
	GLsizei get_height() const;
	void set_dimensions(const GLsizei width, const GLsizei height);

	GLuint get_color_buf() const;
	void set_color_buf(GLuint color_buf);

	GLuint get_depth_buf() const;
	void set_depth_buf(GLuint depth_buf);

	GLuint get_fbo() const;

	// update our OpenGL FBO to match this object
	void update_fbo();

private:
	GLsizei width;
	GLsizei height;

	GLuint color_buf = 0;
	GLuint depth_buf = 0;
	GLuint fbo = 0;
};
