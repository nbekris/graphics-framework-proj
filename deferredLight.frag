#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform int viewMode;

void main()
{         
    // 1. Retrieve data
    vec3 FragPos = texture(gPosition, TexCoords).rgb;
    vec3 Normal = texture(gNormal, TexCoords).rgb;
    vec3 Diffuse = texture(gAlbedoSpec, TexCoords).rgb;
    float Specular = texture(gAlbedoSpec, TexCoords).a;

    // --- DEBUG MODES ---
    if (viewMode == 1) {
        // Position: Divide by 50 to see gradients (adjust if scene is larger/smaller)
        FragColor = vec4(abs(FragPos) / 50.0, 1.0);
        return;
    }
    else if (viewMode == 2) {
        // Normal: Map [-1, 1] to [0, 1] so it looks colorful
        FragColor = vec4(Normal * 0.5 + 0.5, 1.0);
        return;
    }
    else if (viewMode == 3) {
        // Albedo: Just show the color
        FragColor = vec4(Diffuse, 1.0);
        return;
    }
    else if (viewMode == 4) {
        // Specular: Show intensity as greyscale
        FragColor = vec4(vec3(Specular), 1.0);
        return;
    }
    
    // 2. Lighting Constants
    vec3 lighting = Diffuse * 0.1; // Hardcoded Ambient (0.1)
    vec3 viewDir  = normalize(viewPos - FragPos);
    
    // 3. Calculate Light
    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 diffuse = max(dot(Normal, lightDir), 0.0) * Diffuse * lightColor;
    
    // Blinn-Phong Specular
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    float spec = pow(max(dot(Normal, halfwayDir), 0.0), 16.0);
    vec3 specular = lightColor * spec * Specular;
    
    // 4. Attenuation (Simple Linear)
    // Adjustable constants for a HUGE world
    float constant  = 1.0;
    float linear    = 0.00000014; // Very small falloff
    float quadratic = 0.0000000007; // Extremely small curve

    float distance    = length(lightPos - FragPos);
    float attenuation = 1.0 / (constant + linear * distance + quadratic * (distance * distance));
    
    FragColor = vec4((diffuse + specular) * attenuation, 1.0);
}