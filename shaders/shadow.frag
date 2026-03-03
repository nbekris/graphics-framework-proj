/////////////////////////////////////////////////////////////////////////
// Pixel shader for shadows
////////////////////////////////////////////////////////////////////////
#version 330

out vec4 FragColor;

in vec4 vPosition;

void main()
{
	float z = vPosition.z;

	float z1 = z;
	float z2 = z * z;
	float z3 = z2 * z;
	float z4 = z2 * z2;

	FragColor = vec4(z1, z2, z3, z4);
}