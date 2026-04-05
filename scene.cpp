
////////////////////////////////////////////////////////////////////////
// The scene class contains all the parameters needed to define and
// draw a simple scene, including:
//   * Geometry
//   * Light parameters
//   * Material properties
//   * viewport size parameters
//   * Viewing transformation values
//   * others ...
//
// Some of these parameters are set when the scene is built, and
// others are set by the framework in response to user mouse/keyboard
// interactions.  All of them can be used to draw the scene.

#include "math.h"
#include <iostream>
#include <stdlib.h>
#include <algorithm>

#include <glbinding/gl/gl.h>
#include <glbinding/Binding.h>
using namespace gl;

#include <glu.h>                // For gluErrorString

#define GLM_FORCE_CTOR_INIT
#define GLM_FORCE_RADIANS
#define GLM_SWIZZLE
#include <glm/glm.hpp>
#include <glm/ext.hpp>          // For printing GLM objects with to_string

#include "framework.h"
#include "shapes.h"
#include "object.h"
#include "texture.h"
#include "transform.h"
#include "HDR.h"
#include "Texture.h"

const bool fullPolyCount = true; // Use false when emulating the graphics pipeline in software


const float PI = 3.14159f;
const float rad = PI/180.0f;    // Convert degrees to radians

const int numLights = 64;

glm::mat4 Identity(1.0);
glm::mat4 ShadowMatrix;
glm::mat4 ShadowView;
glm::mat4 ShadowProj;

FBO shadowFbo;
FBO reflectionTopFbo;
FBO reflectionBottomFbo;
FBO gBufferFbo;

// Many local light values
std::vector<glm::vec3> lightPositions;
std::vector<glm::vec3> lightColors;
std::vector<float> lightRanges;

// Compute shader for Gaussian blur
GLuint Bindpoint = 0;
GLuint scratchpadTextureID;
GLuint preBlurTextureID; // Debug: copy of shadow map before blur
GLuint SHADOW_WIDTH = 2048;
GLuint SHADOW_HEIGHT = 2048;


const float grndSize = 100.0;    // Island radius;  Minimum about 20;  Maximum 1000 or so
const float grndOctaves = 4.0;  // Number of levels of detail to compute
const float grndFreq = 0.03;    // Number of hills per (approx) 50m
const float grndPersistence = 0.03; // Terrain roughness: Slight:0.01  rough:0.05
const float grndLow = -3.0;         // Lowest extent below sea level
const float grndHigh = 5.0;        // Highest extent above sea level

const bool showSpheres = true;

////////////////////////////////////////////////////////////////////////
// This macro makes it easy to sprinkle checks for OpenGL errors
// throughout your code.  Most OpenGL calls can record errors, and a
// careful programmer will check the error status *often*, perhaps as
// often as after every OpenGL call.  At the very least, once per
// refresh will tell you if something is going wrong.
#define CHECKERROR {GLenum err = glGetError(); if (err != GL_NO_ERROR) { fprintf(stderr, "OpenGL error (at line scene.cpp:%d): %s\n", __LINE__, gluErrorString(err)); exit(-1);} }

// Create an RGB color from human friendly parameters: hue, saturation, value
glm::vec3 HSV2RGB(const float h, const float s, const float v)
{
    if (s == 0.0)
        return glm::vec3(v,v,v);

    int i = (int)(h*6.0) % 6;
    float f = (h*6.0f) - i;
    float p = v*(1.0f - s);
    float q = v*(1.0f - s*f);
    float t = v*(1.0f - s*(1.0f-f));
    if      (i == 0)     return glm::vec3(v,t,p);
    else if (i == 1)  return glm::vec3(q,v,p);
    else if (i == 2)  return glm::vec3(p,v,t);
    else if (i == 3)  return glm::vec3(p,q,v);
    else if (i == 4)  return glm::vec3(t,p,v);
    else   /*i == 5*/ return glm::vec3(v,p,q);
}

////////////////////////////////////////////////////////////////////////
// Constructs a hemisphere of spheres of varying hues
Object* SphereOfSpheres(Shape* SpherePolygons)
{
    Object* ob = new Object(NULL, nullId);
    
    for (float angle=0.0;  angle<360.0;  angle+= 18.0)
        for (float row=0.075;  row<PI/2.0;  row += PI/2.0/6.0) {   
            glm::vec3 hue = HSV2RGB(angle/360.0, 1.0f-2.0f*row/PI, 1.0f);

            Object* sp = new Object(SpherePolygons, spheresId,
                                    hue, glm::vec3(1.0, 1.0, 1.0), 120.0, false, NULL, NULL);
            float s = sin(row);
            float c = cos(row);
            ob->add(sp, Rotate(2,angle)*Translate(c,0,s)*Scale(0.075*c,0.075*c,0.075*c));
        }
    return ob;
}

Object* OrbitingSpheres(Shape* SpherePolygons)
{
    Object* ob = new Object(NULL, nullId);
    float radius = 3.0f;
    float sphereScale = 0.60f;

    float alphas[] = {1.0f, 5.0f, 20.0f, 50.0f, 200.0f, 2000.0f};
    int count = 6;

    for (int i = 0; i < count; i++) {
        float angle = 360.0f * i / count;
        glm::vec3 hue = HSV2RGB(float(i) / count, 0.8f, 1.0f);

        Object* sp = new Object(SpherePolygons, spheresId,
                                hue, glm::vec3(1.0, 1.0, 1.0), alphas[i], false, NULL, NULL);
        ob->add(sp, Rotate(2, angle) * Translate(radius, 0, 0)
                    * Scale(sphereScale, sphereScale, sphereScale));
    }
    return ob;
}

Object* CreateSphere(Shape* SpherePolygons) {
    glm::vec3 hue = glm::vec3(1.0f, 1.0f, 1.0f);

    //Object* ob = new Object(NULL, nullId);
    Object* sp = new Object(SpherePolygons, spheresId,
        hue, glm::vec3(1.0, 1.0, 1.0), 120.0, false, NULL, NULL);
    //Shape* sp = new Sphere(1);

    return sp;
}

////////////////////////////////////////////////////////////////////////
// Constructs a -1...+1  quad (canvas) framed by four (elongated) boxes
Object* FramedPicture(const glm::mat4& modelTr, const int objectId, 
                      Shape* BoxPolygons, Shape* QuadPolygons, Texture* Texture=NULL)
{
    // This draws the frame as four (elongated) boxes of size +-1.0
    float w = 0.05;             // Width of frame boards.
    
    Object* frame = new Object(NULL, nullId);
    Object* ob;
    
    glm::vec3 woodColor(87.0/255.0,51.0/255.0,35.0/255.0);
    ob = new Object(BoxPolygons, frameId,
            woodColor, glm::vec3(0.2, 0.2, 0.2), 10.0, false, NULL, NULL);


    frame->add(ob, Translate(0.0, 0.0, 1.0+w)*Scale(1.0, w, w));
    frame->add(ob, Translate(0.0, 0.0, -1.0-w)*Scale(1.0, w, w));
    frame->add(ob, Translate(1.0+w, 0.0, 0.0)*Scale(w, w, 1.0+2*w));
    frame->add(ob, Translate(-1.0-w, 0.0, 0.0)*Scale(w, w, 1.0+2*w));
    if (Texture) 
    {
        ob = new Object(QuadPolygons, objectId,
            woodColor, glm::vec3(0.0, 0.0, 0.0), 10.0, false, Texture, NULL);
    }
    else
    {
        ob = new Object(QuadPolygons, objectId,
            woodColor, glm::vec3(0.0, 0.0, 0.0), 10.0, false, NULL, NULL);
    }

    frame->add(ob, Rotate(0,90));

    return frame;
}

////////////////////////////////////////////////////////////////////////
// Project an equirectangular HDR image into L=0..2 spherical harmonics
// (9 RGB coefficients). The image must be RGBA float with 4 channels.
void ProjectSH(const float* image, int w, int h, glm::vec3 shCoeffs[9])
{
    // SH basis constants
    const float Y00  = 0.282095f;
    const float Y1m1 = 0.488603f;
    const float Y10  = 0.488603f;
    const float Y11  = 0.488603f;
    const float Y2m2 = 1.092548f;
    const float Y2m1 = 1.092548f;
    const float Y20  = 0.315392f;
    const float Y21  = 1.092548f;
    const float Y22  = 0.546274f;

    for (int i = 0; i < 9; i++)
        shCoeffs[i] = glm::vec3(0.0f);

    float weightSum = 0.0f;

    for (int y = 0; y < h; y++) {
        // theta = polar angle from top (0) to bottom (pi)
        float theta = PI * (float(y) + 0.5f) / float(h);
        float sinTheta = sin(theta);
        float cosTheta = cos(theta);

        for (int x = 0; x < w; x++) {
            // phi = azimuthal angle (0 to 2*pi)
            float phi = 2.0f * PI * (float(x) + 0.5f) / float(w);

            // Direction on the unit sphere
            float dx = sinTheta * cos(phi);
            float dy = sinTheta * sin(phi);
            float dz = cosTheta;

            // Solid angle weight for equirectangular projection
            float solidAngle = (2.0f * PI / float(w)) * (PI / float(h)) * sinTheta;
            weightSum += solidAngle;

            // Read pixel color (RGBA, stride = 4 floats)
            int idx = (y * w + x) * 4;
            glm::vec3 color(image[idx], image[idx + 1], image[idx + 2]);

            glm::vec3 weighted = color * solidAngle;

            // Accumulate into SH coefficients
            shCoeffs[0] += weighted * Y00;
            shCoeffs[1] += weighted * (Y1m1 * dy);
            shCoeffs[2] += weighted * (Y10  * dz);
            shCoeffs[3] += weighted * (Y11  * dx);
            shCoeffs[4] += weighted * (Y2m2 * dx * dy);
            shCoeffs[5] += weighted * (Y2m1 * dy * dz);
            shCoeffs[6] += weighted * (Y20  * (3.0f * dz * dz - 1.0f));
            shCoeffs[7] += weighted * (Y21  * dx * dz);
            shCoeffs[8] += weighted * (Y22  * (dx * dx - dy * dy));
        }
    }

    printf("SH projection complete: weightSum=%.4f (expect ~%.4f)\n", weightSum, 4.0f * PI);

    // Step 3: Multiply by clamped cosine lobe coefficients A^_l
    // A^_0 = pi,  A^_1 = 2pi/3,  A^_2 = pi/4
    float A0 = PI;
    float A1 = 2.0f * PI / 3.0f;
    float A2 = PI / 4.0f;

    shCoeffs[0] *= A0;           // band 0
    shCoeffs[1] *= A1;           // band 1
    shCoeffs[2] *= A1;
    shCoeffs[3] *= A1;
    shCoeffs[4] *= A2;           // band 2
    shCoeffs[5] *= A2;
    shCoeffs[6] *= A2;
    shCoeffs[7] *= A2;
    shCoeffs[8] *= A2;

    for (int i = 0; i < 9; i++)
        printf("  shCoeffs[%d] = (%.4f, %.4f, %.4f)\n", i, shCoeffs[i].x, shCoeffs[i].y, shCoeffs[i].z);
}

////////////////////////////////////////////////////////////////////////
// InitializeScene is called once during setup to create all the
// textures, shape VAOs, and shader programs as well as setting a
// number of other parameters.
void Scene::InitializeScene()
{
    glEnable(GL_DEPTH_TEST);
    CHECKERROR;

    // @@ Initialize interactive viewing variables here. (spin, tilt, ry, front back, ...)
    fps = 0.0f;
    spin = 0.0;
	tilt = 30.0;
    tx = 0.0;
    ty = 0.0;
    zoom = 25.0;
    ry = 0.4;
    front = 0.5;
    back = 5000.0;
    speed = 10.0;
	eye = glm::vec3(0.0, -20.0, 0.0);

	transformation_mode = false;
	last_refresh_time = 0.0;

	w_down = false;
	a_down = false;
	s_down = false;
	d_down = false;

    // Set initial light parameters
    lightSpin = 150.0;
    lightTilt = -45.0;
    lightDist = 100.0;
    // @@ Perhaps initialize additional scene lighting values here. (lightVal, lightAmb)

    // Set initial shadow/blur parameters
    blurWidth = 3;
    lastBlurWidth = -1;
    shadowLinstepLo = 0.000f;
    shadowLinstepHi = 0.010f;

    // Many local light values
    lightPositions.clear();
    lightColors.clear();
    lightRanges.clear();

    float radius = 12.0f;

    for (int i = 0; i < numLights; i++) {
        float angle = (float)i / (float)numLights * PI * 10.0f;

        float x = cos(angle) * radius;
        float z = sin(angle) * radius;
        float y = 0.09f * i;

		glm::vec3 pos = glm::vec3(x, z, y);

        lightPositions.push_back(pos);
        if (i % 3 == 0)      lightColors.push_back(glm::vec3(10.0f, 2.0f, 2.0f));
        else if (i % 3 == 1) lightColors.push_back(glm::vec3(2.0f, 10.0f, 2.0f));
        else                 lightColors.push_back(glm::vec3(2.0f, 2.0f, 10.0f));

        // Range: Medium size
        lightRanges.push_back(5.0f);
    }

    CHECKERROR;
    objectRoot = new Object(NULL, nullId);

	QuadPolygons = new Quad();
    fullScreenQuad = new Object(QuadPolygons, nullId,
        glm::vec3(0, 0, 0), glm::vec3(0, 0, 0), 0, false,
        NULL,
        NULL);

    shadowFbo.CreateFBO(SHADOW_WIDTH, SHADOW_HEIGHT);
	reflectionTopFbo.CreateFBO(1024, 1024);
	reflectionBottomFbo.CreateFBO(1024, 1024);
	gBufferFbo.CreateGBuffer(750, 750);
    
	// Create Deferred Rendering shader program
    gBufferProgram = new ShaderProgram();
	gBufferProgram->AddShader("gBuffer.vert", GL_VERTEX_SHADER);
	gBufferProgram->AddShader("gBuffer.frag", GL_FRAGMENT_SHADER);
	glBindAttribLocation(gBufferProgram->programId, 0, "vertex");
	glBindAttribLocation(gBufferProgram->programId, 1, "vertexNormal");
	glBindAttribLocation(gBufferProgram->programId, 2, "vertexTexture");
	glBindAttribLocation(gBufferProgram->programId, 3, "vertexTangent");
	gBufferProgram->LinkProgram();

	deferredLightProgram = new ShaderProgram();
	deferredLightProgram->AddShader("deferredLight.vert", GL_VERTEX_SHADER);
	deferredLightProgram->AddShader("deferredLight.frag", GL_FRAGMENT_SHADER);
	glBindAttribLocation(deferredLightProgram->programId, 0, "vertex");
	glBindAttribLocation(deferredLightProgram->programId, 2, "vertexTexture");
	deferredLightProgram->LinkProgram();

    {
        GLuint loc = glGetUniformBlockIndex(deferredLightProgram->programId, "HammersleyBlock");
        if (loc != GL_INVALID_INDEX)
            glUniformBlockBinding(deferredLightProgram->programId, loc, 1);
        else
            printf("WARNING: HammersleyBlock not found in deferredLightProgram\n");
    }

    localLightsProgram = new ShaderProgram();
    localLightsProgram->AddShader("localLights.vert", GL_VERTEX_SHADER);
    localLightsProgram->AddShader("localLights.frag", GL_FRAGMENT_SHADER);
    glBindAttribLocation(localLightsProgram->programId, 0, "vertex");
    localLightsProgram->LinkProgram();

	// Reflection Shader Program (dual-paraboloid, forward lighting)
	// reflection.vert does paraboloid projection + calls LightingVertex() from lighting.vert
	// reflection.frag is self-contained with lighting matching deferredLight.frag
	reflectionProgram = new ShaderProgram();
	reflectionProgram->AddShader("reflection.vert", GL_VERTEX_SHADER);
	reflectionProgram->AddShader("lighting.vert", GL_VERTEX_SHADER);
	reflectionProgram->AddShader("reflection.frag", GL_FRAGMENT_SHADER);
	glBindAttribLocation(reflectionProgram->programId, 0, "vertex");
	glBindAttribLocation(reflectionProgram->programId, 1, "vertexNormal");
	glBindAttribLocation(reflectionProgram->programId, 2, "vertexTexture");
	glBindAttribLocation(reflectionProgram->programId, 3, "vertexTangent");
	reflectionProgram->LinkProgram();

	// Shadow Map Shader Program Initialization
    shadowProgram = new ShaderProgram();
    shadowProgram->AddShader("shadow.frag", GL_FRAGMENT_SHADER);
    shadowProgram->AddShader("shadow.vert", GL_VERTEX_SHADER);
    glBindAttribLocation(shadowProgram->programId, 0, "vertex");
    glBindAttribLocation(shadowProgram->programId, 1, "vertexNormal");
    glBindAttribLocation(shadowProgram->programId, 2, "vertexTexture");
    glBindAttribLocation(shadowProgram->programId, 3, "vertexTangent");
    shadowProgram->LinkProgram();

    // Compute Blur Shader Program
    computeBlurShader = new ShaderProgram();
    computeBlurShader->AddShader("blur.comp", GL_COMPUTE_SHADER);
    computeBlurShader->LinkProgram();

    // Scratchpad texture for ping-pong blur (same size/format as shadow FBO)
    glGenTextures(1, &scratchpadTextureID);
    glBindTexture(GL_TEXTURE_2D, scratchpadTextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, (int)GL_RGBA32F, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (int)GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (int)GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (int)GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (int)GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Debug: pre-blur copy of shadow map (same size/format)
    glGenTextures(1, &preBlurTextureID);
    glBindTexture(GL_TEXTURE_2D, preBlurTextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, (int)GL_RGBA32F, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (int)GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (int)GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (int)GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (int)GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Blur kernel UBO: allocate full 101-element buffer for dynamic updates
    glGenBuffers(1, &blurUBO);
    Bindpoint = 0;

    // Initialize with default blurWidth=1 weights
    std::vector<float> paddedWeights(101 * 4, 0.0f);
    paddedWeights[0 * 4] = 0.25f;
    paddedWeights[1 * 4] = 0.5f;
    paddedWeights[2 * 4] = 0.25f;

    glBindBuffer(GL_UNIFORM_BUFFER, blurUBO);
    glBufferData(GL_UNIFORM_BUFFER, paddedWeights.size() * sizeof(float), paddedWeights.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, Bindpoint, blurUBO);

    // Create all the Polygon shapes
    proceduralground = new ProceduralGround(grndSize, 400,
                                     grndOctaves, grndFreq, grndPersistence,
                                     grndLow, grndHigh);
    
    Shape* TeapotPolygons =  new Teapot(fullPolyCount?12:2);
    Shape* BoxPolygons = new Box();
    Shape* SpherePolygons = new Sphere(32);
    Shape* LightSpherePolygons = new Sphere(32);
    Shape* RoomPolygons = new Ply("room.ply");
    Shape* FloorPolygons = new Plane(10.0, 10);
    Shape* QuadPolygons = new Quad();
    Shape* SeaPolygons = new Plane(2000.0, 50);
    Shape* GroundPolygons = proceduralground;

    // Various colors used in the subsequent models
    glm::vec3 woodColor(87.0/255.0, 51.0/255.0, 35.0/255.0);
    glm::vec3 brickColor(134.0/255.0, 60.0/255.0, 56.0/255.0);
    glm::vec3 floorColor(6*16/255.0, 5.5*16/255.0, 3*16/255.0);
    glm::vec3 brassColor(0.5, 0.5, 0.1);
    glm::vec3 grassColor(62.0/255.0, 102.0/255.0, 38.0/255.0);
    glm::vec3 waterColor(0.3, 0.3, 1.0);

    // Ks values in a range appropriate range for BRDF calculations. (Phong needs 10* this.)
    glm::vec3 noSpec(0.0, 0.0, 0.0);
    glm::vec3 brightSpec(0.03, 0.03, 0.03);
 
    // Creates all the models from which the scene is composed.  Each
    // is created with a polygon shape (possibly NULL), a
    // transformation, and the surface lighting parameters Kd, Ks, and
    // alpha.

    // @@ This is where you could read in all the textures and
    // associate them with the various objects being created in the
    // next dozen lines of code.

    Texture* roomTexture = new Texture("textures/Standard_red_pxr128.png");
    Texture* floorTexture = new Texture("textures/6670-diffuse.jpg");
    Texture* teapotTexture = new Texture("textures/cracks.png");
    Texture* podiumTexture = new Texture("textures/Brazilian_rosewood_pxr128.png");
    Texture* groundTexture = new Texture("textures/grass.jpg");
    Texture* rightFrameTexture = new Texture("textures/my-house-01.png");
    Texture* skyTexture = new Texture("skys/Ocean.png");

    HDR* skyHDR = new HDR("skys/Road_to_MonumentValley_Ref.hdr", true);
    ProjectSH(skyHDR->image, skyHDR->width, skyHDR->height, shCoeffs);
    skyHDRWidth  = skyHDR->width;
    skyHDRHeight = skyHDR->height;
    skyHDR->FreePixels();

    // Build Hammersley low-discrepancy sequence for IBL specular
    {
        memset(&hammersleyBlock, 0, sizeof(hammersleyBlock));
        hammersleyBlock.numSamples = numSamples;

        int kk;
        float p, u;
        for (int k = 0; k < numSamples; k++) {
            u = 0.0f;
            for (p = 0.5f, kk = k; kk; p *= 0.5f, kk >>= 1)
                if (kk & 1) u += p;
            float v = (k + 0.5f) / numSamples;
            // Pack 2 pairs per vec4: k=0,1 -> vec4[0], k=2,3 -> vec4[1], etc.
            int vec4Idx = k / 2;
            int offset  = (k % 2) * 2; // 0 or 2
            hammersleyBlock.hammersley[vec4Idx][offset]     = u;
            hammersleyBlock.hammersley[vec4Idx][offset + 1] = v;
        }

        glGenBuffers(1, &hammersleyUBO);
        glBindBufferBase(GL_UNIFORM_BUFFER, 1, hammersleyUBO);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(hammersleyBlock), &hammersleyBlock, GL_STATIC_DRAW);
    }

    Texture* roomNormal = new Texture("textures/Standard_red_pxr128_normal.png");
    Texture* seaNormal = new Texture("textures/ripples_normalmap.png");
    Texture* podiumNormal = new Texture("textures/Brazilian_rosewood_pxr128_normal.png");
    Texture* floorNormal = new Texture("textures/6670-normal.jpg");
    // @@ To change an object's surface parameters (Kd, Ks, or alpha),
    // modify the following lines.
    
    central    = new Object(NULL, nullId);
    anim       = new Object(NULL, nullId);
    room       = new Object(RoomPolygons, roomId, brickColor, noSpec, 2, false, roomTexture, roomNormal);
    floor      = new Object(FloorPolygons, floorId, floorColor, noSpec, 2, false, floorTexture, floorNormal);
    teapot     = new Object(TeapotPolygons, teapotId, brassColor, brightSpec, 100, true, teapotTexture);
    podium     = new Object(BoxPolygons, boxId, glm::vec3(woodColor), brightSpec, 5, false, podiumTexture, podiumNormal);
    sky        = new Object(SpherePolygons, skyId, noSpec, noSpec, 0, false, skyHDR);
    ground     = new Object(GroundPolygons, groundId, grassColor, noSpec, 3, false, groundTexture);
    sea        = new Object(SeaPolygons, seaId, waterColor, brightSpec, 100, false, skyTexture, seaNormal);
    leftFrame  = FramedPicture(Identity, lPicId, BoxPolygons, QuadPolygons);
    rightFrame = FramedPicture(Identity, rPicId, BoxPolygons, QuadPolygons, rightFrameTexture); 
    spheres    = SphereOfSpheres(SpherePolygons);
    orbitSpheres = OrbitingSpheres(SpherePolygons);
    orbitAnim    = new Object(NULL, nullId);

    // Deferred rendering light sphere mesh
    lightVolumeSphere = CreateSphere(SpherePolygons);
#ifdef REFL
    spheres->drawMe = true;
#else
    spheres->drawMe = false;
#endif


    // @@ To change the scene hierarchy, examine the hierarchy created
    // by the following object->add() calls and adjust as you wish.
    // The objects being manipulated and their polygon shapes are
    // created above here.

    // Scene is composed of sky, ground, sea, room and some central models
    if (fullPolyCount) {
        objectRoot->add(sky, Scale(2000.0, 2000.0, 2000.0)); //check scale, but this probably fine
        objectRoot->add(sea); 
        objectRoot->add(ground); 

        // Deferred Rendering
        objectRoot->add(lightVolumeSphere);
    }
    objectRoot->add(central);
#ifndef REFL
    objectRoot->add(room,  Translate(0.0, 0.0, 0.02));
#endif
    objectRoot->add(floor, Translate(0.0, 0.0, 0.02));

    // Central model has a rudimentary animation (constant rotation on Z)
    animated.push_back(anim);

    // Central contains a teapot on a podium and an external sphere of spheres
    central->add(podium, Translate(0.0, 0,0));
    central->add(anim, Translate(0.0, 0,0));
    anim->add(teapot, Translate(0,0,1)*Scale(0.31,0.31,0.31));

    if (fullPolyCount)
        anim->add(spheres, Translate(0.0, 0.0, 0.0)*Scale(16, 16, 16));

    // Orbiting spheres around the teapot
    central->add(orbitAnim, Translate(0.0, 0.0, 1.0));
    orbitAnim->add(orbitSpheres);
    
    // Room contains two framed pictures
    if (fullPolyCount) {
        room->add(leftFrame, Translate(-1.5, 9.85, 1.)*Scale(0.8, 0.8, 0.8));
        room->add(rightFrame, Translate( 1.5, 9.85, 1.)*Scale(0.8, 0.8, 0.8)); }

    CHECKERROR;

    // Options menu stuff
    show_demo_window = false;
}

void Scene::DrawMenu()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (ImGui::BeginMainMenuBar()) {
        // This menu demonstrates how to provide the user a list of toggleable settings.
        if (ImGui::BeginMenu("Objects")) {
            if (ImGui::MenuItem("Draw spheres", "", spheres->drawMe))  {spheres->drawMe ^= true; }
            if (ImGui::MenuItem("Draw walls", "", room->drawMe))       {room->drawMe ^= true; }
            if (ImGui::MenuItem("Draw ground", "", ground->drawMe))     {ground->drawMe ^= true; }
            if (ImGui::MenuItem("Draw sea", "", sea->drawMe))           {sea->drawMe ^= true; }
            if (ImGui::MenuItem("Draw orbit spheres", "", orbitSpheres->drawMe)) {orbitSpheres->drawMe ^= true; }
            ImGui::EndMenu(); }
                	
        // This menu demonstrates how to provide the user a choice
        // among a set of choices.  The current choice is stored in a
        // variable named "mode" in the application, and sent to the
        // shader to be used as you wish.
        if (ImGui::BeginMenu("Menu ")) {
            if (ImGui::MenuItem("<debug views>", "",	false, false)) {}
            if (ImGui::MenuItem("Normal Rendering", "",		mode==0)) { mode=0; }
            if (ImGui::MenuItem("Shadow Map (G_shadow)", "",		mode==1)) { mode=1; }
            if (ImGui::MenuItem("Shadow Moments (z)", "",		mode==2)) { mode=2; }
            if (ImGui::MenuItem("Shadow Moments (z^2)", "",		mode==3)) { mode=3; }
            if (ImGui::MenuItem("Shadow Moments (z^3)", "",		mode==4)) { mode=4; }
            if (ImGui::MenuItem("Shadow Moments (z^4)", "",		mode==5)) { mode=5; }
            if (ImGui::MenuItem("Pre-Blur Moments (z)", "",		mode==6)) { mode=6; }
            if (ImGui::MenuItem("Blur Comparison (split)", "",		mode==7)) { mode=7; }
            if (ImGui::MenuItem("SH Irradiance (surfaces)", "",		mode==8)) { mode=8; }
            if (ImGui::MenuItem("SH Irradiance (sky sphere)", "",	mode==9)) { mode=9; }
            if (ImGui::MenuItem("<HDR views>", "",	false, false)) {}
            if (ImGui::MenuItem("HDR Skybox", "",			mode==10)) { mode=10; }
            if (ImGui::MenuItem("Irradiance Map (SH)", "",		mode==11)) { mode=11; }
            ImGui::EndMenu(); }
        
        ImGui::SameLine(ImGui::GetWindowWidth() - 100);
        ImGui::Text("FPS: %.1f", fps);
        ImGui::EndMainMenuBar(); }

    ImGui::SetNextWindowPos(ImVec2(10, 30), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(300, 160), ImGuiCond_Once);
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_Once);
    ImGui::Begin("Settings");
    ImGui::SliderFloat("Exposure", &exposure, 0.1f, 10000.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderInt("Blur Radius", &blurWidth, 0, 50);
    ImGui::SliderFloat("Linstep Lo", &shadowLinstepLo, 0.0f, 0.5f, "%.3f");
    ImGui::SliderFloat("Linstep Hi", &shadowLinstepHi, 0.01f, 1.0f, "%.3f");
    // Ensure Lo < Hi to prevent shadow inversion
    if (shadowLinstepLo >= shadowLinstepHi)
        shadowLinstepLo = shadowLinstepHi - 0.01f;
    ImGui::Checkbox("Point Lights", &enablePointLights);
    ImGui::Checkbox("Direct Light", &enableDirectLight);
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Scene::BuildTransforms()
{
    // @@ When you are ready to try interactive viewing, replace the
    // following hard coded values for WorldProj and WorldView with
    // transformation matrices calculated from variables such as spin,
    // tilt, tr, ry, front, and back.
    //WorldProj[0][0]=  2.368;
    //WorldProj[1][0]= -0.800;
    //WorldProj[2][0]=  0.000;
    //WorldProj[3][0]=  0.000;
    //WorldProj[0][1]=  0.384;
    //WorldProj[1][1]=  1.136;
    //WorldProj[2][1]=  2.194;
    //WorldProj[3][1]=  0.000;
    //WorldProj[0][2]=  0.281;
    //WorldProj[1][2]=  0.831;
    //WorldProj[2][2]= -0.480;
    //WorldProj[3][2]= 42.451;
    //WorldProj[0][3]=  0.281;
    //WorldProj[1][3]=  0.831;
    //WorldProj[2][3]= -0.480;
    //WorldProj[3][3]= 43.442;
    //
    //WorldView[3][0]= 0.0;
    //WorldView[3][1]= 0.0;
    //WorldView[3][2]= 0.0;

    if (transformation_mode)
    {
        WorldProj = Perspective(rx, ry, front, back);
        WorldView = Rotate(6, tilt - 90) * Rotate(2, spin) *
            Translate(-eye.x, -eye.y, -eye.z);
    }
    else
    {
        WorldProj = Perspective(rx, ry, front, back);
        WorldView = Translate(tx, ty, -zoom) * Rotate(6, tilt - 90) * Rotate(2, spin);
    }

    // @@ Print the two matrices (in column-major order) for
    // comparison with the project document.
    //std::cout << "WorldView: " << glm::to_string(WorldView) << std::endl;
    //std::cout << "WorldProj: " << glm::to_string(WorldProj) << std::endl;
}

////////////////////////////////////////////////////////////////////////
// Procedure DrawScene is called whenever the scene needs to be
// drawn. (Which is often: 30 to 60 times per second are the common
// goals.)
void Scene::DrawScene()
{
    // Set the viewport
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

	// recalculate eye position
    // Calculate time step
    double current_time = glfwGetTime();

    time_since_last_refresh = current_time - last_refresh_time;
    step = speed * time_since_last_refresh;
    last_refresh_time = current_time;

    if (time_since_last_refresh > 0.0)
        fps = fps + 0.03f * (float(1.0 / time_since_last_refresh) - fps);

	// Update eye position based on keys pressed
    if (w_down) 
    {
        eye += step * glm::vec3(sin(spin * rad), cos(spin * rad), 0.0);
    }
    else if (s_down) 
    {
        eye -= step * glm::vec3(sin(spin * rad), cos(spin * rad), 0.0);
	}
    else if (a_down)
    {
        eye -= step * glm::vec3(cos(spin * rad), -sin(spin * rad), 0.0);
    }
    else if (d_down) 
	{
        eye += step * glm::vec3(cos(spin * rad), -sin(spin * rad), 0.0);
	}

	eye.z = proceduralground->HeightAt(eye.x, eye.y) + 2.0;

    CHECKERROR;
    // Calculate the light's position from lightSpin, lightTilt, lightDist
    lightPos = glm::vec3(lightDist*cos(lightSpin*rad)*sin(lightTilt*rad),
                         lightDist*sin(lightSpin*rad)*sin(lightTilt*rad), 
                         lightDist*cos(lightTilt*rad));

    // Update position of any continuously animating objects
    double atime = 360.0*glfwGetTime()/36;
    for (std::vector<Object*>::iterator m=animated.begin();  m<animated.end();  m++)
        (*m)->animTr = Rotate(2, atime);

    // Orbit spheres rotation (around Z axis, faster than teapot)
    double orbitTime = 360.0*glfwGetTime()/12;
    orbitAnim->animTr = Rotate(2, orbitTime);

    rx = ry * width / height;
    BuildTransforms();

    // The lighting algorithm needs the inverse of the WorldView matrix
    WorldInverse = glm::inverse(WorldView);

    CreateShader();
}

void Scene::CreateShader()
{
    ////////////////////////////////////////////////////////////////////////////////
    // Anatomy of a pass:
    //   Choose a shader  (create the shader in InitializeScene above)
    //   Choose and FBO/Render-Target (if needed; create the FBO in InitializeScene above)
    //   Set the viewport (to the pixel size of the screen or FBO)
    //   Clear the screen.
    //   Set the uniform variables required by the shader
    //   Draw the geometry
    //   Unset the FBO (if one was used)
    //   Unset the shader
    ////////////////////////////////////////////////////////////////////////////////

    CHECKERROR;
    int loc, programId;

    // Relative depth bounds for MSM (used in shadow and lighting passes)
    // Tight bounds around the scene as seen from the light.
    // lightDist is the distance from the light to the origin;
    // the scene roughly spans [lightDist - sceneRadius, lightDist + sceneRadius].
    float sceneRadius = 30.0f; // approximate bounding radius of the scene geometry
    float shadowZ0 = std::max(lightDist - sceneRadius, 0.1f);
    float shadowZ1 = lightDist + sceneRadius;

    // Set Light
   glm::vec3 Light(3, 3, 3);
   glm::vec3 Ambient(0.4, 0.4, 0.4);

   eye = glm::vec3(WorldInverse * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

   // -----------------------------------------------------------------
    // Shadow Pass - Generate moment shadow map
    // Stores (z, z^2, z^3, z^4) with relative depth
    // -----------------------------------------------------------------

   shadowProgram->UseShader();

   programId = shadowProgram->programId;
   shadowFbo.BindFBO();

   ShadowView = LookAt(lightPos, glm::vec3(0.0, 0.0, 0.0), glm::vec3(0.0, 0.0, 1.0));
   ShadowProj = Perspective(40 / lightDist, 40 / lightDist, front, back);

   glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
   // Clear to (1,1,1,1) = relative depth of 1.0 (far plane, no occluder)
   glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   loc = glGetUniformLocation(programId, "ProjectionMatrix");
   glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(ShadowProj));
   loc = glGetUniformLocation(programId, "ViewMatrix");
   glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(ShadowView));
   loc = glGetUniformLocation(programId, "z0");
   glUniform1f(loc, shadowZ0);
   loc = glGetUniformLocation(programId, "z1");
   glUniform1f(loc, shadowZ1);

   CHECKERROR;
   glEnable(GL_CULL_FACE);
   glCullFace(GL_FRONT);
   objectRoot->Draw(shadowProgram, Identity);
   glDisable(GL_CULL_FACE);
   CHECKERROR;

   shadowFbo.UnbindFBO();
   shadowProgram->UnuseShader();

   // Debug: save a copy of the unblurred shadow map
   glCopyImageSubData(shadowFbo.textureID, GL_TEXTURE_2D, 0, 0, 0, 0,
                      preBlurTextureID,     GL_TEXTURE_2D, 0, 0, 0, 0,
                      SHADOW_WIDTH, SHADOW_HEIGHT, 1);

   // -----------------------------------------------------------------
   // Compute Shader Pass - Gaussian blur on moment shadow map
   // -----------------------------------------------------------------

   computeBlurShader->UseShader();
   programId = computeBlurShader->programId;

   // Recompute Gaussian weights only when blurWidth changes
   if (blurWidth != lastBlurWidth) {
       std::vector<float> paddedWeights(101 * 4, 0.0f);
       if (blurWidth == 0) {
           paddedWeights[0] = 1.0f;
       } else {
           float sigma = std::max(blurWidth / 2.0f, 0.5f);
           float sum = 0.0f;
           for (int i = -blurWidth; i <= blurWidth; i++) {
               float w = exp(-0.5f * (i * i) / (sigma * sigma));
               sum += w;
               paddedWeights[(i + blurWidth) * 4] = w;
           }
           for (int i = 0; i <= 2 * blurWidth; i++)
               paddedWeights[i * 4] /= sum;
       }
       glBindBuffer(GL_UNIFORM_BUFFER, blurUBO);
       glBufferSubData(GL_UNIFORM_BUFFER, 0, paddedWeights.size() * sizeof(float), paddedWeights.data());
       lastBlurWidth = blurWidth;
   }

   // Bind UBO
   loc = glGetUniformBlockIndex(programId, "blurKernel");
   glUniformBlockBinding(programId, loc, Bindpoint);

   glUniform1i(glGetUniformLocation(programId, "blurWidth"), blurWidth);

   // --- Horizontal blur: shadow FBO -> scratchpad ---
   glBindImageTexture(0, shadowFbo.textureID, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
   glUniform1i(glGetUniformLocation(programId, "src"), 0);
   glBindImageTexture(1, scratchpadTextureID, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
   glUniform1i(glGetUniformLocation(programId, "dst"), 1);
   glUniform2i(glGetUniformLocation(programId, "blurDirection"), 1, 0);

   glDispatchCompute((SHADOW_WIDTH + 127) / 128, SHADOW_HEIGHT, 1);
   glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

   // --- Vertical blur: scratchpad -> shadow FBO ---
   glBindImageTexture(0, scratchpadTextureID, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
   glUniform1i(glGetUniformLocation(programId, "src"), 0);
   glBindImageTexture(1, shadowFbo.textureID, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
   glUniform1i(glGetUniformLocation(programId, "dst"), 1);
   glUniform2i(glGetUniformLocation(programId, "blurDirection"), 0, 1);

   glDispatchCompute((SHADOW_WIDTH + 127) / 128, SHADOW_HEIGHT, 1);
   glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

   computeBlurShader->UnuseShader();

    // -----------------------------------------------------------------
    // Reflection Pass - Render scene into dual-paraboloid maps
    // Two passes: top hemisphere (reflectDir=+1) and bottom (reflectDir=-1)
    // -----------------------------------------------------------------

    // Shadow matrix with bias (used by reflection and deferred passes)
    const glm::mat4 B = Translate(0.5f, 0.5f, 0.5f) * Scale(0.5f, 0.5f, 0.5f);
    ShadowMatrix = B * ShadowProj * ShadowView;

    reflectionProgram->UseShader();
    programId = reflectionProgram->programId;

    // ShadowMatrix needed by lighting.vert for shadowCoord varying
    loc = glGetUniformLocation(programId, "ShadowMatrix");
    glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(ShadowMatrix));

    // Uniforms needed by reflection.frag
    loc = glGetUniformLocation(programId, "lightPos");
    glUniform3fv(loc, 1, &lightPos[0]);
    loc = glGetUniformLocation(programId, "Light");
    glUniform3fv(loc, 1, &Light[0]);
    loc = glGetUniformLocation(programId, "Ambient");
    glUniform3fv(loc, 1, &Ambient[0]);
    loc = glGetUniformLocation(programId, "shCoeffs");
    glUniform3fv(loc, 9, &shCoeffs[0][0]);

    sky->texture->BindTexture(10, programId, "skyboxMap");

    loc = glGetUniformLocation(programId, "exposure");
    glUniform1f(loc, exposure);

    // Reflection eye position (centered above the scene)
    glm::vec3 reflectEye(0.0f, 0.0f, 1.5f);
    loc = glGetUniformLocation(programId, "Eye");
    glUniform3fv(loc, 1, &reflectEye[0]);

    // Top hemisphere
    reflectionTopFbo.BindFBO();
    glViewport(0, 0, reflectionTopFbo.width, reflectionTopFbo.height);
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    loc = glGetUniformLocation(programId, "ReflectDir");
    glUniform1f(loc, 1.0f);

    teapot->drawMe = false;  // Don't draw reflective objects in their own reflection
    objectRoot->Draw(reflectionProgram, Identity);
    reflectionTopFbo.UnbindFBO();

    // Bottom hemisphere
    reflectionBottomFbo.BindFBO();
    glViewport(0, 0, reflectionBottomFbo.width, reflectionBottomFbo.height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    loc = glGetUniformLocation(programId, "ReflectDir");
    glUniform1f(loc, -1.0f);

    objectRoot->Draw(reflectionProgram, Identity);
    reflectionBottomFbo.UnbindFBO();

    teapot->drawMe = true;  // Restore for G-buffer pass

    sky->texture->UnbindTexture(10);
    reflectionProgram->UnuseShader();

    // -----------------------------------------------------------------
    // G Buffer Pass
    // Render all scene objects into the G-Buffer textures
    // -----------------------------------------------------------------

    gBufferFbo.BindFBO();

    // Clear Color and Depth of the G-Buffer
    // We clear to black (0,0,0) so empty space has no position/normal data
    glViewport(0, 0, 750, 750); // We might need to clear again
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    gBufferProgram->UseShader();

    // Set View/Projection Matrices (Used by all objects)
    loc = glGetUniformLocation(gBufferProgram->programId, "WorldView");
    glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(WorldView));

    loc = glGetUniformLocation(gBufferProgram->programId, "WorldProj");
    glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(WorldProj));

    // Set Light Position
    loc = glGetUniformLocation(gBufferProgram->programId, "lightPos");
    glUniform3fv(loc, 1, &lightPos[0]); // Ensure this variable exists in your class!

    // Set View Position (Needed for Specular)
    loc = glGetUniformLocation(gBufferProgram->programId, "eye");
    glUniform3fv(loc, 1, &eye[0]);

    loc = glGetUniformLocation(gBufferProgram->programId, "viewPos");
    glUniform3fv(loc, 1, &eye[0]);

    // Draw the entire scene hierarchy
    // Note: The 'Draw' method in your Object class sets the Model matrix
    CHECKERROR;
    objectRoot->Draw(gBufferProgram, Identity);
    CHECKERROR;

    gBufferProgram->UnuseShader();
    gBufferFbo.UnbindFBO();


    // -----------------------------------------------------------------
    // Lighting Pass
    // Render a full-screen quad and calculate lighting per-pixel
    // -----------------------------------------------------------------

    // Revert to default framebuffer (the screen)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Clear the screen (optional, but good practice)
    glViewport(0, 0, 750, 750);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    deferredLightProgram->UseShader();
	programId = deferredLightProgram->programId;

    // Bind G-Buffer (We only need to do this ONCE for all lights)
    gBufferFbo.BindGBufferTextures(2, programId,
        "gFragData", "gLightVec", "gEyeVec");

    shadowFbo.BindTexture(8, programId, "shadowMap");

    sky->texture->BindTexture(10, programId, "skyboxMap");
    reflectionTopFbo.BindTexture(12, programId, "reflectionTop");
    reflectionBottomFbo.BindTexture(13, programId, "reflectionBottom");

    // Upload SH coefficients for diffuse IBL
    loc = glGetUniformLocation(programId, "shCoeffs");
    glUniform3fv(loc, 9, &shCoeffs[0][0]);

    // Bind pre-blur shadow map for debug visualization
    glActiveTexture(GL_TEXTURE9);
    glBindTexture(GL_TEXTURE_2D, preBlurTextureID);
    loc = glGetUniformLocation(programId, "preBlurShadowMap");
    glUniform1i(loc, 9);

    // ShadowMatrix already computed before reflection pass
    loc = glGetUniformLocation(programId, "ShadowMatrix");
    glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(ShadowMatrix));

    // Pass relative depth bounds (must match shadow pass)
    loc = glGetUniformLocation(programId, "z0");
    glUniform1f(loc, shadowZ0);
    loc = glGetUniformLocation(programId, "z1");
    glUniform1f(loc, shadowZ1);

    loc = glGetUniformLocation(programId, "linstepLo");
    glUniform1f(loc, shadowLinstepLo);
    loc = glGetUniformLocation(programId, "linstepHi");
    glUniform1f(loc, shadowLinstepHi);

    loc = glGetUniformLocation(programId, "Ambient");
    glUniform3fv(loc, 1, &(Ambient[0]));

    loc = glGetUniformLocation(programId, "viewMode");
    glUniform1i(loc, mode);

    loc = glGetUniformLocation(programId, "lightPos");
    glUniform3fv(loc, 1, &lightPos[0]);

    loc = glGetUniformLocation(programId, "lightColor");
    glUniform3fv(loc, 1, &Light[0]);

    loc = glGetUniformLocation(programId, "viewPos");
    glUniform3fv(loc, 1, &eye[0]);

    loc = glGetUniformLocation(programId, "width");
    glUniform1f(loc, width);

    loc = glGetUniformLocation(programId, "height");
    glUniform1f(loc, height);

    loc = glGetUniformLocation(programId, "sceneLightPos");
    glUniform3fv(loc, 1, &lightPos[0]);

    loc = glGetUniformLocation(programId, "sceneEye");
    glUniform3fv(loc, 1, &eye[0]);

    // Sky HDR dimensions for mipmap level calculation
    loc = glGetUniformLocation(programId, "skyWidth");
    glUniform1f(loc, (float)skyHDRWidth);
    loc = glGetUniformLocation(programId, "skyHeight");
    glUniform1f(loc, (float)skyHDRHeight);

    loc = glGetUniformLocation(programId, "exposure");
    glUniform1f(loc, exposure);

    loc = glGetUniformLocation(programId, "enableDirectLight");
    glUniform1i(loc, enableDirectLight ? 1 : 0);

    CHECKERROR;
    fullScreenQuad->Draw(deferredLightProgram, Identity);
    CHECKERROR;

    gBufferFbo.UnbindGBufferTextures(2);
	shadowFbo.UnbindTexture(8);
	sky->texture->UnbindTexture(10);
	reflectionTopFbo.UnbindTexture(12);
	reflectionBottomFbo.UnbindTexture(13);

    glActiveTexture(GL_TEXTURE9);
    glBindTexture(GL_TEXTURE_2D, 0);
    deferredLightProgram->UnuseShader();

    // -----------------------------------------------------------------
    // Local Lights Pass
    // Render many local lights
    // -----------------------------------------------------------------

    if (enablePointLights)
    {
        glBlendFunc(GL_ONE, GL_ONE);
        glEnable(GL_BLEND);

        glCullFace(GL_FRONT);
        glEnable(GL_CULL_FACE);

        glDisable(GL_DEPTH_TEST);

        localLightsProgram->UseShader();

        programId = localLightsProgram->programId;

        const GLint lightPosLoc = glGetUniformLocation(programId, "lightPos");
        const GLint colorLoc = glGetUniformLocation(programId, "lightColor");
        const GLint rangeLoc = glGetUniformLocation(programId, "lightRadius");

        loc = glGetUniformLocation(programId, "WorldView");
        glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(WorldView));

        loc = glGetUniformLocation(programId, "WorldProj");
        glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(WorldProj));

        loc = glGetUniformLocation(programId, "width");
        glUniform1f(loc, width);

        loc = glGetUniformLocation(programId, "height");
        glUniform1f(loc, height);

        // Set View Position (Needed for Specular)
        loc = glGetUniformLocation(programId, "eye");
        glUniform3fv(loc, 1, &eye[0]);

        //// Bind G-Buffer (We only need to do this ONCE for all lights)
        gBufferFbo.BindGBufferTextures(2, programId,
            "gFragData", "gLightVec", "gEyeVec");

        for (int i = 0; i < numLights; ++i) {
            const glm::vec3 position = lightPositions[i];
            const glm::vec3 color = lightColors[i];

            const float range = lightRanges[i];

            // Calculate the model matrix
            glm::mat4 model = Translate(position.x, position.y, position.z) * Scale(range, range, range);

            glUniform3fv(colorLoc, 1, &color[0]);
            glUniform3fv(lightPosLoc, 1, &position[0]);
            glUniform1fv(rangeLoc, 1, &range);

            CHECKERROR;
            lightVolumeSphere->Draw(localLightsProgram, model);
            CHECKERROR;
        }

        // Clean up
        glDisable(GL_BLEND);
        glCullFace(GL_BACK);
        glDisable(GL_CULL_FACE);

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);

        gBufferFbo.UnbindGBufferTextures(2);
        localLightsProgram->UnuseShader();
    }
}
