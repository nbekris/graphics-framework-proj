#version 330 core

layout (location = 0) out vec4 FragData[4];
layout (location = 4) out vec3 LightVec;
layout (location = 5) out vec3 EyeVec;

in vec2 TexCoords;
in vec3 worldPos;
in vec3 Normal;
in vec3 tanVec;

in vec3 lightVec, eyeVec;

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
const float PI = 3.14159265359;

uniform vec3 diffuse;        // Object.cpp sends "diffuse" --- Kd
uniform float hasTexture;    // Object.cpp sends "hasTexture" as a float
uniform vec3 specular;       // Object.cpp sends "specular" ---- Ks
uniform float shininess;     // Alpha

uniform vec3 viewPos;
uniform vec3 lightPos;

uniform int objectId;
uniform sampler2D tex;
uniform sampler2D normalTex;
uniform float hasNormal;
uniform bool reflective;

vec3 SetNormalMap(vec2 uv, vec3 T, vec3 B, vec3 N)
{
	vec3 delta;
	delta = texture(normalTex, uv).xyz;
	delta = delta * 2.0 - vec3(1, 1, 1);
	return delta.x + delta.y * B + delta.z * N;
}

void main()
{
	vec3 N = normalize(Normal);
	vec3 V = normalize(eyeVec);
	vec3 T = normalize(tanVec);
	vec3 B = normalize(cross(T, N));

	vec2 uv = TexCoords;

	// Per-object UV and normal manipulation
	if (objectId == roomId)
	{
		float angle = 90.0 * (PI / 180.0);
		mat2 rotation =
			mat2(cos(angle), -sin(angle),
			     sin(angle), cos(angle));

		uv = rotation * (TexCoords * 15.0);
		if (hasNormal > 0.5)
			N = SetNormalMap(uv, T, B, N);
	}
	if (objectId == floorId)
	{
		uv = TexCoords * 2.0;
		if (hasNormal > 0.5)
			N = SetNormalMap(uv, T, B, N);
	}
	if (objectId == groundId)
	{
		uv = TexCoords * 100.0;
	}
	if (objectId == boxId)
	{
		if (hasNormal > 0.5)
			N = SetNormalMap(uv, T, B, N);
	}
	if (objectId == seaId)
	{
		if (hasNormal > 0.5)
		{
			N = SetNormalMap(uv * 30.0, T, B, N);
		}
		float VN = max(dot(V, N), 0.0);
		vec3 R = V - 2.0 * VN * N;
		uv = vec2(-atan(R.y, R.x) / (2 * PI), acos(R.z) / PI);
	}

	// Determine diffuse color (Kd)
	vec3 Kd = diffuse;

	if (objectId == rPicId)
	{
		vec2 innerUv = (uv - 0.1) / 0.8;
		bool outsideInnerRegion = innerUv.x < 0.0 || innerUv.x > 1.0 || innerUv.y < 0.0 || innerUv.y > 1.0;

		if (outsideInnerRegion)
		{
			Kd = vec3(0.5);
		}
		else
		{
			Kd = texture(tex, innerUv).rgb;
		}
	}
	else if (objectId == lPicId)
	{
		ivec2 cuv = ivec2(floor(10.0 * TexCoords));
		if ((cuv.x + cuv.y) % 2 == 0)
		{
			Kd = vec3(0.0);
		}
		else
		{
			Kd = vec3(1.0);
		}
	}
	else if (hasTexture > 0.5)
	{
		Kd = texture(tex, uv).rgb;
	}

	// Store G-buffer data
	// FragData[0].a = objectId (for deferred lighting pass)
	FragData[0] = vec4(worldPos, float(objectId));

	// FragData[1].a = emissiveFlag (0 = sky/emissive, 1 = normal lit geometry)
	float emissiveFlag = (objectId == skyId) ? 0.0 : 1.0;
	FragData[1] = vec4(N, emissiveFlag);

	// FragData[2].a = reflective flag (1.0 if reflective)
	FragData[2] = vec4(Kd, reflective ? 1.0 : 0.0);

	FragData[3] = vec4(specular, shininess);

	LightVec = normalize(lightPos - worldPos);
	EyeVec   = normalize(viewPos - worldPos);
}
