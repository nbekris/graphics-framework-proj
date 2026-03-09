#version 400 core
out vec4 FragColor;

in vec4 shadowCoord;

uniform sampler2D gFragData[4];
uniform sampler2D gLightVec;
uniform sampler2D gEyeVec;
uniform sampler2D shadowMap;
uniform mat4 ShadowMatrix;

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

bool PixelInShadow(vec3 FragPos, vec3 lightVec, vec3 Normal)
{
	vec4 fragPosLightSpace = ShadowMatrix * vec4(FragPos, 1.0);

	float lightDepth;
	float pixelDepth;
	vec2 shadowIndex = fragPosLightSpace.xy / fragPosLightSpace.w;
	bool isShadowed = false;
	float bias = max(0.005 * (1.0 - dot(Normal, lightVec)), 0.0005);

	if (fragPosLightSpace.w > 0 && ((shadowIndex.x > 0 && shadowIndex.x < 1) && (shadowIndex.y > 0 && shadowIndex.y < 1)))
	{
		lightDepth = texture(shadowMap, shadowIndex).w;
		pixelDepth = fragPosLightSpace.w;

		isShadowed = pixelDepth - bias > lightDepth;
	}

	return isShadowed;
}

// A small helper to compute the roots of a quadratic equation
vec2 computeRoots(float c0, float c1) 
{
    float half_c1 = c1 * 0.5;
    // Ensure the discriminant is strictly positive to avoid NaN
    float discriminant = max(half_c1 * half_c1 - c0, 0.0); 
    float root = sqrt(discriminant);
    return vec2(-half_c1 - root, -half_c1 + root);
}

float MomentShadow(vec4 moments, float fragmentDepth) 
{
    // 1. Apply a very small bias to prevent precision issues and singularity
    float bias = 0.00003;
    vec4 b = mix(moments, vec4(0.5), bias);

    // 2. Compute the elements of the Cholesky factorization of the Hankel matrix
    float L32D22 = fma(-b.x, b.y, b.z);
    float D22 = fma(-b.x, b.x, b.y);
    float squaredDepthVariance = fma(-b.y, b.y, b.w);
    float D33D22 = dot(vec2(squaredDepthVariance, -L32D22), vec2(D22, L32D22));
    
    // SAFETY FIX: Prevent division by zero
    float InvD22 = 1.0 / max(D22, 0.00001);
    float L32 = L32D22 * InvD22;

    // SAFETY FIX: Prevent division by zero
    float InvD33 = 1.0 / max(D33D22, 0.00001);
    
    float c2 = L32 * L32 * InvD22 + InvD33;
    float c1_final = -L32 * InvD33;
    float c0_final = InvD22 - b.x * c1_final;

    // We normalize the polynomial by dividing by c2
    float invC2 = 1.0 / c2;
    vec2 roots = computeRoots(c0_final * invC2, c1_final * invC2);

    // 3. Calculate the shadow intensity based on where the fragment depth falls
    float z_f = fragmentDepth; 
    
    // If the fragment is in front of the closest occluder, it is fully lit
    if (z_f <= roots.x) 
    {
        return 0.0; 
    }
    // If it falls between the two roots, compute the probability distribution
    else if (z_f <= roots.y) 
    {
        float numerator = z_f - roots.x;
        // SAFETY FIX: Prevent division by zero when roots collapse
        float denominator = max(roots.y - roots.x, 0.00001);
        float quotient = numerator / denominator;
        
        // Shadow intensity
        return 1.0 - (1.0 - quotient) / (1.0 + quotient * quotient);
    }
    // If it is behind the furthest statistical root, it is fully in shadow
    else 
    {
        float numerator = z_f - roots.y;
        // SAFETY FIX: Prevent division by zero when roots collapse
        float denominator = max(roots.y - roots.x, 0.00001);
        float quotient = numerator / denominator;
        
        return 1.0 - 1.0 / (1.0 + quotient * quotient);
    }
}

float CalculateShadowMSM(vec3 FragPos, vec3 lightVec, vec3 Normal) 
{
	float offsetScale = max(0.005 * (1.0 - dot(Normal, lightVec)), 0.0005);
    vec3 biasedFragPos = FragPos + (Normal * offsetScale);

	vec4 fragPosLightSpace = ShadowMatrix * vec4(biasedFragPos, 1.0);

	vec3 shadowIndex = fragPosLightSpace.xyz / fragPosLightSpace.w;
	//float bias = max(0.005 * (1.0 - dot(Normal, lightVec)), 0.0005);
	shadowIndex = shadowIndex * 0.5 + 0.5; // Transform from [-1,1] to [0,1] for texture sampling

	if (shadowIndex.z > 1.0) {
		return 1.0; // Fully lit if beyond the far plane of the shadow map
	}

	vec4 moments = texture(shadowMap, shadowIndex.xy);
	float currentDepth = shadowIndex.z;

	return MomentShadow(moments, currentDepth);
}

void main()
{       
	vec2 uv = gl_FragCoord.xy / vec2(width, height);

	// Retrieve g buffer data
	vec3 FragPos = texture(gFragData[0], uv).rgb;
	vec3 Normal = texture(gFragData[1], uv).rgb;
	vec3 Diffuse = texture(gFragData[2], uv).rgb; // diffuse ie Kd
	vec3 Specular = texture(gFragData[3], uv).rgb; // specular ie Ks
	float Shininess = texture(gFragData[3], uv).a; // alpha

	vec3 lightVec = texture(gLightVec, uv).rgb;
	vec3 eyeVec = texture(gEyeVec, uv).rgb;

	// Leaving this here for if we want to pass light and eye vectors directly
	//vec3 L = normalize(sceneLightPos - FragPos);
	//vec3 V = normalize(sceneEye - FragPos);

	vec3 N = normalize(Normal); // Should probably just assign N to fragData[1]
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

	// BRDF calculations
	float D = DistributionGGX(HN, roughness);
	float G = SmithMethod(VN, LN, roughness);
	vec3 F = SchlickFresnel(HV, Specular);
	vec3 Fs = (F * G * D) / max(4.0 * LN * VN, 0.001);

	vec3 totalBRDF = Fd + Fs;
	vec3 directLight = totalBRDF * lightColor * LN;
	vec3 ambient = Ambient * kD;

	float shadowMask = CalculateShadowMSM(FragPos, N, L);

	FragColor = vec4((directLight * shadowMask) + ambient, 1.0);

//	if (PixelInShadow(FragPos, L, N)) {
//		FragColor = vec4(ambient, 1.0);
//	} else {
//		FragColor = vec4(directLight + ambient, 1.0);	
//	}
}