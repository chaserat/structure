///////////////////////////////////////////////////////////////////////////////
// main.cpp
// ========
// drawing a torus using VBO & GLSL
// dependency: glad, glfw
//
//  AUTHOR: Song Ho Ahn (song.ahn@gmail.com)
// CREATED: 2017-11-02
// UPDATED: 2023-05-01
///////////////////////////////////////////////////////////////////////////////

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include "Matrices.h"
#include "Png.h"
#include "BitmapFontData.h"     // to draw bitmap font with GLFW
#include "fontCourier20.h"      // font:courier new, height:20px
#include "Timer.h"
#include "Torus.h"


// glfw callbacks
void errorCallback(int error, const char* description);
void framebufferSizeCallback(GLFWwindow* window, int width, int height);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void cursorPosCallback(GLFWwindow* window, double x, double y);

// function prototypes
void preFrame(double frameTime);
void frame(double frameTime);
void postFrame(double frameTime);
void toPerspective();
void toOrtho();
void initGL();
bool initGLSL();
void initVBO();
bool initSharedMem();
void clearSharedMem();
GLuint loadTexture(const char* fileName, bool wrap=true);
void showInfo();
void showFPS();


// Bui-Tuong Phong shading model with texture =================================
const char* vsSource = R"(
// GLSL version (OpenGL 3.3)
#version 330
// uniforms
uniform mat4 matrixModelView;
uniform mat4 matrixNormal;
uniform mat4 matrixModelViewProjection;
// vertex attribs (input)
layout(location=0) in vec3 vertexPosition;
layout(location=1) in vec3 vertexNormal;
layout(location=2) in vec2 vertexTexCoord;
// varyings (output)
out vec3 esVertex;
out vec3 esNormal;
out vec2 texCoord0;
void main()
{
    esVertex = vec3(matrixModelView * vec4(vertexPosition, 1.0));
    esNormal = vec3(matrixNormal * vec4(vertexNormal, 1.0));
    texCoord0 = vertexTexCoord;
    gl_Position = matrixModelViewProjection * vec4(vertexPosition, 1.0);
}
)";

const char* fsSource = R"(
// GLSL version (OpenGL 3.3)
#version 330
// uniforms
uniform vec4 lightPosition;             // should be in the eye space
uniform vec4 lightAmbient;              // light ambient color
uniform vec4 lightDiffuse;              // light diffuse color
uniform vec4 lightSpecular;             // light specular color
uniform vec4 materialAmbient;           // material ambient color
uniform vec4 materialDiffuse;           // material diffuse color
uniform vec4 materialSpecular;          // material specular color
uniform float materialShininess;        // material specular shininess
uniform sampler2D map0;                 // texture map #1
uniform bool textureUsed;               // flag for texture
// varyings (input)
in vec3 esVertex;
in vec3 esNormal;
in vec2 texCoord0;
// output
out vec4 fragColor;
void main()
{
    vec3 normal = normalize(esNormal);
    vec3 light;
    if(lightPosition.w == 0.0)
    {
        light = normalize(lightPosition.xyz);
    }
    else
    {
        light = normalize(lightPosition.xyz - esVertex);
    }
    vec3 view = normalize(-esVertex);
    vec3 reflectVec = reflect(-light, normal);  // 2 * N * (N dot L) - L

    vec3 color = lightAmbient.rgb * materialAmbient.rgb;        // begin with ambient
    float dotNL = max(dot(normal, light), 0.0);
    color += lightDiffuse.rgb * materialDiffuse.rgb * dotNL;    // add diffuse
    if(textureUsed)
        color *= texture(map0, texCoord0).rgb;                  // modulate texture map
    float dotVR = max(dot(view, reflectVec), 0.0);
    color += pow(dotVR, materialShininess) * lightSpecular.rgb * materialSpecular.rgb; // add specular
    fragColor = vec4(color, materialDiffuse.a);                 // set frag color
}
)";



// constants
const int   WINDOW_WIDTH    = 1500;
const int   WINDOW_HEIGHT   = 500;
const float CAMERA_DISTANCE = 5.0f;


// global variables
GLFWwindow* window;
int windowWidth;
int windowHeight;
int fbWidth;
int fbHeight;
double runTime;
bool mouseLeftDown;
bool mouseRightDown;
bool mouseMiddleDown;
double mouseX, mouseY;
float cameraAngleX;
float cameraAngleY;
float cameraDistance;
int drawMode;
GLuint vaoId1, vaoId2;      // IDs of VAO for vertex array states
GLuint vboId1, vboId2;      // IDs of VBO for vertex arrays
GLuint iboId1, iboId2;      // IDs of VBO for index array
GLuint texId;
Matrix4 matrixModelView;
Matrix4 matrixProjection;
BitmapFontData bmFont;
// GLSL
GLuint progId = 0;                  // ID of GLSL program
GLint uniformMatrixModelView;
GLint uniformMatrixModelViewProjection;
GLint uniformMatrixNormal;
GLint uniformLightPosition;
GLint uniformLightAmbient;
GLint uniformLightDiffuse;
GLint uniformLightSpecular;
GLint uniformMaterialAmbient;
GLint uniformMaterialDiffuse;
GLint uniformMaterialSpecular;
GLint uniformMaterialShininess;
GLint uniformMap0;
GLint uniformTextureUsed;
GLint attribVertexPosition;     // 0
GLint attribVertexNormal;       // 1
GLint attribVertexTexCoord;     // 2

// torus: min sectors = 3, min sides = 2
Torus torus1(1.0f, 0.5f, 36, 18, false, 3); // R, r, sectors, sides, flat shading, Z-up
Torus torus2(1.0f, 0.5f, 36, 18);           // R, r, sectors, sides, smooth(default), Z-up(default)



///////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
    // init global vars
    initSharedMem();

    // init GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    //NOTE: mac only supports forward-compatible, core profile for v3.x & v4.x
    #ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif

    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, argv[0], 0, 0);
    if(window)
    {
        int ww, wh, fw, fh;
        glfwGetWindowSize(window, &ww, &wh);
        glfwGetFramebufferSize(window, &fw, &fh);
        std::cout << "GLFW window is created: " << ww << "x" << wh
                  << " (Framebuffer: " << fw << "x" << fh << ")" << std::endl;
    }
    else
    {
        std::cout << "[ERROR] Failed to create GLFW window" << std::endl;
        glfwTerminate();
        exit(1);
    }

    // set OpenGL RC
    glfwMakeContextCurrent(window);

    // init glad to load gl extensions
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    //DEBUG: print glfw/opengl info
    //int majorVersion = 0, minorVersion = 0, revision = 0;
    //glfwGetVersion(&majorVersion, &minorVersion, &revision);
    //std::cout << "OpenGL Version: " << majorVersion << "." << minorVersion << "." << revision << std::endl;
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

    // init OpenGL, vbo glsl after setting RC
    initGL();
    initGLSL();
    initVBO();

    // load bitmap font from array, call it after GL/GLSL is initialized
    bmFont.loadFont(fontCourier20, bitmapCourier20);

    // init GLFW callbacks
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetErrorCallback(errorCallback);

    // load PNG image
    texId = loadTexture("grid512.png", true);
    if(texId)
    {
        std::cout << "Loaded a texture: ID=" << texId << std::endl;
    }
    else
    {
        std::cout << "[ERROR] Failed to load a texture" << std::endl;
    }

    // init projection matrix
    toPerspective();

    // main rendering loop
    glfwSwapInterval(1); // frequency
    while(!glfwWindowShouldClose(window))
    {
        // get frame time
        double currTime = glfwGetTime();
        double frameTime = currTime - runTime;
        runTime = currTime;

        // draw
        preFrame(frameTime);
        frame(frameTime);
        postFrame(frameTime);

        // get events
        glfwPollEvents();
    }

    // terminate program
    glfwTerminate();
    std::cout << "Terminate program" << std::endl;
    return 0;
}



///////////////////////////////////////////////////////////////////////////////
// initialize OpenGL
// disable unused features
///////////////////////////////////////////////////////////////////////////////
void initGL()
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);      // 4-byte pixel alignment

    // enable /disable features
    //glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    //glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    //glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    glClearColor(0, 0, 0, 0);                   // background color
    glClearStencil(0);                          // clear stencil buffer
    glClearDepth(1.0f);                         // 0 is near, 1 is far
    glDepthFunc(GL_LEQUAL);
}



///////////////////////////////////////////////////////////////////////////////
// create glsl programs
///////////////////////////////////////////////////////////////////////////////
bool initGLSL()
{
    const int MAX_LENGTH = 2048;
    char log[MAX_LENGTH];
    int logLength = 0;

    // create shader and program
    GLuint vsId = glCreateShader(GL_VERTEX_SHADER);
    GLuint fsId = glCreateShader(GL_FRAGMENT_SHADER);
    progId = glCreateProgram();

    // load shader sources
    glShaderSource(vsId, 1, &vsSource, NULL);
    glShaderSource(fsId, 1, &fsSource, NULL);

    // compile shader sources
    glCompileShader(vsId);
    glCompileShader(fsId);

    //@@ debug
    int vsStatus, fsStatus;
    glGetShaderiv(vsId, GL_COMPILE_STATUS, &vsStatus);
    if(vsStatus == GL_FALSE)
    {
        glGetShaderiv(vsId, GL_INFO_LOG_LENGTH, &logLength);
        glGetShaderInfoLog(vsId, MAX_LENGTH, &logLength, log);
        std::cout << "===== Vertex Shader Log =====\n" << log << std::endl;
    }
    glGetShaderiv(fsId, GL_COMPILE_STATUS, &fsStatus);
    if(fsStatus == GL_FALSE)
    {
        glGetShaderiv(fsId, GL_INFO_LOG_LENGTH, &logLength);
        glGetShaderInfoLog(fsId, MAX_LENGTH, &logLength, log);
        std::cout << "===== Fragment Shader Log =====\n" << log << std::endl;
    }

    // attach shaders to the program
    glAttachShader(progId, vsId);
    glAttachShader(progId, fsId);

    // link program
    glLinkProgram(progId);

    // get uniform/attrib locations
    glUseProgram(progId);
    uniformMatrixModelView           = glGetUniformLocation(progId, "matrixModelView");
    uniformMatrixModelViewProjection = glGetUniformLocation(progId, "matrixModelViewProjection");
    uniformMatrixNormal              = glGetUniformLocation(progId, "matrixNormal");
    uniformLightPosition             = glGetUniformLocation(progId, "lightPosition");
    uniformLightAmbient              = glGetUniformLocation(progId, "lightAmbient");
    uniformLightDiffuse              = glGetUniformLocation(progId, "lightDiffuse");
    uniformLightSpecular             = glGetUniformLocation(progId, "lightSpecular");
    uniformMaterialAmbient           = glGetUniformLocation(progId, "materialAmbient");
    uniformMaterialDiffuse           = glGetUniformLocation(progId, "materialDiffuse");
    uniformMaterialSpecular          = glGetUniformLocation(progId, "materialSpecular");
    uniformMaterialShininess         = glGetUniformLocation(progId, "materialShininess");
    uniformMap0                      = glGetUniformLocation(progId, "map0");
    uniformTextureUsed               = glGetUniformLocation(progId, "textureUsed");
    attribVertexPosition = glGetAttribLocation(progId, "vertexPosition");
    attribVertexNormal   = glGetAttribLocation(progId, "vertexNormal");
    attribVertexTexCoord = glGetAttribLocation(progId, "vertexTexCoord");

    // set uniform values
    float lightPosition[] = {0, 0, 1, 0};
    float lightAmbient[]  = {0.3f, 0.3f, 0.3f, 1};
    float lightDiffuse[]  = {0.7f, 0.7f, 0.7f, 1};
    float lightSpecular[] = {1.0f, 1.0f, 1.0f, 1};
    float materialAmbient[]  = {0.5f, 0.5f, 0.5f, 1};
    float materialDiffuse[]  = {0.7f, 0.7f, 0.7f, 1};
    float materialSpecular[] = {0.4f, 0.4f, 0.4f, 1};
    float materialShininess  = 16;
    glUniform4fv(uniformLightPosition, 1, lightPosition);
    glUniform4fv(uniformLightAmbient, 1, lightAmbient);
    glUniform4fv(uniformLightDiffuse, 1, lightDiffuse);
    glUniform4fv(uniformLightSpecular, 1, lightSpecular);
    glUniform4fv(uniformMaterialAmbient, 1, materialAmbient);
    glUniform4fv(uniformMaterialDiffuse, 1, materialDiffuse);
    glUniform4fv(uniformMaterialSpecular, 1, materialSpecular);
    glUniform1f(uniformMaterialShininess, materialShininess);
    glUniform1i(uniformMap0, 0);
    glUniform1i(uniformTextureUsed, 1);

    // unbind GLSL
    glUseProgram(0);
    glDeleteShader(vsId);
    glDeleteShader(fsId);

    // check GLSL status
    int linkStatus;
    glGetProgramiv(progId, GL_LINK_STATUS, &linkStatus);
    if(linkStatus == GL_FALSE)
    {
        glGetProgramiv(progId, GL_INFO_LOG_LENGTH, &logLength);
        glGetProgramInfoLog(progId, MAX_LENGTH, &logLength, log);
        std::cout << "===== GLSL Program Log =====\n" << log << std::endl;
        return false;
    }
    else
    {
        return true;
    }
}



///////////////////////////////////////////////////////////////////////////////
// copy vertex data to VBO and VA state to VAO
///////////////////////////////////////////////////////////////////////////////
void initVBO()
{
    // create vertex array object to store all vertex array states only once
    if(!vaoId1)
        glGenVertexArrays(1, &vaoId1);
    glBindVertexArray(vaoId1);

    // create vertex buffer objects
    if(!vboId1)
        glGenBuffers(1, &vboId1);

    glBindBuffer(GL_ARRAY_BUFFER, vboId1);
    glBufferData(GL_ARRAY_BUFFER, torus1.getInterleavedVertexSize(), torus1.getInterleavedVertices(), GL_STATIC_DRAW);

    if(!iboId1)
        glGenBuffers(1, &iboId1);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iboId1);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, torus1.getIndexSize(), torus1.getIndices(), GL_STATIC_DRAW);

    // enable vertex array attributes for bound VAO
    glEnableVertexAttribArray(attribVertexPosition);
    glEnableVertexAttribArray(attribVertexNormal);
    glEnableVertexAttribArray(attribVertexTexCoord);

    // store vertex array pointers to bound VAO
    int stride = torus1.getInterleavedStride();
    glVertexAttribPointer(attribVertexPosition, 3, GL_FLOAT, false, stride, 0);
    glVertexAttribPointer(attribVertexNormal, 3, GL_FLOAT, false, stride, (void*)(3 * sizeof(float)));
    glVertexAttribPointer(attribVertexTexCoord, 2, GL_FLOAT, false, stride, (void*)(6 * sizeof(float)));


    if(!vaoId2)
        glGenVertexArrays(1, &vaoId2);
    glBindVertexArray(vaoId2);

    if(!vboId2)
        glGenBuffers(1, &vboId2);

    glBindBuffer(GL_ARRAY_BUFFER, vboId2);
    glBufferData(GL_ARRAY_BUFFER, torus2.getInterleavedVertexSize(), torus2.getInterleavedVertices(), GL_STATIC_DRAW);

    if(!iboId2)
        glGenBuffers(1, &iboId2);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iboId2);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, torus2.getIndexSize(), torus2.getIndices(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(attribVertexPosition);
    glEnableVertexAttribArray(attribVertexNormal);
    glEnableVertexAttribArray(attribVertexTexCoord);

    stride = torus2.getInterleavedStride();
    glVertexAttribPointer(attribVertexPosition, 3, GL_FLOAT, false, stride, 0);
    glVertexAttribPointer(attribVertexNormal, 3, GL_FLOAT, false, stride, (void*)(3 * sizeof(float)));
    glVertexAttribPointer(attribVertexTexCoord, 2, GL_FLOAT, false, stride, (void*)(6 * sizeof(float)));

    // unbind
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}



///////////////////////////////////////////////////////////////////////////////
// initialize global variables
///////////////////////////////////////////////////////////////////////////////
bool initSharedMem()
{
    windowWidth = fbWidth = WINDOW_WIDTH;
    windowHeight = fbHeight = WINDOW_HEIGHT;

    runTime = 0;

    mouseLeftDown = mouseRightDown = mouseMiddleDown = false;
    mouseX = mouseY = 0;

    cameraAngleX = cameraAngleY = 0.0f;
    cameraDistance = CAMERA_DISTANCE;

    drawMode = 0; // 0:fill, 1: wireframe, 2:points

    vaoId1 = vaoId2 = 0;
    vboId1 = vboId2 = 0;
    iboId1 = iboId2 = 0;
    texId = 0;

    // test setters
    //torus2.setMajorRadius(2);
    //torus2.setMinorRadius(0.1f);
    //torus2.setSectorCount(4);
    //torus2.setSideCount(5);
    //torus2.setSmooth(false);
    //torus2.setUpAxis(1);

    // debug
    torus2.printSelf();

    return true;
}



///////////////////////////////////////////////////////////////////////////////
// clean up global vars
///////////////////////////////////////////////////////////////////////////////
void clearSharedMem()
{
    // clean up VBOs
    glDeleteBuffers(1, &vboId1);
    glDeleteBuffers(1, &iboId1);
    glDeleteBuffers(1, &vboId2);
    glDeleteBuffers(1, &iboId2);
    vboId1 = iboId1 = 0;
    vboId2 = iboId2 = 0;

    // clean up VAOs
    glDeleteVertexArrays(1, &vaoId1);
    glDeleteVertexArrays(1, &vaoId2);
    vaoId1 = vaoId2 = 0;

    // clean up tex
    glDeleteTextures(1, &texId);
    texId = 0;
}



///////////////////////////////////////////////////////////////////////////////
// load raw image as a texture
///////////////////////////////////////////////////////////////////////////////
GLuint loadTexture(const char* fileName, bool wrap)
{
    Image::Png png;
    if(!png.read(fileName))
        return 0;     // exit if failed load image

    // get bitmap info
    int width = png.getWidth();
    int height = png.getHeight();
    const unsigned char* data = png.getData();
    GLenum type = GL_UNSIGNED_BYTE;    // only allow PNG with 8-bit per channel

    // We assume the image is 8-bit, 24-bit or 32-bit PNG
    GLenum format;
    int bpp = png.getBitCount();
    if(bpp == 8)
        format = GL_LUMINANCE;
    else if(bpp == 24)
        format = GL_RGB;
    else if(bpp == 32)
        format = GL_RGBA;
    else
        return 0;               // NOT supported, exit

    // gen texture ID
    GLuint texture;
    glGenTextures(1, &texture);

    // set active texture and configure it
    glBindTexture(GL_TEXTURE_2D, texture);

    // select modulate to mix texture with color for shading
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);

    // if wrap is true, the texture wraps over at the edges (repeat)
    //       ... false, the texture ends at the edges (clamp)
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap ? GL_REPEAT : GL_CLAMP);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap ? GL_REPEAT : GL_CLAMP);
    //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // copy texture data
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, type, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    return texture;
}



///////////////////////////////////////////////////////////////////////////////
// display info messages
///////////////////////////////////////////////////////////////////////////////
void showInfo()
{
    toOrtho();

    // call it once before drawing text to configure orthographic projection
    bmFont.setWindowSize(windowWidth, windowHeight);

    std::stringstream ss;
    ss << std::fixed << std::setprecision(3);

    int x = 1;
    int y = windowHeight - bmFont.getBaseline();
    bmFont.setColor(1, 1, 1, 1);

    ss << "Major Radius: " << torus2.getMajorRadius() << std::ends;
    bmFont.drawText(x, y, ss.str().c_str());
    ss.str("");
    y -= bmFont.getHeight();

    ss << "Minor Radius: " << torus2.getMinorRadius() << std::ends;
    bmFont.drawText(x, y, ss.str().c_str());
    ss.str("");
    y -= bmFont.getHeight();

    ss << "Sector Count: " << torus2.getSectorCount() << std::ends;
    bmFont.drawText(x, y, ss.str().c_str());
    ss.str("");
    y -= bmFont.getHeight();

    ss << "Side Count: " << torus2.getSideCount() << std::ends;
    bmFont.drawText(x, y, ss.str().c_str());
    ss.str("");
    y -= bmFont.getHeight();

    ss << "Vertex Count: " << torus2.getVertexCount() << std::ends;
    bmFont.drawText(x, y, ss.str().c_str());
    ss.str("");
    y -= bmFont.getHeight();

    ss << "Index Count: " << torus2.getIndexCount() << std::ends;
    bmFont.drawText(x, y, ss.str().c_str());
    ss.str("");

    // unset floating format
    ss << std::resetiosflags(std::ios_base::fixed | std::ios_base::floatfield);

    showFPS();

    // go back to perspective mode
    toPerspective();
}



///////////////////////////////////////////////////////////////////////////////
// display frame rates
///////////////////////////////////////////////////////////////////////////////
void showFPS()
{
    static Timer timer;
    static int count = 0;
    static std::string fps = "0.0 FPS";
    double elapsedTime = 0.0;

    ++count;

    // update fps every second
    elapsedTime = timer.getElapsedTime();
    if(elapsedTime >= 1.0)
    {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1);
        ss << (count / elapsedTime) << " FPS" << std::ends; // update fps string
        ss << std::resetiosflags(std::ios_base::fixed | std::ios_base::floatfield);
        fps = ss.str();
        count = 0;                      // reset counter
        timer.start();                  // restart timer
    }

    // draw FPS at top-right corner
    int x = windowWidth - bmFont.getTextWidth(fps.c_str()) - 1;
    int y = windowHeight - bmFont.getBaseline();
    bmFont.setColor(1, 1, 0, 1.0f);
    bmFont.drawText(x, y, fps.c_str());
}



///////////////////////////////////////////////////////////////////////////////
// set projection matrix as orthogonal
///////////////////////////////////////////////////////////////////////////////
void toOrtho()
{
    const float N = -1.0f;
    const float F = 1.0f;

    // get current dimensions
    glfwGetWindowSize(window, &windowWidth, &windowHeight); // get window size
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);    // get framebuffer size

    // set viewport to be the entire framebuffer size
    glViewport(0, 0, (GLsizei)fbWidth, (GLsizei)fbHeight);

    // construct ortho projection matrix, not framebuffer size
    matrixProjection.identity();
    matrixProjection[0]  =  2.0f / windowWidth;
    matrixProjection[5]  =  2.0f / windowHeight;
    matrixProjection[10] = -2.0f / (F - N);
    matrixProjection[12] = -1.0f;
    matrixProjection[13] = -1.0f;
    matrixProjection[14] = -(F + N) / (F - N);
}



///////////////////////////////////////////////////////////////////////////////
// set the projection matrix as perspective
///////////////////////////////////////////////////////////////////////////////
void toPerspective()
{
    const float N = 0.1f;
    const float F = 100.0f;
    const float PI = acos(-1.0f);
    const float FOV_Y = 40.0f / 180.0f * PI;    // in radian

    // get current dimensions
    glfwGetWindowSize(window, &windowWidth, &windowHeight); // get window size
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);    // get framebuffer size

    // set viewport to be the entire framebuffer size
    glViewport(0, 0, (GLsizei)fbWidth, (GLsizei)fbHeight);

    // construct perspective projection matrix
    float aspectRatio = (float)(windowWidth) / windowHeight;
    float tangent = tanf(FOV_Y / 2.0f);     // tangent of half fovY
    float h = N * tangent;                  // half height of near plane
    float w = h * aspectRatio;              // half width of near plane
    matrixProjection.identity();
    matrixProjection[0]  =  N / w;
    matrixProjection[5]  =  N / h;
    matrixProjection[10] = -(F + N) / (F - N);
    matrixProjection[11] = -1;
    matrixProjection[14] = -(2 * F * N) / (F - N);
    matrixProjection[15] =  0;
}







///////////////////////////////////////////////////////////////////////////////
// render each frame
///////////////////////////////////////////////////////////////////////////////
void preFrame(double frameTime)
{
}

void frame(double frameTime)
{
    // clear buffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // transform camera (view)
    Matrix4 matrixView;
    matrixView.translate(0, 0, -cameraDistance);

    // common model matrix
    Matrix4 matrixModelCommon;
    matrixModelCommon.rotateY(cameraAngleY);
    matrixModelCommon.rotateX(cameraAngleX);

    // model matrix for each instance
    Matrix4 matrixModel1(matrixModelCommon);    // left
    Matrix4 matrixModel2(matrixModelCommon);    // center
    Matrix4 matrixModel3(matrixModelCommon);    // right
    matrixModel1.translate(-3.5f, 0, 0);        // shift left
    matrixModel3.translate(3.5f, 0, 0);         // shift right

    // bind GLSL, texture
    glUseProgram(progId);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texId);

    // set model matrix uniforms
    matrixModelView = matrixView * matrixModel1;
    Matrix4 matrixModelViewProjection = matrixProjection * matrixModelView;
    Matrix4 matrixNormal = matrixModelView;
    matrixNormal.setColumn(3, Vector4(0,0,0,1));
    glUniformMatrix4fv(uniformMatrixModelView, 1, false, matrixModelView.get());
    glUniformMatrix4fv(uniformMatrixModelViewProjection, 1, false, matrixModelViewProjection.get());
    glUniformMatrix4fv(uniformMatrixNormal, 1, false, matrixNormal.get());

    // left and center torus do not use texture
    glUniform1i(uniformTextureUsed, 0);

    // draw left torus
    glBindVertexArray(vaoId1);
    glDrawElements(GL_TRIANGLES,            // primitive type
                   torus1.getIndexCount(),  // # of indices
                   GL_UNSIGNED_INT,         // data type
                   (void*)0);               // ptr to indices

    // set matrix uniforms for center torus
    matrixModelView = matrixView * matrixModel2;
    matrixModelViewProjection = matrixProjection * matrixModelView;
    matrixNormal = matrixModelView;
    matrixNormal.setColumn(3, Vector4(0,0,0,1));
    glUniformMatrix4fv(uniformMatrixModelView, 1, false, matrixModelView.get());
    glUniformMatrix4fv(uniformMatrixModelViewProjection, 1, false, matrixModelViewProjection.get());
    glUniformMatrix4fv(uniformMatrixNormal, 1, false, matrixNormal.get());

    // draw center torus
    glBindVertexArray(vaoId2);
    glDrawElements(GL_TRIANGLES,            // primitive type
                   torus2.getIndexCount(),  // # of indices
                   GL_UNSIGNED_INT,         // data type
                   (void*)0);               // ptr to indices

    // set matric uniforms for right torus
    matrixModelView = matrixView * matrixModel3;
    matrixModelViewProjection = matrixProjection * matrixModelView;
    matrixNormal = matrixModelView;
    matrixNormal.setColumn(3, Vector4(0,0,0,1));
    glUniformMatrix4fv(uniformMatrixModelView, 1, false, matrixModelView.get());
    glUniformMatrix4fv(uniformMatrixModelViewProjection, 1, false, matrixModelViewProjection.get());
    glUniformMatrix4fv(uniformMatrixNormal, 1, false, matrixNormal.get());

    // right torus is rendered with texture
    glUniform1i(uniformTextureUsed, 1);

    // draw right torus
    glBindVertexArray(vaoId2);
    glDrawElements(GL_TRIANGLES,            // primitive type
                   torus2.getIndexCount(),  // # of indices
                   GL_UNSIGNED_INT,         // data type
                   (void*)0);               // ptr to indices

    // unbind
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    glUseProgram(0);

    showInfo();

    glfwSwapBuffers(window);
}

void postFrame(double frameTime)
{
    static double elapsedTime = 0.0;
    static int frameCount = 0;
    elapsedTime += frameTime;
    ++frameCount;
    if(elapsedTime > 1.0)
    {
        double fps = frameCount / elapsedTime;
        elapsedTime = 0;
        frameCount = 0;
        //std::cout << "FPS: " << fps << std::endl;
    }
}



//=============================================================================
// GLFW CALLBACKS
//=============================================================================
void errorCallback(int error, const char* description)
{
    std::cout << "[ERROR]: " << description << std::endl;
}

void framebufferSizeCallback(GLFWwindow* window, int w, int h)
{
    toPerspective();
    std::cout << "Framebuffer resized: " << fbWidth << "x" << fbHeight
              << " (Window: " << windowWidth << "x" << windowHeight << ")" << std::endl;
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        //std::cout << "Closing program" << std::endl;
        clearSharedMem();
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    else if(key == GLFW_KEY_SPACE && action == GLFW_PRESS)
    {
        torus1.reverseNormals();
        torus2.reverseNormals();
        initVBO();  // copy new vertext data to VBOs
    }
    else if(key == GLFW_KEY_D && action == GLFW_PRESS)
    {
        ++drawMode;
        drawMode %= 3;
        if(drawMode == 0)        // fill mode
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
        }
        else if(drawMode == 1)  // wireframe mode
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
        }
        else                    // point mode
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
        }
    }
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    // remember mouse position
    glfwGetCursorPos(window, &mouseX, &mouseY);

    if(button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if(action == GLFW_PRESS)
            mouseLeftDown = true;
        else if(action == GLFW_RELEASE)
            mouseLeftDown = false;
    }
    else if(button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if(action == GLFW_PRESS)
            mouseRightDown = true;
        else if(action == GLFW_RELEASE)
            mouseRightDown = false;
    }
    else if(button == GLFW_MOUSE_BUTTON_MIDDLE)
    {
        if(action == GLFW_PRESS)
            mouseMiddleDown = true;
        else if(action == GLFW_RELEASE)
            mouseMiddleDown = false;
    }
}

void cursorPosCallback(GLFWwindow* window, double x, double y)
{
    if(mouseLeftDown)
    {
        cameraAngleY += (x - mouseX);
        cameraAngleX += (y - mouseY);
        mouseX = x;
        mouseY = y;
    }
    if(mouseRightDown)
    {
        cameraDistance -= (y - mouseY) * 0.2f;
        mouseY = y;
    }
}
