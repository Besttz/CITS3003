#include "Angel.h"

#include <stdlib.h>
#include <dirent.h>
#include <time.h>

// Open Asset Importer header files (in ../../assimp--3.0.1270/include)
// This is a standard open source library for loading meshes, see gnatidread.h
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

GLint windowHeight=640, windowWidth=960;

// gnatidread.cpp is the CITS3003 "Graphics n Animation Tool Interface & Data
// Reader" code.  This file contains parts of the code that you shouldn't need
// to modify (but, you can).
#include "gnatidread.h"
#include "gnatidread2.h"


using namespace std;        // Import the C++ standard functions (e.g., min) 


// IDs for the GLSL program and GLSL variables.
GLuint shaderProgram; // The number identifying the GLSL shader program
GLuint vPosition, vNormal, vTexCoord; // IDs for vshader input vars (from glGetAttribLocation)
GLuint projectionU, modelViewU; // IDs for uniform variables (from glGetUniformLocation)
GLuint vBoneIDs, vBoneWeights, uBoneTransforms; // IDs for uniform variables for Part 2


static float viewDist = 1.5; // Distance from the camera to the centre of the scene
static float camRotSidewaysDeg=0; // rotates the camera sideways around the centre
static float camRotUpAndOverDeg=20; // rotates the camera up and over the centre.

mat4 projection; // Projection matrix - set in the reshape function
mat4 view; // View matrix - set in the display function.

// These are used to set the window title
char lab[] = "Project1";
char *programName = NULL; // Set in main 
int numDisplayCalls = 0; // Used to calculate the number of frames per second

//------Meshes----------------------------------------------------------------
// Uses the type aiMesh from ../../assimp--3.0.1270/include/assimp/mesh.h
//                           (numMeshes is defined in gnatidread.h)
aiMesh* meshes[numMeshes]; // For each mesh we have a pointer to the mesh to draw
GLuint vaoIDs[numMeshes]; // and a corresponding VAO ID from glGenVertexArrays
const aiScene* scenes[numMeshes];


// -----Textures--------------------------------------------------------------
//                           (numTextures is defined in gnatidread.h)
texture* textures[numTextures]; // An array of texture pointers - see gnatidread.h
GLuint textureIDs[numTextures]; // Stores the IDs returned by glGenTextures

//------Scene Objects---------------------------------------------------------
//
// For each object in a scene we store the following
// Note: the following is exactly what the sample solution uses, you can do things differently if you want.
typedef struct {
    vec4 loc;
    float scale;
    float angles[3]; // rotations around X, Y and Z axes.
    float diffuse, specular, ambient; // Amount of each light component
    float shine;
    vec3 rgb;
    float brightness; // Multiplies all colours
    int meshId;
    int texId;
    float texScale;
    //Animation parameters
    float aniStartime;//The start time of animation
    float aniSpeed; //Speed for animation
    float aniTotalTicks;//Total ticks for animatiomn
    float aniTicksPerSec;//Ticks per seconds
    float aniTotalTime;//Total time for animation
    float aniCurrentTicks;//Present animation time
    bool aniUpend = false;
    bool hide = false;
} SceneObject;

const int maxObjects = 1024; // Scenes with more than 1024 objects seem unlikely

SceneObject sceneObjs[maxObjects]; // An array storing the objects currently in the scene.
int nObjects = 0;    // How many objects are currenly in the scene.
int currObject = -1; // The current object
int toolObj = -1;    // The object currently being modified

int currentObjectId;//Saving the currentObject Menu ID.

//----------------------------------------------------------------------------
//
// Loads a texture by number, and binds it for later use.    
void loadTextureIfNotAlreadyLoaded(int i)
{
    if (textures[i] != NULL) return; // The texture is already loaded.

    textures[i] = loadTextureNum(i); CheckError();
    glActiveTexture(GL_TEXTURE0); CheckError();

    // Based on: http://www.opengl.org/wiki/Common_Mistakes
    glBindTexture(GL_TEXTURE_2D, textureIDs[i]); CheckError();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textures[i]->width, textures[i]->height,
                 0, GL_RGB, GL_UNSIGNED_BYTE, textures[i]->rgbData); CheckError();
    glGenerateMipmap(GL_TEXTURE_2D); CheckError();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); CheckError();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); CheckError();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); CheckError();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); CheckError();

    glBindTexture(GL_TEXTURE_2D, 0); CheckError(); // Back to default texture
}

//------Mesh loading----------------------------------------------------------
//
// The following uses the Open Asset Importer library via loadMesh in 
// gnatidread.h to load models in .x format, including vertex positions, 
// normals, and texture coordinates.
// You shouldn't need to modify this - it's called from drawMesh below.

void loadMeshIfNotAlreadyLoaded(int meshNumber)
{
    if (meshNumber>=numMeshes || meshNumber < 0) {
        printf("Error - no such model number");
        exit(1);
    }

    if (meshes[meshNumber] != NULL)
        return; // Already loaded

    const aiScene* scene = loadScene(meshNumber);
    scenes[meshNumber] = scene;
    aiMesh* mesh = scene->mMeshes[0];
    meshes[meshNumber] = mesh;;

    glBindVertexArrayAPPLE( vaoIDs[meshNumber] );

    // Create and initialize a buffer object for positions and texture coordinates, initially empty.
    // mesh->mTextureCoords[0] has space for up to 3 dimensions, but we only need 2.
    GLuint buffer[1];
    glGenBuffers( 1, buffer );
    glBindBuffer( GL_ARRAY_BUFFER, buffer[0] );
    glBufferData( GL_ARRAY_BUFFER, sizeof(float)*(3+3+3)*mesh->mNumVertices, NULL, GL_STATIC_DRAW );

    int nVerts = mesh->mNumVertices;
    // Next, we load the position and texCoord data in parts.    
    glBufferSubData( GL_ARRAY_BUFFER, 0, sizeof(float)*3*nVerts, mesh->mVertices );
    glBufferSubData( GL_ARRAY_BUFFER, sizeof(float)*3*nVerts, sizeof(float)*3*nVerts, mesh->mTextureCoords[0] );
    glBufferSubData( GL_ARRAY_BUFFER, sizeof(float)*6*nVerts, sizeof(float)*3*nVerts, mesh->mNormals);



    // Load the element index data
    GLuint elements[mesh->mNumFaces*3];
    for (GLuint i=0; i < mesh->mNumFaces; i++) {
        elements[i*3] = mesh->mFaces[i].mIndices[0];
        elements[i*3+1] = mesh->mFaces[i].mIndices[1];
        elements[i*3+2] = mesh->mFaces[i].mIndices[2];
    }

    GLuint elementBufferId[1];
    glGenBuffers(1, elementBufferId);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBufferId[0]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * mesh->mNumFaces * 3, elements, GL_STATIC_DRAW);

    // vPosition it actually 4D - the conversion sets the fourth dimension (i.e. w) to 1.0                 
    glVertexAttribPointer( vPosition, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0) );
    glEnableVertexAttribArray( vPosition );

    // vTexCoord is actually 2D - the third dimension is ignored (it's always 0.0)
    glVertexAttribPointer( vTexCoord, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(sizeof(float)*3*mesh->mNumVertices) );
    glEnableVertexAttribArray( vTexCoord );

    glVertexAttribPointer( vNormal,   3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(sizeof(float)*6*mesh->mNumVertices) );
    glEnableVertexAttribArray( vNormal );
    CheckError();

    // Get boneIDs and boneWeights for each vertex from the imported mesh data
    GLint boneIDs[mesh->mNumVertices][4];
    GLfloat boneWeights[mesh->mNumVertices][4];
    getBonesAffectingEachVertex(mesh, boneIDs, boneWeights);

    GLuint buffers[2];
    glGenBuffers( 2, buffers );  // Add two vertex buffer objects
    
    glBindBuffer( GL_ARRAY_BUFFER, buffers[0] ); CheckError();
    glBufferData( GL_ARRAY_BUFFER, sizeof(int)*4*mesh->mNumVertices, boneIDs, GL_STATIC_DRAW ); CheckError();
    glVertexAttribPointer(vBoneIDs, 4, GL_INT, GL_FALSE, 0, BUFFER_OFFSET(0)); CheckError();
    glEnableVertexAttribArray(vBoneIDs);     CheckError();
    
    glBindBuffer( GL_ARRAY_BUFFER, buffers[1] );
    glBufferData( GL_ARRAY_BUFFER, sizeof(float)*4*mesh->mNumVertices, boneWeights, GL_STATIC_DRAW );
    glVertexAttribPointer(vBoneWeights, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
    glEnableVertexAttribArray(vBoneWeights);    CheckError();
	
}

//----------------------------------------------------------------------------

static void mouseClickOrScroll(int button, int state, int x, int y)
{
    if (button==GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        if (glutGetModifiers() != GLUT_ACTIVE_SHIFT) activateTool(button);
        else activateTool(GLUT_MIDDLE_BUTTON);
    }
    else if (button==GLUT_LEFT_BUTTON && state == GLUT_UP) deactivateTool();
    else if (button==GLUT_MIDDLE_BUTTON && state==GLUT_DOWN) { activateTool(button); }
    else if (button==GLUT_MIDDLE_BUTTON && state==GLUT_UP) deactivateTool();

    else if (button == 3) { // scroll up
        viewDist = (viewDist < 0.0 ? viewDist : viewDist*0.8) - 0.05; // ?: is a ternary operator.. syntax ... condition ? if_true : if_false
    }
    else if (button == 4) { // scroll down
        viewDist = (viewDist < 0.0 ? viewDist : viewDist*1.25) + 0.05;
    }
}

//----------------------------------------------------------------------------

static void mousePassiveMotion(int x, int y)
{
    mouseX=x;
    mouseY=y;
}

//----------------------------------------------------------------------------

mat2 camRotZ()
{
    return rotZ(-camRotSidewaysDeg) * mat2(10.0, 0, 0, -10.0);
}

//------callback functions for doRotate below and later-----------------------

static void adjustCamrotsideViewdist(vec2 cv)
{
    cout << cv << endl;
    camRotSidewaysDeg+=cv[0]; viewDist+=cv[1];
}

static void adjustcamSideUp(vec2 su)
{
    camRotSidewaysDeg+=su[0]; camRotUpAndOverDeg+=su[1];
}
    
static void adjustLocXZ(vec2 xz)
{
    sceneObjs[toolObj].loc[0]+=xz[0]; sceneObjs[toolObj].loc[2]+=xz[1];
}

static void adjustScaleY(vec2 sy)
{
    sceneObjs[toolObj].scale+=sy[0]; sceneObjs[toolObj].loc[1]+=sy[1];
}


//----------------------------------------------------------------------------
//------Set the mouse buttons to rotate the camera----------------------------
//------around the centre of the scene.---------------------------------------
//----------------------------------------------------------------------------

static void doRotate()
{
    setToolCallbacks(adjustCamrotsideViewdist, mat2(400,0,0,-2),
                     adjustcamSideUp, mat2(400, 0, 0,-90) );
}
                                     
//------Add an object to the scene--------------------------------------------

static void addObject(int id, int tex)
{

    vec2 currPos = currMouseXYworld(camRotSidewaysDeg);
    sceneObjs[nObjects].loc[0] = currPos[0];
    sceneObjs[nObjects].loc[1] = 0.0;
    sceneObjs[nObjects].loc[2] = currPos[1];
    sceneObjs[nObjects].loc[3] = 1.0;

    if (id!=0 && id!=55)
        sceneObjs[nObjects].scale = 0.005;
    //Intialation Animation 
    if (id>55) {
        loadMeshIfNotAlreadyLoaded(id);
        sceneObjs[nObjects].scale = 0.05;
        sceneObjs[nObjects].aniStartime = glutGet(GLUT_ELAPSED_TIME);
        sceneObjs[nObjects].aniSpeed = 1.0;
        sceneObjs[nObjects].aniCurrentTicks = 0.0;
        sceneObjs[nObjects].aniTotalTicks =
        scenes[id]->mAnimations[0]->mDuration; //Save the number of ticks
        sceneObjs[nObjects].aniTicksPerSec = 
        scenes[id]->mAnimations[0]->mTicksPerSecond; //Save the number of ticks per second
        sceneObjs[nObjects].aniTotalTime = 
        1000 * sceneObjs[nObjects].aniTotalTicks/sceneObjs[nObjects].aniTicksPerSec;//Calculate the total time in MS
    }

    sceneObjs[nObjects].rgb[0] = 0.7; sceneObjs[nObjects].rgb[1] = 0.7;
    sceneObjs[nObjects].rgb[2] = 0.7; sceneObjs[nObjects].brightness = 1.0;

    sceneObjs[nObjects].diffuse = 1.0; sceneObjs[nObjects].specular = 0.5;
    sceneObjs[nObjects].ambient = 0.7; sceneObjs[nObjects].shine = 10.0;

    sceneObjs[nObjects].angles[0] = 0.0; sceneObjs[nObjects].angles[1] = 180.0;
    sceneObjs[nObjects].angles[2] = 0.0;


    sceneObjs[nObjects].meshId = id;
    if (tex == 0)
    {
        sceneObjs[nObjects].texId = rand() % numTextures;
    } else {
        sceneObjs[nObjects].texId = tex;
    }
    
    
    sceneObjs[nObjects].texScale = 2.0;

    toolObj = currObject = nObjects++;
    //Add to currentObject Menu
    glutSetMenu(currentObjectId);
    glutAddMenuEntry(objectMenuEntries[id-1],currObject+100);
    setToolCallbacks(adjustLocXZ, camRotZ(),
                     adjustScaleY, mat2(0.05, 0, 0, 10.0) );
    glutPostRedisplay();
}
static void duplicateObject(int id)
{
    addObject(sceneObjs[id].meshId,sceneObjs[id].texId);
 
    if (sceneObjs[id].meshId>55) {
        sceneObjs[currObject].aniSpeed = sceneObjs[id].aniSpeed;
        sceneObjs[currObject].aniCurrentTicks = sceneObjs[id].aniCurrentTicks;
        sceneObjs[currObject].aniUpend= sceneObjs[id].aniUpend;
    }

    sceneObjs[currObject].rgb[0] = sceneObjs[id].rgb[0]; 
    sceneObjs[currObject].rgb[1] = sceneObjs[id].rgb[1];
    sceneObjs[currObject].rgb[2] = sceneObjs[id].rgb[2]; 
    sceneObjs[currObject].brightness = sceneObjs[id].brightness;

    sceneObjs[currObject].diffuse = sceneObjs[id].diffuse; 
    sceneObjs[currObject].specular = sceneObjs[id].specular;
    sceneObjs[currObject].ambient = sceneObjs[id].ambient; 
    sceneObjs[currObject].shine = sceneObjs[id].shine;

    sceneObjs[currObject].angles[0] = sceneObjs[id].angles[0]; 
    sceneObjs[currObject].angles[1] = sceneObjs[id].angles[1];
    sceneObjs[currObject].angles[2] = sceneObjs[id].angles[2];

    setToolCallbacks(adjustLocXZ, camRotZ(),
                     adjustScaleY, mat2(0.05, 0, 0, 10.0) );
    glutPostRedisplay();
}

//------The init function-----------------------------------------------------

void init( void )
{
    srand ( time(NULL) ); /* initialize random seed - so the starting scene varies */
    aiInit();

    // for (int i=0; i < numMeshes; i++)
    //     meshes[i] = NULL;

    glGenVertexArraysAPPLE(numMeshes, vaoIDs); CheckError(); // Allocate vertex array objects for meshes
    glGenTextures(numTextures, textureIDs); CheckError(); // Allocate texture objects

    // Load shaders and use the resulting shader program
    shaderProgram = InitShader( "vStart new.glsl", "fStart.glsl" );

    glUseProgram( shaderProgram ); CheckError();

    // Initialize the vertex position attribute from the vertex shader        
    vPosition = glGetAttribLocation( shaderProgram, "vPosition" );
    vNormal = glGetAttribLocation( shaderProgram, "vNormal" ); CheckError();

    // Likewise, initialize the vertex texture coordinates attribute.    
    vTexCoord = glGetAttribLocation( shaderProgram, "vTexCoord" );
    // Likewise, initialize the Bone attributes.    
    vBoneIDs = glGetAttribLocation(shaderProgram, "boneIDs");
    vBoneWeights = glGetAttribLocation(shaderProgram, "boneWeights");
    CheckError();

    projectionU = glGetUniformLocation(shaderProgram, "Projection");
    modelViewU = glGetUniformLocation(shaderProgram, "ModelView");

    uBoneTransforms = glGetUniformLocation(shaderProgram, "boneTransforms");

    // Objects 0, and 1 are the ground and the first light.
    addObject(0,0); // Square for the ground
    sceneObjs[0].loc = vec4(0.0, 0.0, 0.0, 1.0);
    sceneObjs[0].scale = 10.0;
    sceneObjs[0].angles[0] = 90.0; // Rotate it.
    sceneObjs[0].texScale = 5.0; // Repeat the texture.

    addObject(55,0); // Sphere for the first light
    sceneObjs[1].loc = vec4(2.0, 1.0, 1.0, 1.0);
    sceneObjs[1].scale = 0.1;
    sceneObjs[1].texId = 0; // Plain texture
    sceneObjs[1].brightness = 0.5; // The light's brightness is 5 times this (below).

    addObject(55,0); // Sphere for the second light
    sceneObjs[2].loc = vec4(-2.0, 1.0, 1.0, 0.0);
    sceneObjs[2].scale = 0.2;
    sceneObjs[2].texId = 0; // Plain texture
    sceneObjs[2].brightness = 0.75; // The light's brightness is 5 times this (below).

    addObject(rand() % numMeshes,0); // A test mesh

    // We need to enable the depth test to discard fragments that
    // are behind previously drawn fragments for the same pixel.
    glEnable( GL_DEPTH_TEST );
    doRotate(); // Start in camera rotate mode.
    glClearColor( 0.0, 0.0, 0.0, 1.0 ); /* black background */
}

//----------------------------------------------------------------------------

void drawMesh(SceneObject& sceneObj,int index)
{

    // Activate a texture, loading if needed.
    loadTextureIfNotAlreadyLoaded(sceneObj.texId);
    glActiveTexture(GL_TEXTURE0 );
    glBindTexture(GL_TEXTURE_2D, textureIDs[sceneObj.texId]);

    // Texture 0 is the only texture type in this program, and is for the rgb
    // colour of the surface but there could be separate types for, e.g.,
    // specularity and normals.
    glUniform1i( glGetUniformLocation(shaderProgram, "texture"), 0 );

    // Set the texture scale for the shaders
    glUniform1f( glGetUniformLocation( shaderProgram, "texScale"), sceneObj.texScale );

    // Set the projection matrix for the shaders
    glUniformMatrix4fv( projectionU, 1, GL_TRUE, projection );

    // Set the model matrix - this should combine translation, rotation and scaling based on what's
    // in the sceneObj structure (see near the top of the program).

    mat4 model = Translate(sceneObj.loc) *RotateX(sceneObj.angles[0])*
    RotateY(sceneObj.angles[1])*RotateZ(sceneObj.angles[2])* Scale(sceneObj.scale);


    // Set the model-view matrix for the shaders
    glUniformMatrix4fv( modelViewU, 1, GL_TRUE, view * model );

    // Activate the VAO for a mesh, loading if needed.
    loadMeshIfNotAlreadyLoaded(sceneObj.meshId);
    CheckError();
    glBindVertexArrayAPPLE( vaoIDs[sceneObj.meshId] );
    CheckError();


    int nBones = meshes[sceneObj.meshId]->mNumBones;
    if(nBones == 0)
        // If no bones, just a single identity matrix is used
        nBones = 1;

    // get boneTransforms for the first (0th) animation at the given
    // animation time 
    // giving the time relative to the start of the animation,
    // measured in frames, like the frame numbers in Blender.)

    mat4 boneTransforms[nBones];     // was: mat4 boneTransforms[mesh->mNumBones];
    calculateAnimPose(meshes[sceneObj.meshId], scenes[sceneObj.meshId], 0,
                     sceneObj.aniCurrentTicks, boneTransforms);
    glUniformMatrix4fv(uBoneTransforms, nBones, GL_TRUE,
                      (const GLfloat *)boneTransforms);

    glDrawElements(GL_TRIANGLES, meshes[sceneObj.meshId]->mNumFaces * 3,
                   GL_UNSIGNED_INT, NULL);
    CheckError();
}

//----------------------------------------------------------------------------

void display( void )
{
    numDisplayCalls++;

    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    CheckError(); // May report a harmless GL_INVALID_OPERATION with GLEW on the first frame

    // Set the view matrix.  To start with this just moves the camera
    // backwards.  You'll need to add appropriate rotations.

    view = Translate(0.0, 0.0, -viewDist)* RotateX(camRotUpAndOverDeg) * RotateY(camRotSidewaysDeg);

    SceneObject lightObj1 = sceneObjs[1]; 
    vec4 lightPosition = view * lightObj1.loc ;

    if (lightObj1.brightness<=0.0) lightObj1.brightness=0.0;


    glUniform4fv( glGetUniformLocation(shaderProgram, "LightPosition"),
                  1, lightPosition);
    glUniform3fv( glGetUniformLocation(shaderProgram, "Light1Rgb"),
     1, lightObj1.rgb * lightObj1.brightness );

    CheckError();


// QI Light 2
    SceneObject lightObj2 = sceneObjs[2]; 
    vec4 lightPosition2 = view * lightObj2.loc ;
    if (lightObj2.brightness<=0.0) lightObj2.brightness=0.0;


    glUniform4fv( glGetUniformLocation(shaderProgram, "LightPosition2"),
                  1, lightPosition2);
    glUniform3fv( glGetUniformLocation(shaderProgram, "Light2Rgb"),
     1, lightObj2.rgb * lightObj2.brightness );
    CheckError();


    for (int i=0; i < nObjects; i++) {
        SceneObject so = sceneObjs[i];

        if (!so.hide){
        vec3 rgb = so.rgb * so.brightness * 2.0;
        glUniform3fv( glGetUniformLocation(shaderProgram, "AmbientProduct"), 1, so.ambient * rgb );
        CheckError();
        glUniform3fv( glGetUniformLocation(shaderProgram, "DiffuseProduct"), 1, so.diffuse * rgb );
        glUniform3fv( glGetUniformLocation(shaderProgram, "SpecularProduct"), 1, so.specular * rgb );
        glUniform1f( glGetUniformLocation(shaderProgram, "Shininess"), so.shine );
        CheckError();

        //If it's an animation, then calculate the present animation progress
        if(so.aniTotalTime > 0.0) {

            
            float runnedTime = glutGet(GLUT_ELAPSED_TIME)*so.aniSpeed - so.aniStartime;
            if (!so.aniUpend)
            {
                sceneObjs[i].aniCurrentTicks = 
                so.aniTicksPerSec * fmod(runnedTime, so.aniTotalTime)/1000;
            } else {
                sceneObjs[i].aniCurrentTicks = so.aniTicksPerSec * 
                (so.aniTotalTime-fmod(runnedTime, so.aniTotalTime))/1000;
            }
            
        }

        drawMesh(sceneObjs[i],i);
        }
    }

    glutSwapBuffers();
}

//----------------------------------------------------------------------------
//------Menus-----------------------------------------------------------------
//----------------------------------------------------------------------------

static void objectMenu(int id)
{
    deactivateTool();
    addObject(id,0);
}

static void currentObjectMenu(int id){
    deactivateTool();
    toolObj = currObject = id-100;
}

static void texMenu(int id)
{
    deactivateTool();
    if (currObject>=0) {
        sceneObjs[currObject].texId = id;
        glutPostRedisplay();
    }
}

static void groundMenu(int id)
{
    deactivateTool();
    sceneObjs[0].texId = id;
    glutPostRedisplay();
}

static void adjustBrightnessY(vec2 by)
{
    sceneObjs[toolObj].brightness+=by[0];
    sceneObjs[toolObj].loc[1]+=by[1];
}

static void adjustRedGreen(vec2 rg)
{
    sceneObjs[toolObj].rgb[0]+=rg[0];
    sceneObjs[toolObj].rgb[1]+=rg[1];
}

//QC 1
static void adjustAmbientDiffuse(vec2 ad)
{
    sceneObjs[toolObj].ambient+=ad[0];
    sceneObjs[toolObj].diffuse+=ad[1];
}

//QC 2
static void adjustSpecularShine(vec2 ss)
{
    sceneObjs[toolObj].specular+=ss[0];
    sceneObjs[toolObj].shine+=ss[1];
}

static void adjustBlueBrightness(vec2 bl_br)
{
    sceneObjs[toolObj].rgb[2]+=bl_br[0];
    sceneObjs[toolObj].brightness+=bl_br[1];
}

static void lightMenu(int id)
{
    deactivateTool();
    if (id == 70) {
        toolObj = 1;
        setToolCallbacks(adjustLocXZ, camRotZ(),
                         adjustBrightnessY, mat2( 1.0, 0.0, 0.0, 10.0) );

    }
    else if (id >= 71 && id <= 74) {
        toolObj = 1;
        setToolCallbacks(adjustRedGreen, mat2(1.0, 0, 0, 1.0),
                         adjustBlueBrightness, mat2(1.0, 0, 0, 1.0) );
    }
    else if (id == 80){
        toolObj = 2;
        setToolCallbacks(adjustLocXZ, camRotZ(),
                         adjustBrightnessY, mat2( 1.0, 0.0, 0.0, 10.0) );

    } 
    else if (id >= 81 &&  id <=84) {
        toolObj = 2;
        setToolCallbacks(adjustRedGreen, mat2(1.0, 0, 0, 1.0),
                         adjustBlueBrightness, mat2(1.0, 0, 0, 1.0) );

    } else {
        printf("Error in lightMenu\n");
        exit(1);
    }
}

static int createArrayMenu(int size, const char menuEntries[][128], void(*menuFn)(int))
{
    int nSubMenus = (size-1)/10 + 1;
    int subMenus[nSubMenus];

    for (int i=0; i < nSubMenus; i++) {
        subMenus[i] = glutCreateMenu(menuFn);
        for (int j = i*10+1; j <= min(i*10+10, size); j++)
            glutAddMenuEntry( menuEntries[j-1] , j);
        CheckError();
    }
    int menuId = glutCreateMenu(menuFn);

    for (int i=0; i < nSubMenus; i++) {
        char num[6];
        sprintf(num, "%d-%d", i*10+1, min(i*10+10, size));
        glutAddSubMenu(num,subMenus[i]);
        CheckError();
    }
    return menuId;
}

static void materialMenu(int id)
{
    deactivateTool();
    if (currObject < 0) return;
    if (id==10) {
        toolObj = currObject;
        setToolCallbacks(adjustRedGreen, mat2(1, 0, 0, 1),
                         adjustBlueBrightness, mat2(1, 0, 0, 1) );
    }
    if (id==20) {
        toolObj = currObject;
        setToolCallbacks(adjustAmbientDiffuse, mat2(10, 0, 0, 1),
                         adjustSpecularShine, mat2(1, 0, 0, 10) );
    }
    else {
        printf("Error in materialMenu\n");
    }
}

static void adjustAngleYX(vec2 angle_yx)
{
    sceneObjs[currObject].angles[1]+=angle_yx[0];
    sceneObjs[currObject].angles[0]-=angle_yx[1];
}

static void adjustAngleZTexscale(vec2 az_ts)
{
    sceneObjs[currObject].angles[2]+=az_ts[0];
    sceneObjs[currObject].texScale+=az_ts[1];
}

static void mainmenu(int id)
{
    deactivateTool();
    if (id == 41 && currObject>=0) {
        toolObj=currObject;
        setToolCallbacks(adjustLocXZ, camRotZ(),
                         adjustScaleY, mat2(0.05, 0, 0, 10) );
    }
    if (id == 50)
        doRotate();
    if (id == 55 && currObject>=0) {
        setToolCallbacks(adjustAngleYX, mat2(400, 0, 0, -400),
                         adjustAngleZTexscale, mat2(400, 0, 0, 15) );
    }
    if (id == 90 && currObject>=0) {
        bool statu = sceneObjs[currObject].hide;
        if (statu) sceneObjs[currObject].hide = false;
        if (!statu) sceneObjs[currObject].hide = true;
    }
    if (id == 91 && currObject>=0) {
        duplicateObject(currObject);
    }
    if (id == 99) exit(0);
}

void unifyAnimation() {
    float currentTime = glutGet(GLUT_ELAPSED_TIME);
    for (int i=0; i < nObjects; i++) {
        if(sceneObjs[i].aniTotalTime > 0.0) {
        sceneObjs[i].scale = 0.05;
        sceneObjs[i].aniStartime = currentTime;
        sceneObjs[i].aniSpeed = 1.0;
        sceneObjs[i].aniCurrentTicks = 0.0;
        }
    }
}

static void aniMenu(int id)
{
    deactivateTool();
    if (currObject < 0) return;
    if (id==60) {
        toolObj = currObject;
        sceneObjs[currObject].aniSpeed =0.5;
    }
    if (id==61) {
        toolObj = currObject;
        sceneObjs[currObject].aniSpeed =1.0;
    }
    if (id==62) {
        toolObj = currObject;
        sceneObjs[currObject].aniSpeed =1.5;
    }
    if (id==63) {
        toolObj = currObject;
        sceneObjs[currObject].aniSpeed =2.0;
    }
    if (id==64) {
        toolObj = currObject;
        sceneObjs[currObject].aniSpeed =3.0;
    }
    if (id==65) {
        toolObj = currObject;
        sceneObjs[currObject].aniSpeed =4.0;
    }
    if (id==66) {
        toolObj = currObject;
        sceneObjs[currObject].aniSpeed =5.0;
    }
    if (id==67) {
        bool statu = sceneObjs[currObject].aniUpend;
        if (statu) sceneObjs[currObject].aniUpend = false;
        if (!statu) sceneObjs[currObject].aniUpend = true;
    }
    if (id==68) {
        unifyAnimation();
    }
    else {
        printf("Error in animationMenu\n");
    }
}

static void makeMenu()
{
    int objectId = createArrayMenu(numMeshes-1, objectMenuEntries, objectMenu);

    int materialMenuId = glutCreateMenu(materialMenu);
    glutAddMenuEntry("R/G/B/All",10);
    glutAddMenuEntry("Ambient/Diffuse/Specular/Shine",20);

    int texMenuId = createArrayMenu(numTextures, textureMenuEntries, texMenu);
    int groundMenuId = createArrayMenu(numTextures, textureMenuEntries, groundMenu);

    int lightMenuId = glutCreateMenu(lightMenu);
    glutAddMenuEntry("Move Light 1",70);
    glutAddMenuEntry("R/G/B/All Light 1",71);
    glutAddMenuEntry("Move Light 2",80);
    glutAddMenuEntry("R/G/B/All Light 2",81);

    int aniMenuId = glutCreateMenu(aniMenu);
    glutAddMenuEntry("Upend Animation",67);
    glutAddMenuEntry("Unify Animation",68);
    glutAddMenuEntry("Speed 0.5",60);
    glutAddMenuEntry("Speed 1.0",61);
    glutAddMenuEntry("Speed 1.5",62);
    glutAddMenuEntry("Speed 2.0",63);
    glutAddMenuEntry("Speed 3.0",64);
    glutAddMenuEntry("Speed 4.0",65);
    glutAddMenuEntry("Speed 5.0",66);

    currentObjectId =glutCreateMenu(currentObjectMenu);
    glutAddMenuEntry(objectMenuEntries[sceneObjs[currObject].meshId-1],currObject+100);


    glutCreateMenu(mainmenu);
    glutAddMenuEntry("Rotate/Move Camera",50);
    glutAddMenuEntry("Position/Scale", 41);
    glutAddMenuEntry("Rotation/Texture Scale", 55);
    glutAddMenuEntry("Hide/Display",90);
    glutAddMenuEntry("Duplicate Object",91);

    glutAddSubMenu("Add Object", objectId);
    glutAddSubMenu("Current Object", currentObjectId);
    glutAddSubMenu("Animation",aniMenuId);
    glutAddSubMenu("Material", materialMenuId);
    glutAddSubMenu("Texture",texMenuId);
    glutAddSubMenu("Ground Texture",groundMenuId);
    glutAddSubMenu("Lights",lightMenuId);

    glutAddMenuEntry("EXIT", 99);
    glutAttachMenu(GLUT_RIGHT_BUTTON);
}

//----------------------------------------------------------------------------

void keyboard( unsigned char key, int x, int y )
{
    switch ( key ) {
    case 033:
        exit( EXIT_SUCCESS );
        break;
    }
}

//----------------------------------------------------------------------------

void idle( void )
{
    glutPostRedisplay();
}

//----------------------------------------------------------------------------

void reshape( int width, int height )
{
    windowWidth = width;
    windowHeight = height;

    glViewport(0, 0, width, height);

    GLfloat nearDist = 0.01;
    GLfloat  aspect = float(width)/float(height);

     if ( aspect >1.0 ) {
        projection = Frustum(-nearDist*aspect,
                        nearDist*aspect,
                        -nearDist, nearDist,
                        nearDist, 100.0);
    }
    else {
        projection = Frustum(-nearDist,
                        nearDist,
                        -nearDist,
                        nearDist,
                        nearDist*aspect, 100.0);
    }
}

//----------------------------------------------------------------------------

void timer(int unused)
{
    char title[256];
    sprintf(title, "%s %s: %d Frames Per Second @ %d x %d",
                    lab, programName, numDisplayCalls, windowWidth, windowHeight );

    glutSetWindowTitle(title);

    numDisplayCalls = 0;
    glutTimerFunc(1000, timer, 1);
}

//----------------------------------------------------------------------------

char dirDefault1[] = "models-textures";
char dirDefault3[] = "/tmp/models-textures";
char dirDefault4[] = "/d/models-textures";
char dirDefault2[] = "/cslinux/examples/CITS3003/project-files/models-textures";

void fileErr(char* fileName)
{
    printf("Error reading file: %s\n", fileName);
    printf("When not in the CSSE labs, you will need to include the directory containing\n");
    printf("the models on the command line, or put it in the same folder as the exectutable.");
    exit(1);
}

//----------------------------------------------------------------------------

int main( int argc, char* argv[] )
{
    // Get the program name, excluding the directory, for the window title
    programName = argv[0];
    for (char *cpointer = argv[0]; *cpointer != 0; cpointer++)
        if (*cpointer == '/' || *cpointer == '\\') programName = cpointer+1;

    // Set the models-textures directory, via the first argument or some handy defaults.
    if (argc > 1)
        strcpy(dataDir, argv[1]);
    else if (opendir(dirDefault1)) strcpy(dataDir, dirDefault1);
    else if (opendir(dirDefault2)) strcpy(dataDir, dirDefault2);
    else if (opendir(dirDefault3)) strcpy(dataDir, dirDefault3);
    else if (opendir(dirDefault4)) strcpy(dataDir, dirDefault4);
    else fileErr(dirDefault1);

    glutInit( &argc, argv );
    glutInitDisplayMode( GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH );
    glutInitWindowSize( windowWidth, windowHeight );

    // glutInitContextVersion( 3, 2);
    // glutInitContextProfile( GLUT_CORE_PROFILE );            // May cause issues, sigh, but you
    // glutInitContextProfile( GLUT_COMPATIBILITY_PROFILE ); // should still use only OpenGL 3.2 Core
                                                            // features.
    glutCreateWindow( "Initialising..." );

    // glewInit(); // With some old hardware yields GL_INVALID_ENUM, if so use glewExperimental.
    CheckError(); // This bug is explained at: http://www.opengl.org/wiki/OpenGL_Loading_Library

    init();
    CheckError();

    glutDisplayFunc( display );
    glutKeyboardFunc( keyboard );
    glutIdleFunc( idle );

    glutMouseFunc( mouseClickOrScroll );
    glutPassiveMotionFunc(mousePassiveMotion);
    glutMotionFunc( doToolUpdateXY );
 
    glutReshapeFunc( reshape );
    glutTimerFunc( 1000, timer, 1 );
    CheckError();

    makeMenu();
    CheckError();

    glutMainLoop();
    return 0;
}
