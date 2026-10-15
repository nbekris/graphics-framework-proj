/////////////////////////////////////////////////////////////////////////
// Pixel shader for lighting
////////////////////////////////////////////////////////////////////////
#version 330

out vec4 FragColor;

vec3 LightingPixel();
void main()
{
    vec3 cIn = LightingPixel();
    //Exposure Control
    vec3 exposeControl = 10.0 * cIn; 
    // Tone Mapping & Gamma
    float k = 7.0; 
    vec3 colorSpaceConversion = vec3(k / 2.2);

    vec3 cOut = pow(exposeControl / (exposeControl + vec3(1.0)), colorSpaceConversion);

    FragColor.xyz = cOut;
}
