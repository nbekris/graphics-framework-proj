#version 330 core

// Snow particle render vertex shader.
// No vertex buffer is needed: gl_VertexID indexes into the position texture
// directly, replacing the paper's glReadPixels approach (Paper Section III.D).

uniform sampler2D posTex;      // current particle positions
uniform int       texWidth;    // width of the particle texture (SNOW_TEX_SIZE)
uniform mat4      WorldView;
uniform mat4      WorldProj;
uniform float     particleSize;

void main()
{
    // Decode linear particle index back to 2-D texture coordinate
    ivec2 coord = ivec2(gl_VertexID % texWidth, gl_VertexID / texWidth);
    vec3  pos   = texelFetch(posTex, coord, 0).xyz;

    gl_Position  = WorldProj * WorldView * vec4(pos, 1.0);
    gl_PointSize = particleSize;
}
