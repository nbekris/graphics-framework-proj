////////////////////////////////////////////////////////////////////////
// The scene class contains all the parameters needed to define and
// draw a simple scene, including:
//   * Geometry
//   * Light parameters
//   * Material properties
//   * Viewport size parameters
//   * Viewing transformation values
//   * others ...
//
// Some of these parameters are set when the scene is built, and
// others are set by the framework in response to user mouse/keyboard
// interactions.  All of them can be used to draw the scene.

#include "shapes.h"
#include "object.h"
#include "texture.h"
#include "fbo.h"

enum ObjectIds {
    nullId	= 0,
    skyId	= 1,
    seaId	= 2,
    groundId	= 3,
    roomId	= 4,
    boxId	= 5,
    frameId	= 6,
    lPicId	= 7,
    rPicId	= 8,
    teapotId	= 9,
    spheresId	= 10,
    floorId     = 11
};

class Shader;


class Scene
{
public:
    GLFWwindow* window;

    // @@ Declare interactive viewing variables here. (spin, tilt, ry, front back, ...)
    float spin, tilt, tx, ty, zoom, rx, ry, front, back;

	// eye position
    glm::vec3 eye;

    double time_since_last_refresh;
	double last_refresh_time;
    float step;
    float speed;

	bool transformation_mode;

    bool w_down;
    bool a_down;
    bool s_down;
    bool d_down;

    // Light parameters
    float lightSpin, lightTilt, lightDist;
    glm::vec3 lightPos;
    // @@ Perhaps declare additional scene lighting values here. (lightVal, lightAmb)

    int mode; // Extra mode indicator hooked up to number keys and sent to shader
    
    // Viewport
    int width, height;

    // Transformations
    glm::mat4 WorldProj, WorldView, WorldInverse;

    // All objects in the scene are children of this single root object.
    Object* objectRoot;
    Object* lightVolumeSphere;
    Object *central, *anim, *room, *floor, *teapot, *podium, *sky,
            *ground, *sea, *spheres, *leftFrame, *rightFrame;

	Shape* QuadPolygons;
    Object* fullScreenQuad;

    std::vector<Object*> animated;
    ProceduralGround* proceduralground;

    // Shader programs
    ShaderProgram* lightingProgram;
    // @@ Declare additional shaders if necessary
    ShaderProgram* shadowProgram;
	ShaderProgram* reflectionProgram;
	ShaderProgram* gBufferProgram;
    ShaderProgram* deferredLightProgram;
    ShaderProgram* localLightsProgram;
    ShaderProgram* computeBlurShader;

    // Blur parameters
    int blurWidth;
    int lastBlurWidth;
    GLuint blurUBO;
    float shadowLinstepLo;
    float shadowLinstepHi;

    float fps;

    // Spherical Harmonics coefficients (L=0..2, 9 vec3s)
    glm::vec3 shCoeffs[9];

    // IBL specular: Hammersley low-discrepancy sequence
    int numSamples = 30;
    GLuint hammersleyUBO = 0;
    int skyHDRWidth = 0, skyHDRHeight = 0;

    // std140-compatible struct: numSamples (int, padded to 16), then vec4[50] holding 2 pairs each
    struct HammersleyBlock {
        int numSamples;
        int pad[3];              // pad to 16 bytes
        float hammersley[50][4]; // vec4[50]: each holds (u0,v0, u1,v1)
    } hammersleyBlock;

    // Tone mapping
    float exposure = 2.0f;

    bool enablePointLights = true;
    bool enableDirectLight = true;
    bool enableOrbitSpheres = true;

    Object *orbitSpheres, *orbitAnim;

    // Options menu stuff
    bool show_demo_window;

    void InitializeScene();
    void BuildTransforms();
    void DrawMenu();
    void DrawScene();
    void CreateShader();
};
