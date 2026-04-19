#version 330 core
layout (location = 0) in vec4 vertex;
layout (location = 1) in vec3 vertexNormal;
layout (location = 2) in vec2 vertexTexture;
layout (location = 3) in vec3 vertexTangent;

out vec3 worldPos;
out vec2 TexCoords;
out vec3 Normal;
out vec3 tanVec;

out vec3 lightVec, eyeVec;

uniform vec3 lightPos;
uniform vec3 eye;
uniform mat4 WorldView;
uniform mat4 WorldProj;
uniform mat4 ModelTr; // Passed by Object::Draw
uniform mat4 NormalTr; // Inverse Transpose of ModelTr (usually passed by Object::Draw)

void main()
{
    // Calculate World Position
    worldPos = (ModelTr*vertex).xyz;
    
    TexCoords = vertexTexture;
    
    Normal = vertexNormal * mat3(NormalTr);
    //Normal = mat3(NormalTr) * vertexNormal;

    // Lighting eye calc
    lightVec = lightPos - worldPos;
	eyeVec = eye - worldPos;

    tanVec = mat3(ModelTr) * vertexTangent;

    gl_Position = WorldProj * WorldView * ModelTr * vertex;
}