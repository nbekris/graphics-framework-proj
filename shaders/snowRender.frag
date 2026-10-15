#version 330 core

// Snow particle fragment shader.
// Renders each point as a soft circular sprite (Paper Section III.E).

out vec4 fragColor;

void main()
{
    // gl_PointCoord in [0,1]^2 — map to [-1,1]^2 to measure distance from centre
    vec2  uv = gl_PointCoord * 2.0 - 1.0;
    float d  = dot(uv, uv);
    if (d > 1.0) discard;          // clip outside the circle

    float alpha = (1.0 - d) * 0.85;   // smooth falloff toward edge
    fragColor = vec4(0.95, 0.97, 1.0, alpha);
}
