// glfb.cc

#include <GL/glew.h>
//#include <GL/glext.h>

#include "glfb.h"

void gle(const char *s) {
	int err;
	while((err = glGetError()) != GL_NO_ERROR) {
		fprintf(stderr, "gl (%s): %x\n", s, err);
	}
}

GLFB::GLFB(float w, float h)
{
	width = height = 0;

	glGenFramebuffers(1, &fbo);
//	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glGenTextures(1, &texture);
	glGenRenderbuffers(1, &rbo);
	Resize(w, h);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
}

GLFB::~GLFB()
{
	glDeleteFramebuffers(1, &fbo);
	glDeleteTextures(1, &texture);
	glDeleteRenderbuffers(1, &rbo);
}

unsigned int GLFB::getFrameTexture()
{
	return texture;
}

void GLFB::Resize(float w, float h)
{
	if (w != width || h != height) {
		width = w;
		height = h;

		glBindTexture(GL_TEXTURE_2D, texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
		glBindRenderbuffer(GL_RENDERBUFFER, rbo);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
		//fprintf(stderr, "2 resize: %.1fx%.1f\n", w, h);
	}
}

void GLFB::Bind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

void GLFB::Unbind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
