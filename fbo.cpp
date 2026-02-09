///////////////////////////////////////////////////////////////////////
// A slight encapsulation of a Frame Buffer Object (i'e' Render
// Target) and its associated texture.  When the FBO is "Bound", the
// output of the graphics pipeline is captured into the texture.  When
// it is "Unbound", the texture is available for use as any normal
// texture.
////////////////////////////////////////////////////////////////////////

#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>
#include <iostream>
#include <vector>
using namespace gl;

#include "fbo.h"

void FBO::CreateFBO(const int w, const int h)
{
    width = w;
    height = h;

    glGenFramebuffersEXT(1, &fboID);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fboID);

    // Create a render buffer, and attach it to FBO's depth attachment
    unsigned int depthBuffer;
    glGenRenderbuffersEXT(1, &depthBuffer);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, depthBuffer);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT,
                             width, height);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
                                 GL_RENDERBUFFER_EXT, depthBuffer);

    // Create a texture and attach FBO's color 0 attachment.  The
    // GL_RGBA32F and GL_RGBA constants set this texture to be 32 bit
    // floats for each of the 4 components.  Many other choices are
    // possible.
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, (int)GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (int)GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (int)GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (int)GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (int)GL_LINEAR);

    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                              GL_TEXTURE_2D, textureID, 0);

    // Check for completeness/correctness
    int status = (int)glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    if (status != int(GL_FRAMEBUFFER_COMPLETE_EXT))
        printf("FBO Error: %d\n", status);

    // Unbind the fbo until it's ready to be used
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}

void FBO::BindFBO() { glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fboID); }
void FBO::UnbindFBO() { glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0); }

void FBO::BindTexture(const int unit, const int programId, const std::string& name)
{
    glActiveTexture((gl::GLenum)((int)GL_TEXTURE0 + unit));
    glBindTexture(GL_TEXTURE_2D, textureID);
    int loc = glGetUniformLocation(programId, name.c_str());
    glUniform1i(loc, unit);
}

void FBO::UnbindTexture(const int unit)
{  
    glActiveTexture((gl::GLenum)((int)GL_TEXTURE0 + unit));
    glBindTexture(GL_TEXTURE_2D, 0);
}

////////////////////////////////////////////////////////////////////////
// Creates a G-Buffer with 3 Render Targets
// Position (RGB16F)
// Normal (RGB16F)
// Albedo + Specular (RGBA)
////////////////////////////////////////////////////////////////////////
void FBO::CreateGBuffer(const int w, const int h)
{
    width = w;
    height = h;

    // Generate and Bind Framebuffer
    glGenFramebuffers(1, &fboID);
    glBindFramebuffer(GL_FRAMEBUFFER, fboID);

    // Position Buffer
    glGenTextures(1, &gPosition);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, (int)GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (int)GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (int)GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);

    // Normal Buffer
    glGenTextures(1, &gNormal);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, (int)GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (int)GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (int)GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);

    // Albedo + Specular Buffer
    glGenTextures(1, &gAlbedoSpec);
    glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
    glTexImage2D(GL_TEXTURE_2D, 0, (int)GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (int)GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (int)GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedoSpec, 0);

    // Tell OpenGL which color attachments we will use for rendering
    const GLenum attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, attachments);

    // Create and attach Depth Buffer (Renderbuffer)
    glGenRenderbuffers(1, &depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);

    // Check for completeness
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        printf("GBuffer FBO Error, status: 0x%x\n", status);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FBO::BindGBufferTextures(int startUnit, int programId,
    std::string posName, std::string normName, std::string albedoName)
{
    // Bind Position
    glActiveTexture((GLenum)((int)GL_TEXTURE0 + startUnit));
    glBindTexture(GL_TEXTURE_2D, gPosition);
    int loc = glGetUniformLocation(programId, posName.c_str());
    glUniform1i(loc, startUnit);

    // Bind Normal
    glActiveTexture((GLenum)((int)GL_TEXTURE0 + startUnit + 1));
    glBindTexture(GL_TEXTURE_2D, gNormal);
    loc = glGetUniformLocation(programId, normName.c_str());
    glUniform1i(loc, startUnit + 1);

    // Bind Albedo
    glActiveTexture((GLenum)((int)GL_TEXTURE0 + startUnit + 2));
    glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
    loc = glGetUniformLocation(programId, albedoName.c_str());
    glUniform1i(loc, startUnit + 2);
}

void FBO::UnbindGBufferTextures(int startUnit)
{
    // Unbind Position
    glActiveTexture((GLenum)((int)GL_TEXTURE0 + startUnit));
    glBindTexture(GL_TEXTURE_2D, 0);

    // Unbind Normal
    glActiveTexture((GLenum)((int)GL_TEXTURE0 + startUnit + 1));
    glBindTexture(GL_TEXTURE_2D, 0);

    // Unbind Albedo
    glActiveTexture((GLenum)((int)GL_TEXTURE0 + startUnit + 2));
    glBindTexture(GL_TEXTURE_2D, 0);
}
