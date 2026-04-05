#version 330 core
/////////////////////////////////////////////////////////////////////////
// Fragment shader for Reflection
// Self-contained forward lighting matching deferredLight.frag's model
////////////////////////////////////////////////////////////////////////

out vec4 FragColor;

// Varyings from LightingVertex() in lighting.vert
in vec3 normalVec, lightVec, eyeVec;
in vec2 texCoord;
in vec3 tanVec;

uniform int objectId;
uniform sampler2D tex;
uniform sampler2D normalTex;
uniform sampler2D skyboxMap;
uniform float hasTexture;
uniform float hasNormal;
uniform vec3 diffuse;
uniform vec3 specular;
uniform float shininess;

uniform vec3 Light;
uniform vec3 Ambient;
uniform vec3 shCoeffs[9];

const int skyId    = 1;
const int seaId    = 2;
const int groundId = 3;
const int roomId   = 4;
const int boxId    = 5;
const int lPicId   = 7;
const int rPicId   = 8;
const int floorId  = 11;

const float PI = 3.14159265359;

vec3 SchlickFresnel(float cosAngle, vec3 Ks)
{
	return Ks + (1.0 - Ks) * pow(1 - cosAngle, 5.0);
}

float DistributionGGX(float HN, float roughness)
{
	float a2 = pow(roughness, 2.0);
	float NH2 = pow(HN, 2.0);
	float denominator = PI * pow(NH2 * (a2 - 1) + 1, 2.0);
	return a2 / denominator;
}

float G1GGXSchlick(float NV, float roughness)
{
	float r = roughness + 1.0;
	float k = (r * r) / 8.0;
	return NV / (NV * (1.0 - k) + k);
}

float SmithMethod(float VN, float LN, float roughness)
{
	return G1GGXSchlick(LN, roughness) * G1GGXSchlick(VN, roughness);
}

vec3 SetNormalMap(vec2 uv, vec3 T, vec3 B, vec3 N)
{
	vec3 delta = texture(normalTex, uv).xyz;
	delta = delta * 2.0 - vec3(1, 1, 1);
	return delta.x + delta.y * B + delta.z * N;
}

vec3 EvaluateSH(vec3 n)
{
	return shCoeffs[0] * 0.282095
	     + shCoeffs[1] * 0.488603 * n.y
	     + shCoeffs[2] * 0.488603 * n.z
	     + shCoeffs[3] * 0.488603 * n.x
	     + shCoeffs[4] * 1.092548 * n.x * n.y
	     + shCoeffs[5] * 1.092548 * n.y * n.z
	     + shCoeffs[6] * 0.315392 * (3.0 * n.z * n.z - 1.0)
	     + shCoeffs[7] * 1.092548 * n.x * n.z
	     + shCoeffs[8] * 0.546274 * (n.x * n.x - n.y * n.y);
}

void main()
{
	vec3 N = normalize(normalVec);
	vec3 L = normalize(lightVec);
	vec3 V = normalize(eyeVec);
	vec3 H = normalize(L + V);
	vec3 T = normalize(tanVec);
	vec3 B = normalize(cross(T, N));

	vec2 uv = texCoord;

	// Per-object UV and normal manipulation (matches gBuffer.frag)
	if (objectId == roomId)
	{
		float angle = 90.0 * (PI / 180.0);
		mat2 rotation = mat2(cos(angle), -sin(angle),
		                     sin(angle), cos(angle));
		uv = rotation * (texCoord * 15.0);
		if (hasNormal > 0.5)
			N = SetNormalMap(uv, T, B, N);
	}
	if (objectId == floorId)
	{
		uv = texCoord * 2.0;
		if (hasNormal > 0.5)
			N = SetNormalMap(uv, T, B, N);
	}
	if (objectId == groundId)
		uv = texCoord * 100.0;
	if (objectId == boxId && hasNormal > 0.5)
		N = SetNormalMap(uv, T, B, N);
	if (objectId == seaId)
	{
		if (hasNormal > 0.5)
			N = SetNormalMap(uv * 30.0, T, B, N);
		float VN = max(dot(V, N), 0.0);
		vec3 R = V - 2.0 * VN * N;
		uv = vec2(-atan(R.y, R.x) / (2 * PI), acos(clamp(R.z, -1.0, 1.0)) / PI);
	}

	// Diffuse color
	vec3 Kd = diffuse;
	if (objectId == rPicId)
	{
		vec2 innerUv = (uv - 0.1) / 0.8;
		bool outside = innerUv.x < 0.0 || innerUv.x > 1.0 || innerUv.y < 0.0 || innerUv.y > 1.0;
		Kd = outside ? vec3(0.5) : texture(tex, innerUv).rgb;
	}
	else if (objectId == lPicId)
	{
		ivec2 cuv = ivec2(floor(10.0 * texCoord));
		Kd = ((cuv.x + cuv.y) % 2 == 0) ? vec3(0.0) : vec3(1.0);
	}
	else if (hasTexture > 0.5)
		Kd = texture(tex, uv).rgb;

	// Sky: output color directly
	if (objectId == skyId)
	{
		vec2 skyUV = vec2(-atan(V.y, V.x) / (2 * PI), acos(clamp(V.z, -1.0, 1.0)) / PI);
		if (hasTexture > 0.5)
			Kd = texture(tex, skyUV).rgb;
		FragColor = vec4(min(Kd, vec3(25.0)), 1.0);
		return;
	}

	// Lighting (matches deferredLight.frag)
	float LN = max(dot(L, N), 0.0);
	float HN = max(dot(H, N), 0.0);
	float VN = max(dot(V, N), 0.0);
	float HV = max(dot(H, V), 0.0);

	float roughness = sqrt(2.0 / (shininess + 2.0));

	vec3 Fd = Kd / PI;

	// Diffuse IBL via SH
	vec3 irrCalc = max(EvaluateSH(N), vec3(0.0));
	vec3 diffuseIBL = Fd * irrCalc;

	// Specular IBL: simple environment map lookup (no Monte-Carlo in reflection pass)
	vec3 R = 2.0 * VN * N - V;
	vec2 specularUV = vec2(-atan(-R.y, -R.x) / (2.0 * PI), acos(clamp(-R.z, -1.0, 1.0)) / PI);
	vec3 specularIBL = texture(skyboxMap, specularUV).xyz * specular;

	// Direct light BRDF
	float D = DistributionGGX(HN, roughness);
	float G_brdf = SmithMethod(VN, LN, roughness);
	vec3 F = SchlickFresnel(HV, specular);
	vec3 Fs = (F * G_brdf * D) / max(4.0 * LN * VN, 0.01);
	Fs = min(Fs, vec3(10.0));

	vec3 totalBRDF = Fd + Fs;
	vec3 directLight = totalBRDF * Light * LN;
	vec3 ambient = diffuseIBL + specularIBL;

	vec3 hdrColor = ambient + directLight;

	// Clamp to prevent firefly artifacts from stretched paraboloid-edge geometry
	hdrColor = min(hdrColor, vec3(25.0));

	FragColor = vec4(hdrColor, 1.0);
}
