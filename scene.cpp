
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

const int numLights = 128;

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

HDR* skyIrrMap;

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
// InitializeScene is called once during setup to create all the
// textures, shape VAOs, and shader programs as well as setting a
// number of other parameters.
void Scene::InitializeScene()
{
    glEnable(GL_DEPTH_TEST);
    CHECKERROR;

    // @@ Initialize interactive viewing variables here. (spin, tilt, ry, front back, ...)
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

    shadowFbo.CreateFBO(4096, 4096);
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
	gBufferProgram->LinkProgram();

	deferredLightProgram = new ShaderProgram();
	deferredLightProgram->AddShader("deferredLight.vert", GL_VERTEX_SHADER);
	deferredLightProgram->AddShader("deferredLight.frag", GL_FRAGMENT_SHADER);
	glBindAttribLocation(deferredLightProgram->programId, 0, "vertex");
	glBindAttribLocation(deferredLightProgram->programId, 2, "vertexTexture");
	deferredLightProgram->LinkProgram();

    localLightsProgram = new ShaderProgram();
    localLightsProgram->AddShader("localLights.vert", GL_VERTEX_SHADER);
    localLightsProgram->AddShader("localLights.frag", GL_FRAGMENT_SHADER);
    glBindAttribLocation(localLightsProgram->programId, 0, "vertex");
    localLightsProgram->LinkProgram();

	// Shadow Map Shader Program Initialization
    shadowProgram = new ShaderProgram();
    shadowProgram->AddShader("shadow.frag", GL_FRAGMENT_SHADER);
    shadowProgram->AddShader("shadow.vert", GL_VERTEX_SHADER);
    glBindAttribLocation(shadowProgram->programId, 0, "vertex");
    glBindAttribLocation(shadowProgram->programId, 1, "vertexNormal");
    glBindAttribLocation(shadowProgram->programId, 2, "vertexTexture");
    glBindAttribLocation(shadowProgram->programId, 3, "vertexTangent");
    shadowProgram->LinkProgram();

    // Create the lighting shader program from source code files.
    // @@ Initialize additional shaders if necessary

    
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

    HDR* skyHDR = new HDR("skys/Road_to_MonumentValley_Ref.hdr");
    skyIrrMap = new HDR("skys/Road_to_MonumentValley_Ref.irr.hdr"); // is this how you really read it in?

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
            if (ImGui::MenuItem("Draw ground/sea", "", ground->drawMe)){ground->drawMe ^= true;
                							sea->drawMe = ground->drawMe;}
            ImGui::EndMenu(); }
                	
        // This menu demonstrates how to provide the user a choice
        // among a set of choices.  The current choice is stored in a
        // variable named "mode" in the application, and sent to the
        // shader to be used as you wish.
        if (ImGui::BeginMenu("Menu ")) {
            if (ImGui::MenuItem("<sample menu of choices>", "",	false, false)) {}
            if (ImGui::MenuItem("Do nothing 0", "",		mode==0)) { mode=0; }
            if (ImGui::MenuItem("Do nothing 1", "",		mode==1)) { mode=1; }
            if (ImGui::MenuItem("Do nothing 2", "",		mode==2)) { mode=2; }
            ImGui::EndMenu(); }
        
        ImGui::EndMainMenuBar(); }
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

    // Set Light
   glm::vec3 Light(3, 3, 3);
   glm::vec3 Ambient(0.4, 0.4, 0.4);

   eye = glm::vec3(WorldInverse * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

   // -----------------------------------------------------------------
    // Shadow Pass
    // Create Shadow Map
    // -----------------------------------------------------------------

   shadowProgram->UseShader();

   programId = shadowProgram->programId;
   shadowFbo.BindFBO();

   ShadowView = LookAt(lightPos, glm::vec3(0.0, 0.0, 0.0), glm::vec3(0.0, 0.0, 1.0));
   ShadowProj = Perspective(40 / lightDist, 40 / lightDist, front, back);

   // We clear to black (0,0,0) so empty space has no position/normal data
   glViewport(0, 0, 4096, 4096);
   glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   loc = glGetUniformLocation(programId, "ProjectionMatrix");
   glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(ShadowProj));
   loc = glGetUniformLocation(programId, "ViewMatrix");
   glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(ShadowView));

   // Draw all objects (This recursively traverses the object hierarchy.)
   CHECKERROR;
   glEnable(GL_CULL_FACE);
   glCullFace(GL_FRONT);
   objectRoot->Draw(shadowProgram, Identity);
   glDisable(GL_CULL_FACE);
   CHECKERROR;

   // Turn off the shader
   shadowFbo.UnbindFBO();
   shadowProgram->UnuseShader();

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

    // 2. Bind G-Buffer (We only need to do this ONCE for all lights)
    gBufferFbo.BindGBufferTextures(2, programId,
        "gFragData", "gLightVec", "gEyeVec");

    shadowFbo.BindTexture(8, programId, "shadowMap");

    const glm::mat4 B = Translate(0.5f, 0.5f, 0.5f) * Scale(0.5f, 0.5f, 0.5f);
	ShadowMatrix = B * ShadowProj * ShadowView;

    loc = glGetUniformLocation(programId, "ShadowMatrix");
    glUniformMatrix4fv(loc, 1, GL_FALSE, Pntr(ShadowMatrix));

    loc = glGetUniformLocation(programId, "Ambient");
    glUniform3fv(loc, 1, &(Ambient[0]));

    loc = glGetUniformLocation(programId, "viewMode");
    glUniform1i(loc, 0);

    // Set Light Position
    loc = glGetUniformLocation(programId, "lightPos");
    glUniform3fv(loc, 1, &lightPos[0]); // Ensure this variable exists in your class!

    // Set Light Color
    glm::vec3 debugLightColor(1.0, 1.0, 1.0);
    loc = glGetUniformLocation(programId, "lightColor");
    glUniform3fv(loc, 1, &debugLightColor[0]);

    // Set View Position (Needed for Specular)
    loc = glGetUniformLocation(programId, "viewPos");
    glUniform3fv(loc, 1, &eye[0]);

    loc = glGetUniformLocation(programId, "width");
    glUniform1f(loc, width);

    loc = glGetUniformLocation(programId, "height");
    glUniform1f(loc, height);

    // Set Light Position
    loc = glGetUniformLocation(programId, "sceneLightPos");
    glUniform3fv(loc, 1, &lightPos[0]); // Ensure this variable exists in your class!

    // Set View Position
    loc = glGetUniformLocation(programId, "sceneEye");
    glUniform3fv(loc, 1, &eye[0]);

    CHECKERROR;
    fullScreenQuad->Draw(deferredLightProgram, Identity);
    CHECKERROR;

    gBufferFbo.UnbindGBufferTextures(2);
	shadowFbo.UnbindTexture(6);
    deferredLightProgram->UnuseShader();

    // -----------------------------------------------------------------
    // PASS 3: Local Lights Pass
    // Render many local lights
    // -----------------------------------------------------------------

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
        //lightVolumeSphere->DrawVAO();
        //objectRoot->Draw(localLightsProgram, model);
        CHECKERROR;
    }

    //lightVolumeSphere->Draw(localLightsProgram, Scale(3.0, 3.0, 3.0));

    // Clean up
    glDisable(GL_BLEND);
    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    gBufferFbo.UnbindGBufferTextures(2);
    localLightsProgram->UnuseShader();
}
