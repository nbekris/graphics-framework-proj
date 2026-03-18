#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec4 shadowCoord;

uniform sampler2D gFragData[4];
uniform sampler2D gLightVec;
uniform sampler2D gEyeVec;
uniform sampler2D shadowMap;
uniform sampler2D preBlurShadowMap;
uniform mat4 ShadowMatrix;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform int viewMode;
uniform float width, height;
uniform vec3 Ambient;

uniform vec3 sceneLightPos;
uniform vec3 sceneEye;

// Relative depth range (same values used in shadow.frag)
uniform float z0;
uniform float z1;

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
    // Step 1: Bias moments toward 0.5 to prevent singularity
    float alpha = 1.0e-3;
    vec4 bp = (1.0 - alpha) * b + alpha * vec4(0.5);

    // Step 2: Build the 3x3 symmetric Hankel matrix and solve via Cholesky
    //
    //  | 1     bp.x  bp.y |   | c1 |   | 1    |
    //  | bp.x  bp.y  bp.z | * | c2 | = | zf   |
    //  | bp.y  bp.z  bp.w |   | c3 |   | zf^2 |
    //
    float m11 = 1.0;
    float m12 = bp.x;
    float m13 = bp.y;
    float m22 = bp.y;
    float m23 = bp.z;
    float m33 = bp.w;

    float rhs1 = 1.0;
    float rhs2 = zf;
    float rhs3 = zf * zf;

    // Cholesky: M = L * L^T
    // L = | a  0  0 |    L^T = | a  b_ch  c_ch |
    //     | b  d  0 |          | 0  d      e    |
    //     | c  e  f |          | 0  0      f    |
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

    // Step 3: Solve the quadratic c3*z^2 + c2*z + c1 = 0
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

    // Steps 4-6: Compute shadow intensity G
    if (zf <= z2)
    {
        // Step 4: Fragment is in front of both roots => fully lit
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

    // Light bleeding reduction: crush low shadow values to zero
    G = linstep(0.4, 1.0, G);

    return G;
}

void main()
{
	vec2 uv = gl_FragCoord.xy / vec2(width, height);

	// Retrieve g buffer data
	vec3 FragPos = texture(gFragData[0], uv).rgb;
	vec3 Normal = texture(gFragData[1], uv).rgb;
	vec3 Diffuse = texture(gFragData[2], uv).rgb;
	vec3 Specular = texture(gFragData[3], uv).rgb;
	float Shininess = texture(gFragData[3], uv).a;

	vec3 lightVec = texture(gLightVec, uv).rgb;
	vec3 eyeVec = texture(gEyeVec, uv).rgb;

	vec3 N = normalize(Normal);
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
	float G_brdf = SmithMethod(VN, LN, roughness);
	vec3 F = SchlickFresnel(HV, Specular);
	vec3 Fs = (F * G_brdf * D) / max(4.0 * LN * VN, 0.001);

	vec3 totalBRDF = Fd + Fs;
	vec3 directLight = totalBRDF * lightColor * LN;
	vec3 ambient = Ambient * kD;

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

	FragColor = vec4(ambient + (1.0 - G_shadow) * directLight, 1.0);
}
