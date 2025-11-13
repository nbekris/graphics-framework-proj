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
out vec2 texCoord;
uniform vec3 lightPos;

uniform mat4 TextureTr;

void main()
{      
	vec3 worldPos = (ModelTr*vertex).xyz;
	vec4 uv = vec4(vertexTexture, 0.0, 1.0);

    normalVec = vertexNormal*mat3(NormalTr); 
	
	// Compute vectors toward light and eye and output them to frag shader
	vec3 eyePos = (WorldInverse*vec4(0, 0, 0, 1)).xyz;
	lightVec = lightPos - worldPos;
	eyeVec = eyePos - worldPos;
	
    lightVec = lightPos - worldPos;

    uv = TextureTr * uv;
    texCoord = uv.xy;
	
	gl_Position = WorldProj*WorldView*ModelTr*vertex;
}
