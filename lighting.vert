/////////////////////////////////////////////////////////////////////////
// Vertex shader for lighting
//
// Copyright 2013 DigiPen Institute of Technology
////////////////////////////////////////////////////////////////////////
#version 330

uniform mat4 WorldView, WorldInverse, WorldProj, ModelTr, NormalTr;

in vec4 vertex;
in vec3 vertexNormal;
in vec2 vertexTexture;
in vec3 vertexTangent;

out vec3 normalVec, lightVec, eyeVec;
out vec3 tanVec;
out vec2 texCoord;
uniform vec3 lightPos;

void main()
{      
	vec3 worldPos = (ModelTr*vertex).xyz;

    normalVec = mat3(NormalTr) * vertexNormal; 
	
	// Compute vectors toward light and eye and output them to frag shader
	vec3 eyePos = (WorldInverse*vec4(0, 0, 0, 1)).xyz; // maybe missing ()
	lightVec = lightPos - worldPos;
	eyeVec = eyePos - worldPos;
	
    lightVec = lightPos - worldPos;

    texCoord = vertexTexture;
	tanVec = mat3(ModelTr) * vertexTangent;
	
	gl_Position = WorldProj*WorldView*ModelTr*vertex;
}
