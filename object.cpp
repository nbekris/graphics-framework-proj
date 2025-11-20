////////////////////////////////////////////////////////////////////////
// A lightweight class representing an instance of an object that can
// be drawn onscreen.  An Object consists of a shape (batch of
// triangles), and various transformation, color and texture
// parameters.  It also contains a list of child objects to be drawn
// in a hierarchical fashion under the control of parent's
// transformations.
//
// Methods consist of a constructor, and a Draw procedure, and an
// append for building hierarchies of objects.

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

#include "framework.h"
#include "shapes.h"
#include "transform.h"

#include <glu.h>                // For gluErrorString
#define CHECKERROR {GLenum err = glGetError(); if (err != GL_NO_ERROR) { fprintf(stderr, "OpenGL error (at line object.cpp:%d): %s\n", __LINE__, gluErrorString(err)); exit(-1);} }


Object::Object(Shape* _shape, const int _objectId,
               const glm::vec3 _diffuseColor, const glm::vec3 _specularColor, const float _shininess, Texture* _texture, Texture* _normalTexture)
    : diffuseColor(_diffuseColor), specularColor(_specularColor), shininess(_shininess),
      shape(_shape), texture(_texture), normalTexture(_normalTexture), objectId(_objectId), drawMe(true)
     
{}


void Object::Draw(ShaderProgram* program, glm::mat4& objectTr)
{
    CHECKERROR;
    // @@ The object specific parameters (uniform variables) used by
    // the shader are set here.  Scene specific parameters are set in
    // the DrawScene procedure in scene.cpp

    // @@ Textures, being uniform sampler2d variables in the shader,
    // are also set here.  Call texture->Bind in texture.cpp to do so.
    
    // Inform the shader of the surface values Kd, Ks, and alpha.
    int loc = glGetUniformLocation(program->programId, "diffuse");
    glUniform3fv(loc, 1, &diffuseColor[0]);

    loc = glGetUniformLocation(program->programId, "specular");
    glUniform3fv(loc, 1, &specularColor[0]);

    loc = glGetUniformLocation(program->programId, "shininess");
    glUniform1f(loc, shininess);

    // Inform the shader of which object is being drawn so it can make
    // object specific decisions.
    loc = glGetUniformLocation(program->programId, "objectId");
    glUniform1i(loc, objectId);

    // Inform the shader of this object's model transformation.  The
    // inverse of the model transformation, needed for transforming
    // normals, is calculated and passed to the shader here.
    loc = glGetUniformLocation(program->programId, "ModelTr");
    glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(objectTr));
    
    glm::mat4 inv = glm::inverse(objectTr);
    loc = glGetUniformLocation(program->programId, "NormalTr");
    glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(inv));

    // If this object has an associated texture, this is the place to
    // load the texture into a texture-unit of your choice and inform
    // the shader program of the texture-unit number.  See
    // Texture::Bind for the 4 lines of code to do exactly that.
    

    // Draw this object
    CHECKERROR;
    if (shape && drawMe) 
    {
        if (texture)
        {
            texture->BindTexture(0, program->programId, "tex");

            loc = glGetUniformLocation(program->programId, "hasTexture");
            glUniform1f(loc, 1);
        }

        if (normalTexture)
        {
            normalTexture->BindTexture(1, program->programId, "normalTex");

            loc = glGetUniformLocation(program->programId, "hasNormal");
            glUniform1f(loc, 1);

            float currentTime = glfwGetTime();
            loc = glGetUniformLocation(program->programId, "time");
            glUniform1f(loc, currentTime);
        }

        shape->DrawVAO();
        if (normalTexture) normalTexture->UnbindTexture(1);
        if (texture) texture->UnbindTexture(0);
    }

    CHECKERROR;


    CHECKERROR;
    // Recursively draw each sub-objects, each with its own transformation.
    if (drawMe)
        for (int i=0;  i<instances.size();  i++) {
            CHECKERROR;
            glm::mat4 itr = objectTr*instances[i].second*animTr;
            CHECKERROR;
            instances[i].first->Draw(program, itr);
            CHECKERROR; }
    
    CHECKERROR;
}
