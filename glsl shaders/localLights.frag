#version 330 core
out vec4 FragColor;	

uniform sampler2D gFragData[4];
uniform sampler2D gLightVec;
uniform sampler2D gEyeVec;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform float lightRadius;

uniform mat4 ModelTr;

uniform vec3 eye;

uniform float width, height;

const float PI = 3.14159265359;

vec3 SchlickFresnel(float cosAngle, vec3 Ks)
{
	return Ks + (1.0 - Ks) * pow(1.0 - cosAngle, 5.0);
}

float DistributionGGX(float HN, float roughness)
{
	float a2 = pow(roughness, 2.0);
	float NH2 = pow(HN, 2.0);
	
	float denominator = PI * pow(NH2 * (a2 - 1.0) + 1.0, 2.0);
	
	return a2 / denominator;
}

float G1GGXSchlick(float NV, float roughness)
{
//	float k = pow(roughness, 2.0) / 2.0; IBL calculation
//	return NV / (NV * (1 - k) + k);
    float r = roughness + 1.0;
    float k = (r * r) / 8.0; 
    
    return NV / (NV * (1.0 - k) + k);
}

float SmithMethod(float VN, float LN, float roughness) 
{
	return G1GGXSchlick(LN, roughness) * G1GGXSchlick(VN, roughness);
}

void main() {
//	FragColor = vec4(0.2, 0.0, 0.0, 1.0);
//	return;

	vec2 uv = gl_FragCoord.xy / vec2(width, height);

	// 1. Retrieve data
	vec3 FragPos = texture(gFragData[0], uv).rgb;
	vec3 Normal = texture(gFragData[1], uv).rgb;
	vec3 Diffuse = texture(gFragData[2], uv).rgb; // diffuse ie Kd
	vec3 Specular = texture(gFragData[3], uv).rgb; // specular ie Ks
	float Shininess = texture(gFragData[3], uv).a; // alpha

	//vec3 lightVec = texture(gLightVec, uv).rgb;
	//vec3 eyeVec = texture(gEyeVec, uv).rgb;

	float distance = length(lightPos - FragPos);

//	if (distance < 0.5) {
//		FragColor = vec4(lightColor, 1.0);
//		return;
//	}

//	if (length(Normal) < 0.001) {
//		discard;
//	}

	if (distance >= lightRadius) {
		FragColor = vec4(0.0, 0.0, 0.0, 0.0);
		return;
	}

	vec3 N = normalize(Normal);
	vec3 L = normalize(lightPos - FragPos);
	vec3 V = normalize(eye - FragPos);
	//vec3 L = normalize(lightVec);
	//vec3 V = normalize(eyeVec);
	vec3 H = normalize(L + V);

	float LN = max(dot(L, N), 0.0);
	float HN = max(dot(H, N), 0.0);
	float VN = max(dot(V, N), 0.0);
	float LH = max(dot(L, H), 0.0);
	float HV = max(dot(H,V), 0.0);

	float roughness = sqrt(2.0 / (Shininess + 2.0)); //conversion from phong to GGX
	
	vec3 kD = Diffuse;
	vec3 Fd = kD / PI;

	float D = DistributionGGX(HN, roughness);
	float G = SmithMethod(VN, LN, roughness);
	vec3 F = SchlickFresnel(HV, Specular);
	vec3 Fs = (F * G * D) / max(4.0 * LN * VN, 0.01);
	Fs = min(Fs, vec3(10.0));

	vec3 totalBRDF = Fd + Fs;
	vec3 directLight = totalBRDF * lightColor * LN;

	// Attenuation
	float d2 = distance * distance; // distance squared
	float r2 = lightRadius * lightRadius; // radius squared
	float attenuation = (1.0 / (d2 + 0.0001)) - (1.0 / r2);
    
	vec3 result = clamp(directLight * attenuation, vec3(0.0), vec3(1.0));
	FragColor = vec4(result, 1.0);
}