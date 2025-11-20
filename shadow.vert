/////////////////////////////////////////////////////////////////////////
// Vertex shader for shadows
//
// Copyright 2013 DigiPen Institute of Technology
////////////////////////////////////////////////////////////////////////
#version 330

uniform mat4 ViewMatrix, ProjectionMatrix, ModelTr, NormalTr;

in vec4 vertex;

out vec4 vPosition;


void main()
{
	gl_Position = ProjectionMatrix*ViewMatrix*ModelTr*vertex;
	vPosition = gl_Position;
}