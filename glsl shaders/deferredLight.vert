#version 330 core
layout (location = 0) in vec3 vertex;
layout (location = 2) in vec2 vertexTexture;

uniform mat4 ModelTr, ShadowMatrix;

out vec2 TexCoords;
out vec4 shadowCoord;

void main()
{
	shadowCoord = ShadowMatrix * ModelTr * vec4(vertex, 1.0);

    TexCoords = vertexTexture;
    gl_Position = vec4(vertex.x, vertex.y, 0.0, 1.0); 
}