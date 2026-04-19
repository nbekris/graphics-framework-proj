/////////////////////////////////////////////////////////////////////////
// Vertex shader for lighting
//
// Copyright 2013 DigiPen Institute of Technology
////////////////////////////////////////////////////////////////////////
#version 330

uniform mat4 WorldView, WorldInverse, WorldProj, ModelTr, NormalTr, ShadowMatrix;

in vec4 vertex;
in vec3 vertexNormal;
in vec2 vertexTexture;
in vec3 vertexTangent;

out vec3 normalVec, lightVec, eyeVec;
out vec3 tanVec;
out vec2 texCoord;
out vec4 shadowCoord;
uniform vec3 lightPos;

void LightingVertex(vec3 eye)
{
	vec3 worldPos = (ModelTr*vertex).xyz;

	shadowCoord = ShadowMatrix * ModelTr * vertex;

    normalVec =  vertexNormal * mat3(NormalTr);
	
	// Compute vectors toward light and eye and output them to frag shader
	//eye = (WorldInverse*vec4(0, 0, 0, 1)).xyz;
	lightVec = lightPos - worldPos;
	eyeVec = eye - worldPos;
	
    lightVec = lightPos - worldPos;

    texCoord = vertexTexture;
	tanVec = mat3(ModelTr) * vertexTangent;
}