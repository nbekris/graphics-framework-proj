/////////////////////////////////////////////////////////////////////////
// Vertex shader for Reflection
//
// Copyright 2013 DigiPen Institute of Technology
////////////////////////////////////////////////////////////////////////
#version 330

uniform mat4 ModelTr;
uniform float ReflectDir;
uniform vec3 Eye;

in vec4 vertex;

void LightingVertex(vec3 eye);
void main()
{      
	vec3 worldPos = (ModelTr*vertex).xyz;
	LightingVertex(Eye);

	vec3 P = worldPos;

	vec3 R = P - Eye;

	float distR = length(R);
	vec3 normalR = normalize(R);

	float a = normalR.x;
	float b = normalR.y;
	float c = normalR.z;

	float denominator = 1.0 + (ReflectDir * c);
	gl_Position = vec4((a / denominator), b / denominator, distR * (ReflectDir * c) / 1000.0 - 1.0, 1.0);
}