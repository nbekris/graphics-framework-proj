#version 330 core

// These correspond to GL_COLOR_ATTACHMENT 0, 1, and 2
layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;

uniform sampler2D tex;       // Object.cpp binds texture to "tex"
uniform vec3 diffuse;        // Object.cpp sends "diffuse"
uniform float hasTexture;    // Object.cpp sends "hasTexture" as a float
uniform vec3 specular;       // Object.cpp sends "specular"

void main()
{    
    // Position
    gPosition = FragPos;

    // Normal
    gNormal = normalize(Normal);

    // Albedo (Color)
    // Your C++ sends hasTexture as a float (0.0 or 1.0)
    if (hasTexture > 0.5) {
        gAlbedoSpec.rgb = texture(tex, TexCoords).rgb;
    } else {
        gAlbedoSpec.rgb = diffuse;
    }

    // Specular Intensity
    // We'll store the "red" component of the specular color as intensity
    // (Simple approximation for deferred)
    gAlbedoSpec.a = specular.r; 
}