#version 330 core
layout (location = 0) in vec4 vertex;
layout (location = 1) in vec3 vertexNormal;
layout (location = 2) in vec2 vertexTexture;
layout (location = 3) in vec3 vertexTangent;

out vec3 worldPos;
out vec2 TexCoords;
out vec3 Normal;
out vec3 tanVec;

out vec3 lightVec, eyeVec;

uniform vec3 lightPos;
uniform vec3 eye;
uniform mat4 WorldView;
uniform mat4 WorldProj;
uniform mat4 ModelTr;
uniform mat4 NormalTr;

// Snow vertex displacement
// GL_R32I integer texture — must use isampler2D; heights are stored as int × 1000
uniform isampler2D snowHeightTex;
uniform float      snowDispScale; // max pile height in world units
uniform int        snowEnabled;
uniform int        objectId;

// Must match ObjectIds enum in scene.h
const int groundId = 3;
const int floorId  = 11;

void main()
{
    vec4 worldVertex = ModelTr * vertex;
    vec3 geomNormal  = vertexNormal * mat3(NormalTr);

    if (snowEnabled != 0 && (objectId == groundId || objectId == floorId))
    {
        // Map world XY [-100, 100] to UV [0, 1]
        vec2 gridUV = clamp((worldVertex.xy / 100.0) * 0.5 + 0.5, 0.0, 1.0);

        float h = float(texture(snowHeightTex, gridUV).r) / 1000.0 * snowDispScale;

        // Displace vertex upward in Z (Z is up in this scene)
        worldVertex.z += h;

        // Recompute normal via central differences on the height field.
        // One texel spans (200 / SNOW_GRID_SIZE) world units in XY.
        float eps       = 1.0 / 256.0;          // one texel in UV space
        float texelSize = 200.0 / 256.0;        // world-space size of one texel

        float hL = float(texture(snowHeightTex, gridUV + vec2(-eps, 0.0)).r) / 1000.0 * snowDispScale;
        float hR = float(texture(snowHeightTex, gridUV + vec2( eps, 0.0)).r) / 1000.0 * snowDispScale;
        float hD = float(texture(snowHeightTex, gridUV + vec2(0.0, -eps)).r) / 1000.0 * snowDispScale;
        float hU = float(texture(snowHeightTex, gridUV + vec2(0.0,  eps)).r) / 1000.0 * snowDispScale;

        // Central-difference surface normal for the snow layer
        // (hL-hR) and (hD-hU) are the Z gradients; 2*texelSize is the XY span
        vec3 snowNormal = normalize(vec3(hL - hR, hD - hU, 2.0 * texelSize));

        // Blend toward snow normal based on how much snow is present
        float blend = clamp(h / max(snowDispScale, 0.001), 0.0, 1.0);
        geomNormal = normalize(mix(geomNormal, snowNormal, blend));
    }

    worldPos  = worldVertex.xyz;
    TexCoords = vertexTexture;
    Normal    = geomNormal;
    tanVec    = mat3(ModelTr) * vertexTangent;
    lightVec  = lightPos - worldPos;
    eyeVec    = eye     - worldPos;

    gl_Position = WorldProj * WorldView * worldVertex;
}
