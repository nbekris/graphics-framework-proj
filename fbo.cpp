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
void FBO::UnbindFBOEXT() { glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0); }
void FBO::UnbindFBO() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

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
// Creates a G-Buffer with 4 Render Targets
// Position
// Normal
// Albedo
// Specular
////////////////////////////////////////////////////////////////////////
void FBO::CreateGBuffer(const int w, const int h)
{
    width = w;
    height = h;

    // Generate and Bind Framebuffer
    glGenFramebuffers(1, &fboID);
    glBindFramebuffer(GL_FRAMEBUFFER, fboID);
    glGenTextures(4, gFragData);

    for (unsigned int i = 0; i < 4; i++) {
        glBindTexture(GL_TEXTURE_2D, gFragData[i]);

        // Note: Using GL_RGBA16F for high precision (needed for Normals/Positions)
        // If this is just Albedo/Color, GL_RGBA8 is fine.
        glTexImage2D(GL_TEXTURE_2D, 0, (int)GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (int)GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (int)GL_NEAREST);

        // Bind to Attachment 0, 1, 2, 3 respectively
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, gFragData[i], 0);
    }

    // Light Vector buffer
    glGenTextures(1, &gLightVec);
    glBindTexture(GL_TEXTURE_2D, gLightVec);
    glTexImage2D(GL_TEXTURE_2D, 0, (int)GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (int)GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (int)GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, gLightVec, 0);

    // Eye Vector buffer
    glGenTextures(1, &gEyeVec);
    glBindTexture(GL_TEXTURE_2D, gEyeVec);
    glTexImage2D(GL_TEXTURE_2D, 0, (int)GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (int)GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (int)GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT5, GL_TEXTURE_2D, gEyeVec, 0);

    // Tell OpenGL which color attachments we will use for rendering
    const GLenum attachments[6] = {
        GL_COLOR_ATTACHMENT0, // FragData[0]
        GL_COLOR_ATTACHMENT1, // FragData[1]
        GL_COLOR_ATTACHMENT2, // FragData[2]
        GL_COLOR_ATTACHMENT3, // FragData[3]
        GL_COLOR_ATTACHMENT4, // LightVec (Location 4)
        GL_COLOR_ATTACHMENT5  // EyeVec   (Location 5)
    };
    glDrawBuffers(6, attachments);

    // Create and attach Depth Buffer (Renderbuffer)
    unsigned int depthBuffer;
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
    std::string fragDataName, std::string lightVecName, std::string eyeVecName)
{
    int fragDataValues[4]; // To store the units for the uniform array

    for (int i = 0; i < 4; i++)
    {
        glActiveTexture((GLenum)((int)GL_TEXTURE0 + startUnit + i));
        glBindTexture(GL_TEXTURE_2D, gFragData[i]); // Requires gFragData to be an array!

        fragDataValues[i] = startUnit + i;
    }

    int loc = glGetUniformLocation(programId, fragDataName.c_str());
    if (loc != -1) glUniform1iv(loc, 4, fragDataValues);

    int lightUnit = startUnit + 4;
    glActiveTexture((GLenum)((int)GL_TEXTURE0 + lightUnit));
    glBindTexture(GL_TEXTURE_2D, gLightVec);

    loc = glGetUniformLocation(programId, lightVecName.c_str());
    glUniform1i(loc, lightUnit);

    int eyeUnit = startUnit + 5;
    glActiveTexture((GLenum)((int)GL_TEXTURE0 + eyeUnit));
    glBindTexture(GL_TEXTURE_2D, gEyeVec);

    loc = glGetUniformLocation(programId, eyeVecName.c_str());
    glUniform1i(loc, eyeUnit);
}

void FBO::UnbindGBufferTextures(int startUnit)
{
    int fragDataValues[4]; // To store the units for the uniform array

    for (int i = 0; i < 4; i++)
    {
        glActiveTexture((GLenum)((int)GL_TEXTURE0 + startUnit + i));
        glBindTexture(GL_TEXTURE_2D, 0); // Requires gFragData to be an array!

        fragDataValues[i] = startUnit + i;
    }

    int lightUnit = startUnit + 4;
    glActiveTexture((GLenum)((int)GL_TEXTURE0 + lightUnit));
    glBindTexture(GL_TEXTURE_2D, 0);

    int eyeUnit = startUnit + 5;
    glActiveTexture((GLenum)((int)GL_TEXTURE0 + eyeUnit));
    glBindTexture(GL_TEXTURE_2D, 0);
}
