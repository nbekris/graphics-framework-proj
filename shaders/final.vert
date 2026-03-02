/////////////////////////////////////////////////////////////////////////
// Vertex shader for lighting
//
// Copyright 2013 DigiPen Institute of Technology
////////////////////////////////////////////////////////////////////////
#version 330
uniform mat4 WorldView, WorldInverse, WorldProj, ModelTr, NormalTr, ShadowMatrix;
uniform vec3 Eye;
in vec4 vertex;

void LightingVertex(vec3 eye);
void main()
{      
	gl_Position = WorldProj*WorldView*ModelTr*vertex;
	LightingVertex(Eye);
}
