#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec4 shadowCoord;

uniform sampler2D gFragData[4];
uniform sampler2D gLightVec;
uniform sampler2D gEyeVec;
uniform sampler2D shadowMap;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform int viewMode;
uniform float width, height;
uniform vec3 Ambient;

uniform vec3 sceneLightPos;
uniform vec3 sceneEye;

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

bool PixelInShadow()
{
	float lightDepth;
	float pixelDepth;
	vec2 shadowIndex = shadowCoord.xy / shadowCoord.w;
	bool isShadowed = false;
	float bias = 0.005;

	if (shadowCoord.w > 0 && ((shadowIndex.x > 0 && shadowIndex.x < 1) && (shadowIndex.y > 0 && shadowIndex.y < 1)))
	{
		lightDepth = texture(shadowMap, shadowIndex).w;
		pixelDepth = shadowCoord.w;

		isShadowed = pixelDepth - bias > lightDepth;
	}

	return isShadowed;
}

void main()
{       
	vec2 uv = gl_FragCoord.xy / vec2(width, height);
//	FragColor.xyz = vec3(texture(shadowMap, uv).w / 100.0);
//	return;

	//vec2 uv = TexCoords;

//	FragColor = vec4(1.0, 0.0, 0.0, 0.5);
//	return;

	// 1. Retrieve data
	vec3 FragPos = texture(gFragData[0], uv).rgb;
	vec3 Normal = texture(gFragData[1], uv).rgb;
	vec3 Diffuse = texture(gFragData[2], uv).rgb; // diffuse ie Kd
	vec3 Specular = texture(gFragData[3], uv).rgb; // specular ie Ks
	float Shininess = texture(gFragData[3], uv).a; // alpha

	vec3 lightVec = texture(gLightVec, uv).rgb;
	vec3 eyeVec = texture(gEyeVec, uv).rgb;

	vec3 N = normalize(Normal);
	//vec3 L = normalize(sceneLightPos - FragPos);
	//vec3 V = normalize(sceneEye - FragPos);
	vec3 L = normalize(lightVec);
	vec3 V = normalize(eyeVec);
	vec3 H = normalize(L + V);

	float LN = max(dot(L, N), 0.0);
	float HN = max(dot(H, N), 0.0);
	float VN = max(dot(V, N), 0.0);
	float LH = max(dot(L, H), 0.0);
	float HV = max(dot(H,V), 0.0);

	float roughness = sqrt(2 / (Shininess + 2)); //conversion from phong to GGX
	
	vec3 kD = Diffuse;
	vec3 Fd = kD / PI;

	float D = DistributionGGX(HN, roughness);
	float G = SmithMethod(VN, LN, roughness);
	vec3 F = SchlickFresnel(HV, Specular);
	vec3 Fs = (F * G * D) / max(4.0 * LN * VN, 0.001);

	vec3 totalBRDF = Fd + Fs;
	vec3 directLight = totalBRDF * lightColor * LN;
	vec3 ambient = Ambient * kD;

	if (PixelInShadow()) {
		FragColor = vec4(ambient, 1.0);
	} else {
		FragColor = vec4(directLight + ambient, 1.0);	
	}
	//FragColor.xyz = directLight + ambient;
}