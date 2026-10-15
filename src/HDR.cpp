#include "HDR.h"
#include "math.h"
#include <fstream>
#include <stdlib.h>

#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>

using namespace gl;

#define GLM_FORCE_CTOR_INIT
#define GLM_FORCE_RADIANS
#define GLM_SWIZZLE
#include <glm/glm.hpp>

#include "texture.h"

//#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb_image.h"

#include <glu.h>  

HDR::HDR()
{
}

HDR::HDR(const std::string& filePath, bool keepPixels)
{
    stbi_set_flip_vertically_on_load(true);
    image = stbi_loadf(filePath.c_str(), &width, &height, &depth, 4);
    printf("%d %d %d %s\n", depth, width, height, filePath.c_str());
    if (!image) {
        printf("\nRead error on file %s:\n  %s\n\n", filePath.c_str(), stbi_failure_reason());
        exit(-1);
    }

    glGenTextures(1, &textureId);   // Get an integer id for this texture from OpenGL
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, image);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 10);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (int)GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (int)GL_LINEAR_MIPMAP_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (!keepPixels) {
        stbi_image_free(image);
        image = nullptr;
    }
}

void HDR::FreePixels()
{
    if (image) {
        stbi_image_free(image);
        image = nullptr;
    }
}

void HDR::BindTexture(const int unit, const int programId, const std::string& name)
{
    glActiveTexture((gl::GLenum)((int)GL_TEXTURE0 + unit));
    glBindTexture(GL_TEXTURE_2D, textureId);
    int loc = glGetUniformLocation(programId, name.c_str());
    glUniform1i(loc, unit);
}

// Unbind a texture from a texture unit whne no longer needed.
void HDR::UnbindTexture(const int unit)
{
    glActiveTexture((gl::GLenum)((int)GL_TEXTURE0 + unit));
    glBindTexture(GL_TEXTURE_2D, 0);
}
