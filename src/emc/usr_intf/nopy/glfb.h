// GLFB.h

#pragma once
#include <iostream>
//#include <glad/glad.h>
//#include <glm/glm.hpp>

#include <GL/glext.h>

class GLFB
{
public:
	GLFB(float width, float height);
	~GLFB();
	GLuint getFrameTexture();
	void Resize(float width, float height);
	void Bind() const;
	void Unbind() const;
private:
	GLuint fbo;
	unsigned int texture;
	unsigned int rbo;
	float width;
	float height;
};
