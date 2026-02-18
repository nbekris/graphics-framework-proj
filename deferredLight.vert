#version 330 core
layout (location = 0) in vec3 vertex;
layout (location = 2) in vec2 vertexTexture;

out vec2 TexCoords;

void main()
{
    TexCoords = vertexTexture;
    gl_Position = vec4(vertex.x, vertex.y, 0.0, 1.0); 
}