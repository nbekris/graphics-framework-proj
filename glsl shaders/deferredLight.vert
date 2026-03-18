#version 330 core
layout (location = 0) in vec3 vertex;
layout (location = 2) in vec2 vertexTexture;

// Note: ModelTr is NOT used for the full-screen quad, only for shadow coordinate calculation
// if needed. For a standard deferred pass, ModelTr should be Identity.
uniform mat4 ModelTr, ShadowMatrix, WorldView, WorldProj;

out vec2 texCoord;
out vec4 shadowCoord;
out vec3 fragWorldPos;

void main()
{
    // For a full-screen quad in deferred lighting, the vertex coordinates 
    // are already in normalized device coordinates (-1 to 1)
    // Apply ModelTr if needed (usually Identity for full-screen quads)
    vec4 worldPos = ModelTr * vec4(vertex, 1.0);
    
    // Calculate shadow coordinate in light space
    shadowCoord = ShadowMatrix * worldPos;

    texCoord = vertexTexture;
    
    // Pass the world position for any per-pixel calculations
    fragWorldPos = worldPos.xyz;
    
    // Full-screen quad: position is already in NDC space
    gl_Position = vec4(vertex.x, vertex.y, 0.0, 1.0); 
}