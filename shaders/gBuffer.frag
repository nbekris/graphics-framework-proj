#version 330 core

layout (location = 0) out vec4 FragData[4];
layout (location = 4) out vec3 LightVec;
layout (location = 5) out vec3 EyeVec;

// Lighting Calc Variables
in vec3 worldPos;
in vec3 Normal;
in vec3 lightVec, eyeVec;
uniform vec3 diffuse;        // Object.cpp sends "diffuse" --- Kd
uniform vec3 specular;       // Object.cpp sends "specular" ---- Ks
uniform float shininess;     // Alpha
uniform vec3 viewPos;
uniform vec3 lightPos;

// Texture variables
in vec3 tanVec;
in vec2 texCoord;
uniform bool hasNormal;
uniform bool hasTexture;
uniform sampler2D tex;
uniform sampler2D normalTex;

// These definitions agree with the ObjectIds enum in scene.h
const int     nullId	= 0;
const int     skyId	= 1;
const int     seaId	= 2;
const int     groundId	= 3;
const int     roomId	= 4;
const int     boxId	= 5;
const int     frameId	= 6;
const int     lPicId	= 7;
const int     rPicId	= 8;
const int     teapotId	= 9;
const int     spheresId	= 10;
const int     floorId	= 11;
uniform int objectId;
const float PI = 3.14159265359;

vec3 SetNormalMap(vec2 uv, vec3 T, vec3 B, vec3 N)
{
	vec3 delta;
	delta = texture(normalTex, uv).xyz;
	delta = delta * 2.0 - vec3(1, 1, 1);
	return delta.x + delta.y * B + delta.z * N;
}

void main()
{    
	vec2 uv = texCoord;

	vec3 N = normalize(Normal);
    vec3 L = normalize(lightVec);
	vec3 V = normalize(eyeVec);
	vec3 H = normalize(L + V);

	vec3 T = normalize(tanVec);
	vec3 B = normalize(cross(T, N));

	////// Object Ids If block ////////////////////////////////////////////

	if (objectId==groundId) 
	{
		uv = texCoord * 10.0;
	}

	if (objectId==roomId)
	{
		float angle = 90.0 * (PI / 180.0);
		mat2 rotation = 
			mat2(cos(angle), -sin(angle), 
			     sin(angle), cos(angle));

		uv = rotation * (texCoord * 15.0);
		N = SetNormalMap(uv, T, B, N);
	}

	if (objectId==skyId) 
	{
		uv =  vec2(-atan(V.y, V.x) / (2 * PI), acos(V.z) / PI);
	}

	if (objectId==floorId)
	{
		uv = texCoord * 2.0;
		N = SetNormalMap(uv, T, B, N);		
	}

	if (objectId==boxId) 
	{
		N = SetNormalMap(uv, T, B, N);
	}

	if (objectId==seaId)
	{

		float LN = max(dot(L, N), 0.0);
		float HN = max(dot(H, N), 0.0);
		float VN = max(dot(V, N), 0.0);

		if (hasNormal)
		{
			N = SetNormalMap(uv * 30.0, T, B, N);

			LN = max(dot(L, N), 0.0);
			HN = max(dot(H, N), 0.0);
			VN = max(dot(V, N), 0.0);
		}
		vec3 R = V - 2.0 * VN * N;
		uv = vec2(-atan(R.y, R.x) / (2 * PI), acos(R.z) / PI);
	}

	// End Object Ids If block
	///////////////////////////////////////////////////////////////////////

	FragData[0] = vec4(worldPos, 1.0);
	FragData[1] = vec4(N, 1.0);

	if (hasTexture) {
		FragData[2] = vec4(texture(tex, uv).rgb, 1.0);
	} else {
		FragData[2] = vec4(diffuse, 1.0);
	}

	FragData[3] = vec4(specular, shininess);

	LightVec = normalize(lightPos - worldPos);
    EyeVec   = normalize(viewPos - worldPos);
}