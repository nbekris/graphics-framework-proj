#version 330 core
layout (location = 0) in vec3 vertex;
layout (location = 2) in vec2 vertexTexture;

out vec2 TexCoords;

void main()
{
    // The quad model usually goes from -1 to 1. 
    // If your Quad shape is 0 to 1, you might need to scale it here.
    // Assuming standard normalized device coordinates (-1 to 1):
    TexCoords = vertexTexture;
    gl_Position = vec4(vertex.x, vertex.y, 0.0, 1.0); 
}