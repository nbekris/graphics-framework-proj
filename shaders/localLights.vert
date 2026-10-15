#version 330 core
layout (location = 0) in vec3 vertex;

uniform mat4 ModelTr;
uniform mat4 WorldView;
uniform mat4 WorldProj;

void main() {
	gl_Position = WorldProj * WorldView * ModelTr * vec4(vertex, 1.0);
}