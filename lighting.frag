/////////////////////////////////////////////////////////////////////////
// Pixel shader for lighting
////////////////////////////////////////////////////////////////////////
#version 330

out vec4 FragColor;

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

in vec3 normalVec, lightVec, eyeVec;
in vec2 texCoord;
in vec3 tanVec;
in vec4 shadowCoord;

uniform int objectId;
uniform sampler2D tex;
uniform sampler2D normalTex;
uniform sampler2D shadowMap;
uniform sampler2D reflectionTop;
uniform sampler2D reflectionBottom;
uniform sampler2D irradianceMap;
uniform sampler2D skyboxMap;
uniform bool hasNormal;
uniform bool hasTexture;
uniform float currentTime;
uniform bool reflective;

// Values describing the surface
uniform vec3 diffuse; //Kd
uniform vec3 specular; // Ks
uniform float shininess; // alpha exponent

// Values describing the scene's light
uniform vec3 Light; // li
uniform vec3 Ambient; // la

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
	float k = pow(roughness, 2.0) / 2.0;
	return NV / (NV * (1 - k) + k);
}

float SmithMethod(float VN, float LN, float roughness) 
{
	return G1GGXSchlick(LN, roughness) * G1GGXSchlick(VN, roughness);
}

vec3 SetNormalMap(vec2 uv, vec3 T, vec3 B, vec3 N)
{
	vec3 delta;
	delta = texture(normalTex, uv).xyz;
	delta = delta * 2.0 - vec3(1, 1, 1);
	return delta.x + delta.y * B + delta.z * N;
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

vec3 LightingPixel()
{
//	vec2 testUv = gl_FragCoord.xy/vec2(750,750); // (or whatever screen size)
//	//FragColor.xyz = vec3(texture(reflectionTop, uv).w/100.0); // or similar]
//	return texture(irradianceMap, testUv).xyz * 20;

	vec3 delta;
	vec3 Kd;
    vec3 N = normalize(normalVec);
    vec3 L = normalize(lightVec);
	vec3 V = normalize(eyeVec);
	vec3 H = normalize(L + V);

	float roughness = sqrt(2 / (shininess + 2)); //conversion from phong to GGX

	vec2 uv = texCoord;

	vec3 T = normalize(tanVec);
	vec3 B = normalize(cross(T, N));

	float cValue;

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
		uv =  vec2(-atan(V.y, V.x) / (2 * PI), acos(clamp(V.z, -1.0, 1.0)) / PI);
	}
	if (objectId==floorId)
	{
		uv = texCoord * 2.0;
		N = SetNormalMap(uv, T, B, N);		
	}

	if (objectId==groundId) 
	{
		uv = texCoord * 100.0;
	}
	if (objectId==boxId) 
	{
		N = SetNormalMap(uv, T, B, N);
	}

	float LN = max(dot(L, N), 0.0);
	float HN = max(dot(H, N), 0.0);
	float VN = max(dot(V, N), 0.0);
	float LH = max(dot(L, H), 0.0);
	float HV = max(dot(H,V), 0.0);

	if (objectId==seaId)
	{

		if (hasNormal)
		{
			N = SetNormalMap(uv * 30.0, T, B, N);

			LN = max(dot(L, N), 0.0);
			HN = max(dot(H, N), 0.0);
			VN = max(dot(V, N), 0.0);
		}
		vec3 R = V - 2.0 * VN * N;
		uv = vec2(-atan(R.y, R.x) / (2 * PI), acos(clamp(R.z, -1.0, 1.0)) / PI);
	}

	Kd = diffuse;

	if (hasTexture) 
	{
		Kd = texture(tex, uv).xyz;
	}
	else
	{
		Kd = diffuse;
	}

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

	if (objectId==lPicId)
	{
        ivec2 uv = ivec2(floor(10.0*texCoord));
        if ((uv.x + uv.y) % 2 == 0)
		{
            Kd = vec3(0.0);
		}
		else
		{
			Kd = vec3(1.0);
		}
	}
    
	if(objectId != skyId)
	{
//		vec3 F = SchlickFresnel(LH, specular);
//		float D = DistributionGGX(HN, roughness);
//		float G = SmithMethod(VN, LN, roughness);
//		vec3 Fs = (F * G * D) / max(4.0 * LN * VN, 0.001);
//	


		// IBL Calculation ------------------------

		// Diffuse IBL
		VN = max(dot(V, N), 0.0);
		vec3 R = 2.0 * VN * N - V;
		float RN = max(dot(R, N), 0.0);
		vec3 Fd = Kd / PI;

		vec2 irradianceN = vec2(-atan(-R.y, -R.x) / (2 * PI), acos(clamp(-R.z, -1.0, 1.0)) / PI);
		vec3 irrCalc = texture(irradianceMap, irradianceN).xyz * 50;
		vec3 diffuseFinal = Fd * irrCalc;

		// Specular IBL
		vec3 skydomeCalc = texture(skyboxMap, R.xy).xyz;
		vec3 specularCalc = skydomeCalc * max(dot(N, R), 0.0);

		float D = DistributionGGX(HN, roughness);
		float G = SmithMethod(VN, LN, roughness);
		vec3 F = SchlickFresnel(HV, specular);
		vec3 Fs = (F * G * D) / max(4.0 * RN * VN, 0.01);
		Fs = min(Fs, vec3(10.0));

		vec3 totalBRDF = Fd + Fs;
		vec3 directLight = totalBRDF * Light * LN;
		vec3 ambient = Ambient * Kd;

		vec3 specularFinal = specular * Fs;

		// Reflection Calculation ------------------------

		if (reflective)
		{
			float distR = length(R);
			vec3 normalR = normalize(R);
			float a = normalR.x;
			float b = normalR.y;
			float c = normalR.z;

			float reflectDir = c > 0.0 ? 1.0 : -1.0; // double check this

			a = (a / (1.0 + (c * reflectDir)));
			b = (b / (1.0 + (c * reflectDir)));

			vec2 reflectUV = vec2(a, b) * 0.5 + vec2(0.5, 0.5);

			if (reflectDir > 0.0)
			{
				return texture(reflectionTop, reflectUV).xyz;
			}
			else
			{
				return texture(reflectionBottom, reflectUV).xyz;
			}
		}

		// End Reflection Calculation ------------------------

		if (PixelInShadow()) 
		{
			//return ambient;
			return diffuseFinal + specularFinal;
		}

		return directLight + (diffuseFinal + specularFinal);
		//return directLight + ambient;
		//return diffuseFinal + specularFinal;
	}
	else
	{
		return Kd;
	}
	return Kd;
}