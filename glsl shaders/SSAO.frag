#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPositionTex;  // gFragData[0]: worldPos.xyz, objectId in .a
uniform sampler2D gNormalTex;    // gFragData[1]: normal.xyz, emissiveFlag in .a

uniform mat4 WorldView;
uniform float width;
uniform float height;

// SSAO parameters
uniform float radius;    // R - range of influence (world units)
uniform float ssaoScale; // s - scale factor
uniform float contrast;  // k - contrast (power) factor
uniform int numSamples;  // n - number of sample points

const float PI = 3.14159265359;
const float delta = 0.001; // depth bias to prevent self-occlusion

void main()
{
    vec2 uv = gl_FragCoord.xy / vec2(width, height);

    vec3 P = texture(gPositionTex, uv).rgb;
    float emissiveFlag = texture(gNormalTex, uv).a;
    vec3 N = normalize(texture(gNormalTex, uv).rgb);

    // Skip sky/emissive pixels
    if (emissiveFlag < 0.5)
    {
        FragColor = vec4(1.0);
        return;
    }

    // Camera-space depth of this pixel (positive distance from camera)
    float d = -(WorldView * vec4(P, 1.0)).z;

    // Avoid division by zero for very close geometry
    if (d < 0.01)
    {
        FragColor = vec4(1.0);
        return;
    }

    float R = radius;
    float c = 0.1 * R;
    int n = numSamples;

    // Pseudo-random rotation hash (PDF eq: phi = (30*x' ^ y') + 10*x'*y')
    int xp = int(gl_FragCoord.x);
    int yp = int(gl_FragCoord.y);
    float phi = float(((30 * xp) ^ yp) + 10 * xp * yp);

    float S = 0.0;

    for (int i = 0; i < n; i++)
    {
        float alpha = (float(i) + 0.5) / float(n);
        float h = alpha * R / d;  // spiral radius projected to screen space
        float theta = 2.0 * PI * alpha * (7.0 * float(n) / 9.0) + phi;

        // Sample point UV on screen
        vec2 sampleUV = uv + h * vec2(cos(theta), sin(theta));

        // Clamp to valid UV range
        sampleUV = clamp(sampleUV, vec2(0.0), vec2(1.0));

        // World position of sample point
        vec3 Pi = texture(gPositionTex, sampleUV).rgb;

        // Camera-space depth of sample point
        float di = -(WorldView * vec4(Pi, 1.0)).z;

        vec3 omega_i = Pi - P;
        float omega_len_sq = dot(omega_i, omega_i);

        // Heaviside: exclude points outside range of influence
        float H = (R - length(omega_i)) > 0.0 ? 1.0 : 0.0;

        // Alchemy AO occlusion term
        float numerator = max(0.0, dot(N, omega_i) - delta * di);
        float denominator = max(c * c, omega_len_sq);

        S += (numerator * H) / denominator;
    }

    S *= (2.0 * PI * c) / float(n);

    // Final ambient factor: A = max(0, 1 - s*S)^k
    float A = pow(max(0.0, 1.0 - ssaoScale * S), contrast);

    FragColor = vec4(A, A, A, 1.0);
}
