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

uniform int objectId;

// Values describing the surface
uniform vec3 diffuse; //Kd
uniform vec3 specular; // Ks
uniform float shininess; // alpha exponent

uniform sampler2D tex;

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

void main()
{
    vec3 N = normalize(normalVec);
    vec3 L = normalize(lightVec);
	vec3 V = normalize(eyeVec);
	vec3 H = normalize(L + V);

    vec3 Kd = diffuse;
	float roughness = sqrt(2 / (shininess + 2)); //conversion from phong to GGX
	
	float LN = max(dot(L, N), 0.0);
	float HN = max(dot(H, N), 0.0);
	float VN = max(dot(V, N), 0.0);
	float LH = max(dot(L, H), 0.0);

    // A checkerboard pattern to break up large flat expanses.  Remove when using textures.
//    if (objectId==groundId || objectId==floorId || objectId==seaId) {
//        ivec2 uv = ivec2(floor(100.0*texCoord));
//        if ((uv[0]+uv[1])%2==0)
//            Kd *= 0.9; }

	//Kd = texture(tex, texCoord).xyz;

	if (objectId==lPicId)
	{
		ivec2 uv = ivec2(floor(100.0*texCoord));
        if ((uv[0]+uv[1])%2==0)
			Kd *= 0.9;
	}

	if (objectId==skyId) 
	{
		FragColor.xy = vec2(-atan(V.y, V.x) / (2 * PI), acos(V.z / PI));
	}

	vec3 F = SchlickFresnel(LH, specular);
	float D = DistributionGGX(HN, roughness);
	float G = SmithMethod(VN, LN, roughness);
	vec3 Fs = (F * G * D) / max(4.0 * LN * VN, 0.001);
	
	vec3 Fd = Kd / PI;
	vec3 totalBRDF = Fd + Fs;
	vec3 directLight = totalBRDF * Light * LN;
	vec3 ambient = Ambient * Kd;
	vec3 lighting = directLight + ambient;

    //FragColor.xyz = vec3(0.5,0.5,0.5)*Kd + Kd*max(dot(L,N),0.0); // old calculation
	FragColor.xyz = lighting;
	
}
