#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec4 shadowCoord;

uniform sampler2D gFragData[4];
uniform sampler2D gLightVec;
uniform sampler2D gEyeVec;
uniform sampler2D shadowMap;
uniform sampler2D preBlurShadowMap;
uniform sampler2D skyboxMap;
uniform sampler2D reflectionTop;
uniform sampler2D reflectionBottom;
uniform sampler2D ssaoTex;
uniform vec3 shCoeffs[9];
uniform mat4 ShadowMatrix;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform int viewMode;
uniform float width, height;
uniform vec3 Ambient;
uniform float linstepLo;
uniform float linstepHi;

uniform vec3 sceneLightPos;
uniform vec3 sceneEye;

// Relative depth range (same values used in shadow.frag)
uniform float z0;
uniform float z1;

const float PI = 3.14159265359;

// Hammersley low-discrepancy sequence for IBL specular
layout(std140) uniform HammersleyBlock {
	int numSamples;
	vec4 hammersley[50]; // 2 pairs per vec4: (u0,v0, u1,v1)
};

uniform float skyWidth, skyHeight;
uniform float exposure;
uniform int enableDirectLight;

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

// ---------------------------------------------------------------
// Hamburger 4MSM (Algorithm 3 from the MSM paper)
// Uses Cholesky decomposition to solve the 3x3 Hankel system.
//
// Input:  b = (z, z^2, z^3, z^4) from blurred shadow map
//         zf = fragment depth (relative depth)
// Output: G = shadow intensity (0 = lit, 1 = shadowed)
//
// Final lighting uses: ambient + (1-G) * [diffuse + specular]
// ---------------------------------------------------------------
float Hamburger4MSM(vec4 b, float zf)
{
    // Bias moments toward 0.5 to prevent singularity
    float alpha = 1.0e-3;
    vec4 bp = (1.0 - alpha) * b + alpha * vec4(0.5);

    // Build the 3x3 symmetric Hankel matrix and solve via Cholesky
    float m11 = 1.0;
    float m12 = bp.x;
    float m13 = bp.y;
    float m22 = bp.y;
    float m23 = bp.z;
    float m33 = bp.w;

    float rhs1 = 1.0;
    float rhs2 = zf;
    float rhs3 = zf * zf;

    float a_ch = sqrt(max(m11, 1e-8));
    float b_ch = m12 / a_ch;
    float c_ch = m13 / a_ch;
    float d_ch = sqrt(max(m22 - b_ch * b_ch, 1e-8));
    float e_ch = (m23 - b_ch * c_ch) / d_ch;
    float f_ch = sqrt(max(m33 - c_ch * c_ch - e_ch * e_ch, 1e-8));

    // Forward substitution: L * chat = rhs
    float chat1 = rhs1 / a_ch;
    float chat2 = (rhs2 - b_ch * chat1) / d_ch;
    float chat3 = (rhs3 - c_ch * chat1 - e_ch * chat2) / f_ch;

    // Back substitution: L^T * c = chat
    float c3 = chat3 / f_ch;
    float c2 = (chat2 - e_ch * c3) / d_ch;
    float c1 = (chat1 - b_ch * c2 - c_ch * c3) / a_ch;

    // Solve the quadratic c3*z^2 + c2*z + c1 = 0
    float disc = c2 * c2 - 4.0 * c3 * c1;
    disc = max(disc, 0.0); // Guard against negative discriminant
    float sqrtDisc = sqrt(disc);

    float z2, z3;
    if (abs(c3) < 1e-6)
    {
        // Degenerate: linear equation c2*z + c1 = 0
        z2 = -c1 / max(abs(c2), 1e-6);
        z3 = z2;
    }
    else
    {
        z2 = (-c2 - sqrtDisc) / (2.0 * c3);
        z3 = (-c2 + sqrtDisc) / (2.0 * c3);
    }

    // Ensure z2 <= z3
    if (z2 > z3)
    {
        float tmp = z2;
        z2 = z3;
        z3 = tmp;
    }

    // Compute shadow intensity G
    if (zf <= z2)
    {
        // Fragment is in front of both roots => fully lit
        return 0.0;
    }
    else if (zf <= z3)
    {
        // Step 5: Fragment between roots
        float num = zf * z3 - bp.x * (zf + z3) + bp.y;
        float den = (z3 - z2) * (zf - z2);
        return max(num / max(abs(den), 1e-6), 0.0);
    }
    else
    {
        // Step 6: Fragment behind both roots
        float num = z2 * z3 - bp.x * (z2 + z3) + bp.y;
        float den = (zf - z2) * (zf - z3);
        return max(1.0 - num / max(abs(den), 1e-6), 0.0);
    }
}

// Evaluate L=0..2 spherical harmonics (9 coefficients) for a given normal
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

// Light bleeding reduction: remaps [threshold, 1] to [0, 1]
float linstep(float lo, float hi, float v)
{
    return clamp((v - lo) / (hi - lo), 0.0, 1.0);
}

float CalculateShadowMSM(vec3 FragPos, vec3 Normal, vec3 L)
{
    // Normal offset bias to reduce shadow acne
    float offsetScale = max(0.005 * (1.0 - dot(Normal, L)), 0.0005);
    vec3 biasedFragPos = FragPos + (Normal * offsetScale);

    // Project into light's clip space (ShadowMatrix includes bias matrix B)
    vec4 fragPosLightSpace = ShadowMatrix * vec4(biasedFragPos, 1.0);

    // Texture coordinates in [0,1] (bias matrix maps NDC [-1,1] to [0,1])
    vec2 shadowUV = fragPosLightSpace.xy / fragPosLightSpace.w;

    // Bounds check
    if (fragPosLightSpace.w <= 0.0 ||
        shadowUV.x < 0.0 || shadowUV.x > 1.0 ||
        shadowUV.y < 0.0 || shadowUV.y > 1.0)
    {
        return 0.0; // Outside shadow map => lit (G=0)
    }

    // Compute relative fragment depth (same transform as shadow.frag)
    float zf = clamp((fragPosLightSpace.w - z0) / (z1 - z0), 0.0, 1.0);

    // Sample blurred moments from shadow map
    vec4 moments = texture(shadowMap, shadowUV);

    // Run Hamburger 4MSM algorithm
    float G = Hamburger4MSM(moments, zf);

    // Light bleeding reduction + intensity boost:
    // MSM typically returns G in ~[0, 0.5] range after blurring.
    // linstepLo crushes light-bleed; linstepHi amplifies shadow intensity.
    G = linstep(linstepLo, linstepHi, G);

    return G;
}

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

void main()
{
	vec2 uv = gl_FragCoord.xy / vec2(width, height);

	// Retrieve g buffer data
	vec3 FragPos = texture(gFragData[0], uv).rgb;
	int objectId = int(round(texture(gFragData[0], uv).a));
	vec3 Normal = texture(gFragData[1], uv).rgb;
	float emissiveFlag = texture(gFragData[1], uv).a;
	vec3 Diffuse = texture(gFragData[2], uv).rgb;
	float reflectiveFlag = texture(gFragData[2], uv).a;
	vec3 Specular = texture(gFragData[3], uv).rgb;
	float Shininess = texture(gFragData[3], uv).a;

	vec3 lightVec = texture(gLightVec, uv).rgb;
	vec3 eyeVec = texture(gEyeVec, uv).rgb;

	vec3 N = normalize(Normal);
	vec3 L = normalize(lightVec);
	vec3 V = normalize(eyeVec);
	vec3 H = normalize(L + V);

	// Sky/emissive pixels: sample skybox with view-direction spherical UVs and output directly
	if (emissiveFlag < 0.5)
	{
		vec2 skyUV = vec2(-atan(V.y, V.x) / (2.0 * PI), acos(clamp(V.z, -1.0, 1.0)) / PI);

		if (viewMode == 11)
		{
			// Debug: visualize SH irradiance map on sky sphere
			vec3 irr = max(EvaluateSH(-V), vec3(0.0));
			vec3 exposed = exposure * irr;
			FragColor = vec4(pow(exposed / (exposed + vec3(1.0)), vec3(1.0/2.2)), 1.0);
		}
		else
		{
			// Normal sky rendering (also used for mode 10 HDR skybox view)
			vec3 skyColor = texture(skyboxMap, skyUV).xyz;
			vec3 exposed = exposure * skyColor;
			FragColor = vec4(pow(exposed / (exposed + vec3(1.0)), vec3(1.0/2.2)), 1.0);
		}
		return;
	}

	// Reflective pixels: sample dual-paraboloid reflection maps
	vec3 reflectColor = vec3(0.0);
	if (reflectiveFlag > 0.5)
	{
		float VN_r = max(dot(V, N), 0.0);
		vec3 R = 2.0 * VN_r * N - V;

		vec3 normalR = normalize(R);
		float a = normalR.x;
		float b = normalR.y;
		float c = normalR.z;

		float reflectDir = c > 0.0 ? 1.0 : -1.0;

		a = a / (1.0 + c * reflectDir);
		b = b / (1.0 + c * reflectDir);

		vec2 reflectUV = vec2(a, b) * 0.5 + vec2(0.5, 0.5);

		if (reflectDir > 0.0)
			reflectColor = texture(reflectionTop, reflectUV).xyz;
		else
			reflectColor = texture(reflectionBottom, reflectUV).xyz;
	}

	float LN = max(dot(L, N), 0.0);
	float HN = max(dot(H, N), 0.0);
	float VN = max(dot(V, N), 0.0);
	float LH = max(dot(L, H), 0.0);
	float HV = max(dot(H,V), 0.0);

	float roughness = sqrt(2 / (Shininess + 2)); //conversion from phong to GGX

	vec3 kD = Diffuse;
	vec3 Fd = kD / PI;

//	float D = DistributionGGX(HN, roughness);
//	float G_brdf = SmithMethod(VN, LN, roughness);
//	vec3 F = SchlickFresnel(HV, Specular);
//	vec3 Fs = (F * G_brdf * D) / max(4.0 * LN * VN, 0.01);

	// Diffuse IBL: evaluate spherical harmonics with surface normal
	vec3 irrCalc = max(EvaluateSH(N), vec3(0.0));
	vec3 diffuseIBL = Fd * irrCalc;

	// Specular IBL: Monte-Carlo importance sampling
	vec3 R = 2.0 * VN * N - V;

	// Build tangent frame around R; avoid degenerate cross when R is near Z-axis
	vec3 up = (abs(R.z) < 0.999) ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 A = normalize(cross(up, R));
	vec3 B_frame = normalize(cross(R, A));

	int n = numSamples;
	vec3 specularIBL = vec3(0.0);
	for (int k = 0; k < n; k++) {
		vec4 h = hammersley[k / 2];
		vec2 xi = (k % 2 == 0) ? h.xy : h.zw;
		float xi1 = xi.x;
		float xi2 = xi.y;

		// GGX importance sampling: skew xi2 to match D(H)
		float theta = atan(roughness * sqrt(xi2), sqrt(1.0 - xi2));

		// Direction centered around Z via vectorOf(xi1, theta/PI)
		vec3 D_vec = vec3(cos(2.0*PI*(0.5 - xi1)) * sin(theta),
		                  sin(2.0*PI*(0.5 - xi1)) * sin(theta),
		                  cos(theta));

		// Rotate from Z-axis to reflection direction R
		vec3 omega_k = normalize(D_vec.x * A + D_vec.y * B_frame + D_vec.z * R);

		float NdotOmega = max(dot(N, omega_k), 0.0);
		if (NdotOmega <= 0.0) continue;

		// Half vector for this sample; skip degenerate case where omega_k ≈ -V
		vec3 H_k_unnorm = omega_k + V;
		if (dot(H_k_unnorm, H_k_unnorm) < 1e-6) continue;
		vec3 H_k = normalize(H_k_unnorm);
		float HkN = max(dot(H_k, N), 0.0);
		float HkV = max(dot(H_k, V), 0.0);

		// Mipmap level calculation
		float D_H = DistributionGGX(HkN, roughness);
		float level = 0.5 * log2(skyWidth * skyHeight / float(n))
		            - 0.5 * log2(D_H / 4.0) - 1.0;

		// Sample environment map at calculated mip level
		// Negate omega_k to match sky dome's inward-pointing UV convention
		vec2 uv_k = vec2(-atan(-omega_k.y, -omega_k.x) / (2.0*PI),
		                  acos(clamp(-omega_k.z, -1.0, 1.0)) / PI);
		vec3 Li = textureLod(skyboxMap, uv_k, max(level, 0.0)).xyz;

		// Monte-Carlo estimator (eq 3): D(H) canceled by p(omega_k)
		float G_k = SmithMethod(max(VN, 0.01), NdotOmega, roughness);
		vec3  F_k = SchlickFresnel(HkV, Specular);

		vec3 sampleContrib = Li * G_k * F_k / (4.0 * max(VN, 0.01));
		// Guard against NaN from degenerate geometry
		if (any(isnan(sampleContrib)) || any(isinf(sampleContrib))) continue;
		specularIBL += sampleContrib;
	}
	specularIBL /= max(float(n), 1.0);

	// Clamp specular IBL to suppress fireflies from Monte-Carlo variance
	// at grazing angles where the 1/VN term amplifies sample noise
	specularIBL = min(specularIBL, vec3(10.0));

	// Direct light BRDF
	float D = DistributionGGX(HN, roughness);
	float G_brdf = SmithMethod(VN, LN, roughness);
	vec3 F = SchlickFresnel(HV, Specular);
	vec3 Fs = (F * G_brdf * D) / max(4.0 * LN * VN, 0.01);
	Fs = min(Fs, vec3(10.0));

	vec3 totalBRDF = Fd + Fs;
	vec3 directLight = (enableDirectLight == 1) ? totalBRDF * lightColor * LN : vec3(0.0);
	// Apply SSAO: AO modulates ambient light only, not direct light
	float ao = texture(ssaoTex, uv).r;
	vec3 ambient = ao * (diffuseIBL + specularIBL);

	// MSM shadow: G is shadow intensity (0=lit, 1=shadowed)
	// Lighting = ambient + (1-G) * [diffuse + specular]
	float G_shadow = CalculateShadowMSM(FragPos, N, L);

	if (viewMode == 1)
	{
		// Debug: visualize shadow intensity (black=lit, white=shadowed)
		FragColor = vec4(vec3(G_shadow), 1.0);
		return;
	}
	else if (viewMode >= 2 && viewMode <= 5)
	{
		// Debug: visualize shadow map moments from light's perspective
		// Project fragment into light space to get shadow UV
		vec4 fragPosLightSpace = ShadowMatrix * vec4(FragPos, 1.0);
		vec2 shadowUV = fragPosLightSpace.xy / fragPosLightSpace.w;

		if (fragPosLightSpace.w > 0.0 &&
			shadowUV.x >= 0.0 && shadowUV.x <= 1.0 &&
			shadowUV.y >= 0.0 && shadowUV.y <= 1.0)
		{
			vec4 moments = texture(shadowMap, shadowUV);
			float val = 0.0;
			if (viewMode == 2) val = moments.r; // z
			else if (viewMode == 3) val = moments.g; // z^2
			else if (viewMode == 4) val = moments.b; // z^3
			else if (viewMode == 5) val = moments.a; // z^4
			FragColor = vec4(vec3(val), 1.0);
		}
		else
		{
			FragColor = vec4(0.0, 0.0, 0.0, 1.0); // Outside shadow map
		}
		return;
	}
	else if (viewMode == 6)
	{
		// Debug: pre-blur moments (z channel only)
		vec4 fragPosLightSpace = ShadowMatrix * vec4(FragPos, 1.0);
		vec2 shadowUV = fragPosLightSpace.xy / fragPosLightSpace.w;

		if (fragPosLightSpace.w > 0.0 &&
			shadowUV.x >= 0.0 && shadowUV.x <= 1.0 &&
			shadowUV.y >= 0.0 && shadowUV.y <= 1.0)
		{
			float val = texture(preBlurShadowMap, shadowUV).r;
			FragColor = vec4(vec3(val), 1.0);
		}
		else
		{
			FragColor = vec4(0.0, 0.0, 0.0, 1.0);
		}
		return;
	}
	else if (viewMode == 7)
	{
		// Debug: split-screen comparison (left=pre-blur, right=post-blur)
		vec4 fragPosLightSpace = ShadowMatrix * vec4(FragPos, 1.0);
		vec2 shadowUV = fragPosLightSpace.xy / fragPosLightSpace.w;
		vec2 uv2 = gl_FragCoord.xy / vec2(width, height);

		if (fragPosLightSpace.w > 0.0 &&
			shadowUV.x >= 0.0 && shadowUV.x <= 1.0 &&
			shadowUV.y >= 0.0 && shadowUV.y <= 1.0)
		{
			float val;
			if (uv2.x < 0.5)
				val = texture(preBlurShadowMap, shadowUV).r; // Left: pre-blur
			else
				val = texture(shadowMap, shadowUV).r;        // Right: post-blur

			FragColor = vec4(vec3(val), 1.0);

			// Draw a thin white divider line at the center
			if (abs(uv2.x - 0.5) < 0.002)
				FragColor = vec4(1.0, 0.0, 0.0, 1.0);
		}
		else
		{
			FragColor = vec4(0.0, 0.0, 0.0, 1.0);
		}
		return;
	}
	else if (viewMode == 8)
	{
		// Debug: visualize SH irradiance evaluated with surface normal
		vec3 shIrr = max(EvaluateSH(N), vec3(0.0));
		vec3 exposed = exposure * shIrr;
		FragColor = vec4(pow(exposed / (exposed + vec3(1.0)), vec3(1.0/2.2)), 1.0);
		return;
	}
	else if (viewMode == 9)
	{
		// Debug: visualize SH reconstruction on sky sphere using view direction
		vec3 shSky = max(EvaluateSH(V), vec3(0.0));
		vec3 exposed = exposure * shSky;
		FragColor = vec4(pow(exposed / (exposed + vec3(1.0)), vec3(1.0/2.2)), 1.0);
		return;
	}

	else if (viewMode == 10)
	{
		// Debug: visualize raw HDR skybox projected onto geometry via view direction
		vec2 skyUV = vec2(-atan(V.y, V.x) / (2.0 * PI), acos(clamp(V.z, -1.0, 1.0)) / PI);
		vec3 hdr = texture(skyboxMap, skyUV).xyz;
		vec3 exposed = exposure * hdr;
		FragColor = vec4(pow(exposed / (exposed + vec3(1.0)), vec3(1.0/2.2)), 1.0);
		return;
	}
	else if (viewMode == 11)
	{
		// Debug: visualize SH irradiance on surfaces (evaluated with surface normal)
		vec3 irr = max(EvaluateSH(N), vec3(0.0));
		vec3 exposed = exposure * irr;
		FragColor = vec4(pow(exposed / (exposed + vec3(1.0)), vec3(1.0/2.2)), 1.0);
		return;
	}
	else if (viewMode == 12)
	{
		// Debug: visualize SSAO factor (white=no occlusion, black=fully occluded)
		FragColor = vec4(vec3(ao), 1.0);
		return;
	}

	vec3 hdrColor = ambient + (1.0 - G_shadow) * directLight;

	// Blend in dual-paraboloid reflection for reflective surfaces
	if (reflectiveFlag > 0.5)
	{
		vec3 F_refl = SchlickFresnel(VN, Specular);
		hdrColor = mix(hdrColor, reflectColor, F_refl);
	}

	// Exposure control + tone mapping + gamma: C_out = (e*C / (e*C + 1))^(1/2.2)
	vec3 exposed = exposure * hdrColor;
	vec3 toneMapped = pow(exposed / (exposed + vec3(1.0)), vec3(1.0/2.2));

	FragColor = vec4(toneMapped, 1.0);
}
