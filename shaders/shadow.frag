/////////////////////////////////////////////////////////////////////////
// Pixel shader for shadows
// Stores relative depth moments (z, z^2, z^3, z^4) for MSM
////////////////////////////////////////////////////////////////////////
#version 330

out vec4 FragColor;

in vec4 vPosition;

uniform float z0; // Near depth estimate from light
uniform float z1; // Far depth estimate from light

void main()
{
	// Relative depth: maps [z0, z1] to [0, 1]
	float z = clamp((vPosition.w - z0) / (z1 - z0), 0.0, 1.0);

	FragColor = vec4(z, z*z, z*z*z, z*z*z*z);
}
