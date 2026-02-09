#version 330 core
layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 vertexNormal;
layout (location = 2) in vec2 vertexTexture;

out vec3 FragPos;
out vec2 TexCoords;
out vec3 Normal;

uniform mat4 WorldView;
uniform mat4 WorldProj;
uniform mat4 ModelTr; // Passed by Object::Draw
uniform mat4 NormalTr; // Inverse Transpose of ModelTr (usually passed by Object::Draw)

void main()
{
    // Calculate World Position
    vec4 worldPos = ModelTr * vec4(vertex, 1.0);
    FragPos = worldPos.xyz; 
    
    TexCoords = vertexTexture;
    
    // Calculate Normal in World Space
    // Note: If you don't calculate NormalTr in C++, use transpose(inverse(mat3(ModelTr))) here
    Normal = vertexNormal * mat3(NormalTr); //mat3(NormalTr) * vertexNormal;

    gl_Position = WorldProj * WorldView * worldPos;
}