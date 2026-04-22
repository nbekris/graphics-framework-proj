///////////////////////////////////////////////////////////////////////
// A slight encapsulation of a Frame Buffer Object (i'e' Render
// Target) and its associated texture.  When the FBO is "Bound", the
// output of the graphics pipeline is captured into the texture.  When
// it is "Unbound", the texture is available for use as any normal
// texture.
////////////////////////////////////////////////////////////////////////

class FBO {
public:
    unsigned int fboID;
    unsigned int textureID;
    int width, height;  // Size of the texture.

    // --- G-Buffer specific textures ---
    unsigned int gPosition;
    unsigned int gNormal;
    unsigned int gAlbedo;
    unsigned int gSpec;

    unsigned int gFragData[4];
    unsigned int gLightVec;
    unsigned int gEyeVec;

    unsigned int depthBuffer;

    void CreateFBO(const int w, const int h);
    // Bind this FBO to receive the output of the graphics pipeline.
    void BindFBO();
    // Unbind this FBO from the graphics pipeline;  graphics goes to screen by default.
    void UnbindFBOEXT();
    // Unbind FBO modern version
    void FBO::UnbindFBO();
    // Bind this FBO's texture to a texture unit.
    void BindTexture(const int unit, const int programId, const std::string& name);
    // Unbind this FBO's texture from a texture unit.
    void UnbindTexture(const int unit);

    // --- G-Buffer methods ---
    void CreateGBuffer(const int w, const int h);

    // --- Snow particle ping-pong FBO (2 RGBA32F color attachments) ---
    // gFragData[0] = positions, gFragData[1] = velocities
    void CreateDualFBO(const int w, const int h);

    // Helper to bind all 3 textures at once for the lighting pass
    void BindGBufferTextures(int startUnit, int programId,
        std::string fragDataName, std::string lightVecName, std::string eyeVecName);

    // Helper to unbind all 3 textures
    void UnbindGBufferTextures(int startUnit);
};
