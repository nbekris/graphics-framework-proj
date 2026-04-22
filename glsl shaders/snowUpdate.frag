#version 330 core

// Snow particle physics update pass (Paper Section III.C)
// Reads current position/velocity, writes next position/velocity.
// Uses MRT: location 0 = position, location 1 = velocity.

uniform sampler2D posTex;   // current particle positions (xyz)
uniform sampler2D velTex;   // current particle velocities (xyz)
uniform float     dt;       // time step (seconds)
uniform vec3      wind;     // wind force (world units/s^2)
uniform vec3      spawnMin; // spawn box minimum corner
uniform vec3      spawnMax; // spawn box maximum corner
uniform float     groundZ;  // Z value of ground (Z is up in this scene)
uniform float     time;     // glfwGetTime() for varying random seeds

in vec2 TexCoords;

layout(location = 0) out vec4 outPos;
layout(location = 1) out vec4 outVel;

// Simple hash — seeded with time so respawned particles scatter each frame
float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
    vec3 pos = texture(posTex, TexCoords).xyz;
    vec3 vel = texture(velTex, TexCoords).xyz;

    // Accumulate forces: gravity in -Z (Z is up) + wind (Paper eq. 1)
    vec3 accel = vec3(0.0, 0.0, -9.8) + wind;

    // Euler integration (Paper eq. 2 & 3)
    vel = vel + accel * dt;
    pos = pos + vel * dt;

    // Particle death / respawn when it reaches the ground (Paper Section III.B)
    if (pos.z < groundZ) {
        // Respawn at random XY inside the spawn box, at the top Z
        // Seed with TexCoords + time so each particle lands somewhere different
        pos.x = mix(spawnMin.x, spawnMax.x, rand(TexCoords + vec2(time * 0.137, 0.314)));
        pos.y = mix(spawnMin.y, spawnMax.y, rand(TexCoords + vec2(0.718,        time * 0.271)));
        pos.z = spawnMax.z;
        vel   = vec3(0.0);
    }

    outPos = vec4(pos, 1.0);
    outVel = vec4(vel, 0.0);
}
