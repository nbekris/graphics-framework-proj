#version 330 core

layout (location = 0) out vec4 FragData[4];
layout (location = 4) out vec3 LightVec;
layout (location = 5) out vec3 EyeVec;

in vec2 TexCoords;
in vec3 worldPos;
in vec3 Normal;

in vec3 lightVec, eyeVec;

uniform vec3 diffuse;        // Object.cpp sends "diffuse" --- Kd
uniform float hasTexture;    // Object.cpp sends "hasTexture" as a float
uniform vec3 specular;       // Object.cpp sends "specular" ---- Ks
uniform float shininess;     // Alpha

uniform vec3 viewPos;
uniform vec3 lightPos;

uniform int objectId;
const int skyId = 1;

uniform sampler2D tex;

void main()
{
	FragData[0] = vec4(worldPos, 1.0);

	// Flag sky pixels as emissive (alpha=0) so deferred shader can render them unlit
	float emissiveFlag = (objectId == skyId) ? 0.0 : 1.0;
	FragData[1] = vec4(Normal, emissiveFlag);

	if (hasTexture > 0.5) {
		FragData[2] = vec4(texture(tex, TexCoords).rgb, 1.0);
	} else {
		FragData[2] = vec4(diffuse, 1.0);
	}

	FragData[3] = vec4(specular, shininess);

	LightVec = normalize(lightPos - worldPos);
    EyeVec   = normalize(viewPos - worldPos);
}