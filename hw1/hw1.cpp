/*
  CSCI 420 Computer Graphics, USC
  Assignment 1: Height Fields
  Johnny Geng
*/

#include "basicPipelineProgram.h"
#include "openGLMatrix.h"
#include "imageIO.h"
#include "openGLHeader.h"
#include "glutHeader.h"

#include <iostream>
#include <cstring>
#include <vector>

#if defined(WIN32) || defined(_WIN32)
  #ifdef _DEBUG
    #pragma comment(lib, "glew32d.lib")
  #else
    #pragma comment(lib, "glew32.lib")
  #endif
#endif

#if defined(WIN32) || defined(_WIN32)
  char shaderBasePath[1024] = SHADER_BASE_PATH;
#else
  char shaderBasePath[1024] = "../openGLHelper-starterCode";
#endif

using namespace std;

int mousePos[2]; // x,y coordinate of the mouse position

int leftMouseButton = 0; // 1 if pressed, 0 if not 
int middleMouseButton = 0; // 1 if pressed, 0 if not
int rightMouseButton = 0; // 1 if pressed, 0 if not

typedef enum { ROTATE, TRANSLATE, SCALE } CONTROL_STATE;
CONTROL_STATE controlState = ROTATE;

// state of the world
float landRotate[3] = { 0.0f, 0.0f, 0.0f };
float landTranslate[3] = { 0.0f, 0.0f, 0.0f };
float landScale[3] = { 1.0f, 1.0f, 1.0f };
int windowWidth = 1280;
int windowHeight = 720;
char windowTitle[512] = "CSCI 420 homework I";

ImageIO * heightmapImage;

OpenGLMatrix matrix;
BasicPipelineProgram * pipelineProgram;

// Additional variables
typedef enum {Points, Lines, SolidTriangles, SmoothenedTriangles} RenderMode;
RenderMode rendermode = RenderMode::Points; // defaulting to point mode (0)
int imgWidth, imgHeight;
int numOfImg = 0;
float heightscale = 0.15f;
int numPointVerts, numLineVerts, numTriVerts;
vector<float> pointVerts, pointCol, lineVerts, lineCol, triVerts, triCol, leftSmoTriVerts, rightSmoTriVerts, upSmoTriVerts, downSmoTriVerts;
GLuint pointVao, pointPosVbo, pointColVbo, lineVao, linePosVbo, lineColVbo, triVao, triPosVbo, triColVbo, smoTriVao,smoTriVboLeft, smoTriVboRight, smoTriVboUp, smoTriVboDown;

// write a screenshot to the specified filename
void saveScreenshot(const char * filename)
{
  unsigned char * screenshotData = new unsigned char[windowWidth * windowHeight * 3];
  glReadPixels(0, 0, windowWidth, windowHeight, GL_RGB, GL_UNSIGNED_BYTE, screenshotData);

  ImageIO screenshotImg(windowWidth, windowHeight, 3, screenshotData);

  if (screenshotImg.save(filename, ImageIO::FORMAT_JPEG) == ImageIO::OK)
    cout << "File " << filename << " saved successfully." << endl;
  else cout << "Failed to save file " << filename << '.' << endl;

  delete [] screenshotData;
}

void displayFunc()
{
  // render some stuff...
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  matrix.SetMatrixMode(OpenGLMatrix::ModelView);
  matrix.LoadIdentity();
  matrix.LookAt(
                128, 200, 80, // eye x,y,z
                128, 0, -128, // focus point x,y,z (center of the image)
                0, 1, 0 // up vector x,y,z
                );
  matrix.Translate(landTranslate[0], landTranslate[1], landTranslate[2]);
  matrix.Rotate(landRotate[0], 1.0, 0.0, 0.0);
  matrix.Rotate(landRotate[1], 0.0, 1.0, 0.0);
  matrix.Rotate(landRotate[2], 0.0, 0.0, 1.0);
  matrix.Scale(landScale[0], landScale[1], landScale[2]);

  // upload m to the GPU
  float m[16]; // column-major
  matrix.SetMatrixMode(OpenGLMatrix::ModelView);
  matrix.GetMatrix(m);

  float p[16]; // column-major
  matrix.SetMatrixMode(OpenGLMatrix::Projection);
  matrix.GetMatrix(p);

  // bind shader
  pipelineProgram->Bind();

  // set variable
  pipelineProgram->SetModelViewMatrix(m);
  pipelineProgram->SetProjectionMatrix(p);

  // render
  switch (rendermode)
  {
  case RenderMode::Points:
      glBindVertexArray(pointVao);
      glDrawArrays(GL_POINTS, 0, numPointVerts);
  break;

  case RenderMode::Lines:
      glBindVertexArray(lineVao);
      glDrawArrays(GL_LINES, 0, numLineVerts);
  break;

  case RenderMode::SolidTriangles:
      glBindVertexArray(triVao);
      glDrawArrays(GL_TRIANGLES, 0, numTriVerts);
  break;

  case RenderMode::SmoothenedTriangles:
      glBindVertexArray(smoTriVao);
      glDrawArrays(GL_TRIANGLES, 0, numTriVerts);
  break;
  }

  glutSwapBuffers();
}

void idleFunc()
{
  // make the screen update 
  glutPostRedisplay();
}

void reshapeFunc(int w, int h)
{
  glViewport(0, 0, w, h);

  matrix.SetMatrixMode(OpenGLMatrix::Projection);
  matrix.LoadIdentity();
  matrix.Perspective(54.0f, (float)w / (float)h, 0.01f, 1000.0f);
  matrix.SetMatrixMode(OpenGLMatrix::ModelView);
}

void mouseMotionDragFunc(int x, int y)
{
  // mouse has moved and one of the mouse buttons is pressed (dragging)

  // the change in mouse position since the last invocation of this function
  int mousePosDelta[2] = { x - mousePos[0], y - mousePos[1] };

  switch (controlState)
  {
    // translate the landscape
    case TRANSLATE:
      if (leftMouseButton)
      {
        // control x,y translation via the left mouse button
        landTranslate[0] += mousePosDelta[0] * 0.01f;
        landTranslate[1] -= mousePosDelta[1] * 0.01f;
      }
      if (middleMouseButton)
      {
        // control z translation via the middle mouse button
        landTranslate[2] += mousePosDelta[1] * 0.01f;
      }
      break;

    // rotate the landscape
    case ROTATE:
      if (leftMouseButton)
      {
        // control x,y rotation via the left mouse button
        landRotate[0] += mousePosDelta[1];
        landRotate[1] += mousePosDelta[0];
      }
      if (middleMouseButton)
      {
        // control z rotation via the middle mouse button
        landRotate[2] += mousePosDelta[1];
      }
      break;

    // scale the landscape
    case SCALE:
      if (leftMouseButton)
      {
        // control x,y scaling via the left mouse button
        landScale[0] *= 1.0f + mousePosDelta[0] * 0.01f;
        landScale[1] *= 1.0f - mousePosDelta[1] * 0.01f;
      }
      if (middleMouseButton)
      {
        // control z scaling via the middle mouse button
        landScale[2] *= 1.0f - mousePosDelta[1] * 0.01f;
      }
      break;
  }

  // store the new mouse position
  mousePos[0] = x;
  mousePos[1] = y;
}

void mouseMotionFunc(int x, int y)
{
  // mouse has moved
  // store the new mouse position
  mousePos[0] = x;
  mousePos[1] = y;
}

void mouseButtonFunc(int button, int state, int x, int y)
{
  // a mouse button has has been pressed or depressed

  // keep track of the mouse button state, in leftMouseButton, middleMouseButton, rightMouseButton variables
  switch (button)
  {
    case GLUT_LEFT_BUTTON:
      leftMouseButton = (state == GLUT_DOWN);
    break;

    case GLUT_MIDDLE_BUTTON:
      middleMouseButton = (state == GLUT_DOWN);
    break;

    case GLUT_RIGHT_BUTTON:
      rightMouseButton = (state == GLUT_DOWN);
    break;
  }

  // keep track of whether CTRL and SHIFT keys are pressed
  switch (glutGetModifiers())
  {
    case GLUT_ACTIVE_CTRL:
      controlState = TRANSLATE;
    break;

    case GLUT_ACTIVE_SHIFT:
      controlState = SCALE;
    break;

    // if CTRL and SHIFT are not pressed, we are in rotate mode
    default:
      controlState = ROTATE;
    break;
  }

  // store the new mouse position
  mousePos[0] = x;
  mousePos[1] = y;
}

void keyboardFunc(unsigned char key, int x, int y)
{
  GLint location = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "mode");
  switch (key)
  {
    case 27: // ESC key
      exit(0); // exit the program
    break;

    case ' ':
      cout << "You pressed the spacebar." << endl;
    break;

    case 'x':
      // take a screenshot
      saveScreenshot("screenshot.jpg");
    break;

    case '1':
        rendermode = RenderMode::Points;
        glUniform1i(location, 0);
    break;

    case '2':
        rendermode = RenderMode::Lines;
        glUniform1i(location, 0);
    break;

    case '3':
        rendermode = RenderMode::SolidTriangles;
        glUniform1i(location, 0);
    break;

    case '4':
        rendermode = RenderMode::SmoothenedTriangles;
        glUniform1i(location, 1);
    break;
  }
}

// initialization helper functions
void InitPoints()
{
    // push data
    for (int i = 0; i < imgHeight; ++i) { // for each row
        for (int j = 0; j < imgWidth; ++j) { // for each col
            // fill in the pointVerts vector
            pointVerts.push_back((float)i); // Z
            pointVerts.push_back((float)heightmapImage->getPixel(i, j, 0) * heightscale); // Y
            pointVerts.push_back(-(float)j); // X
            // fill in the pointCol vector
            pointCol.push_back(((float)heightmapImage->getPixel(i, j, 0)) / 255.0f); // R
            pointCol.push_back(((float)heightmapImage->getPixel(i, j, 0)) / 255.0f); // G
            pointCol.push_back(((float)heightmapImage->getPixel(i, j, 0)) / 255.0f); // B
            pointCol.push_back(1.0f); // Alpha
        }
    }
    numPointVerts = (int)(pointVerts.size()) / 3;
    // bind vao
    glGenVertexArrays(1, &pointVao);
    glBindVertexArray(pointVao);
    // for position
    // bind vbo
    glGenBuffers(1, &pointPosVbo);
    glBindBuffer(GL_ARRAY_BUFFER, pointPosVbo);
    glBufferData(GL_ARRAY_BUFFER, pointVerts.size() * sizeof(float), pointVerts.data(), GL_STATIC_DRAW);
    // get location index of the “position” shader variable
    GLuint loc = glGetAttribLocation(pipelineProgram->GetProgramHandle(), "position");
    glEnableVertexAttribArray(loc); // enable the “position” attribute
    // set the layout of the “position” attribute data
    const void* offset = (const void*)0;
    GLsizei stride = 0;
    GLboolean normalized = GL_FALSE;
    glVertexAttribPointer(loc, 3, GL_FLOAT, normalized, stride, offset);
    // for color
    // bind vbo
    glGenBuffers(1, &pointColVbo);
    glBindBuffer(GL_ARRAY_BUFFER, pointColVbo);
    glBufferData(GL_ARRAY_BUFFER, pointCol.size() * sizeof(float), pointCol.data(), GL_STATIC_DRAW);
    // get the location index of the “color” shader variable
    loc = glGetAttribLocation(pipelineProgram->GetProgramHandle(), "color");
    glEnableVertexAttribArray(loc); // enable the “color” attribute
    // set the layout of the “color” attribute data
    glVertexAttribPointer(loc, 4, GL_FLOAT, normalized, stride, offset);
    glBindVertexArray(0); // unbind the VAO
}

void InitLines()
{
    // horizontal line
    for (int i = 0; i < imgHeight; ++i) { // for each row
        for (int j = 0; j < imgWidth - 1; ++j) { // for each col; -1 to handle the last col in the row
            // fill in the lineVerts vector
            lineVerts.push_back((float)i); // Z
            lineVerts.push_back((float)heightmapImage->getPixel(i, j, 0) * heightscale); // Y
            lineVerts.push_back(-(float)j); // X
            // and its next neighbor lineVerts vector
            lineVerts.push_back((float)(i)); // Z
            lineVerts.push_back((float)heightmapImage->getPixel(i, j + 1, 0) * heightscale); // Y
            lineVerts.push_back(-(float)(j + 1)); // X + 1
            // fill in the lineCol vector
            lineCol.push_back(((float)heightmapImage->getPixel(i, j, 0)) / 255.0f); // R
            lineCol.push_back(((float)heightmapImage->getPixel(i, j, 0)) / 255.0f); // G
            lineCol.push_back(((float)heightmapImage->getPixel(i, j, 0)) / 255.0f); // B
            lineCol.push_back(1.0f); // Alpha
            // and its next neighbor lineCol vector
            lineCol.push_back(((float)heightmapImage->getPixel(i, j+1, 0)) / 255.0f); // R
            lineCol.push_back(((float)heightmapImage->getPixel(i, j+1, 0)) / 255.0f); // G
            lineCol.push_back(((float)heightmapImage->getPixel(i, j+1, 0)) / 255.0f); // B
            lineCol.push_back(1.0f); // Alpha
        }
    }
    // vertical line
    for (int j = 0; j < imgWidth; ++j) { // for each col
        for (int i = 0; i < imgHeight - 1; ++i) { // for each row; -1 to handle the last row in the col
            // fill in the lineVerts vector
            lineVerts.push_back((float)i); // Z
            lineVerts.push_back((float)heightmapImage->getPixel(i, j, 0) * heightscale); // Y
            lineVerts.push_back(-(float)j); // X
            // and its next neighbor lineVerts vector
            lineVerts.push_back((float)(i + 1)); // Z + 1
            lineVerts.push_back((float)heightmapImage->getPixel(i + 1, j, 0) * heightscale); // Y
            lineVerts.push_back(-(float)(j)); // X
            // fill in the lineCol vector
            lineCol.push_back(((float)heightmapImage->getPixel(i, j, 0)) / 255.0f); // R
            lineCol.push_back(((float)heightmapImage->getPixel(i, j, 0)) / 255.0f); // G
            lineCol.push_back(((float)heightmapImage->getPixel(i, j, 0)) / 255.0f); // B
            lineCol.push_back(1.0f); // Alpha
            // and its next neighbor lineCol vector
            lineCol.push_back(((float)heightmapImage->getPixel(i + 1, j, 0)) / 255.0f); // R
            lineCol.push_back(((float)heightmapImage->getPixel(i + 1, j, 0)) / 255.0f); // G
            lineCol.push_back(((float)heightmapImage->getPixel(i + 1, j, 0)) / 255.0f); // B
            lineCol.push_back(1.0f); // Alpha
        }
    }
    numLineVerts = (int)(lineVerts.size()) / 3;
    // bind vao
    glGenVertexArrays(1, &lineVao);
    glBindVertexArray(lineVao);
    // for position
    // bind vbo
    glGenBuffers(1, &linePosVbo);
    glBindBuffer(GL_ARRAY_BUFFER, linePosVbo);
    glBufferData(GL_ARRAY_BUFFER, lineVerts.size() * sizeof(float), lineVerts.data(), GL_STATIC_DRAW);
    // get location index of the “position” shader variable
    GLuint loc = glGetAttribLocation(pipelineProgram->GetProgramHandle(), "position");
    glEnableVertexAttribArray(loc); // enable the “position” attribute
    // set the layout of the “position” attribute data
    const void* offset = (const void*)(0);
    GLsizei stride = 0;
    GLboolean normalized = GL_FALSE;
    glVertexAttribPointer(loc, 3, GL_FLOAT, normalized, stride, offset);
    // for color
    // bind vbo
    glGenBuffers(1, &lineColVbo);
    glBindBuffer(GL_ARRAY_BUFFER, lineColVbo);
    glBufferData(GL_ARRAY_BUFFER, lineCol.size() * sizeof(float), lineCol.data(), GL_STATIC_DRAW);
    // get the location index of the “color” shader variable
    loc = glGetAttribLocation(pipelineProgram->GetProgramHandle(), "color");
    glEnableVertexAttribArray(loc); // enable the “color” attribute
    // set the layout of the “color” attribute data
    glVertexAttribPointer(loc, 4, GL_FLOAT, normalized, stride, offset);
    glBindVertexArray(0); // unbind the VAO
}

void InitTri()
{
    // triangle (i,j), (i,j+1), (i+1,j+1)
    for (int i = 0; i < imgHeight - 1; ++i) { // for each row
        for (int j = 0; j < imgWidth - 1; ++j) { // for each col
            // fill in the triVerts vector: (i,j)
            triVerts.push_back((float)i); // Z
            triVerts.push_back((float)heightmapImage->getPixel(i, j, 0) * heightscale); // Y
            triVerts.push_back(-(float)j); // X
            // fill in the triVerts vector: (i,j+1)
            triVerts.push_back((float)i); // Z
            triVerts.push_back((float)heightmapImage->getPixel(i, j+1, 0) * heightscale); // Y
            triVerts.push_back(-(float)(j+1)); // X + 1
            // fill in the triVerts vector: (i+1,j+1)
            triVerts.push_back((float)i+1); // Z + 1
            triVerts.push_back((float)heightmapImage->getPixel(i+1, j+1, 0) * heightscale); // Y
            triVerts.push_back(-(float)(j+1)); // X + 1
            // fill in the triCol vector: (i,j)
            triCol.push_back(((float)heightmapImage->getPixel(i, j, 0)) / 255.0f); // R
            triCol.push_back(((float)heightmapImage->getPixel(i, j, 0)) / 255.0f); // G
            triCol.push_back(((float)heightmapImage->getPixel(i, j, 0)) / 255.0f); // B
            triCol.push_back(1.0f); // Alpha
            // fill in the triCol vector: (i,j+1)
            triCol.push_back(((float)heightmapImage->getPixel(i, j+1, 0)) / 255.0f); // R
            triCol.push_back(((float)heightmapImage->getPixel(i, j+1, 0)) / 255.0f); // G
            triCol.push_back(((float)heightmapImage->getPixel(i, j+1, 0)) / 255.0f); // B
            triCol.push_back(1.0f); // Alpha
            // fill in the triCol vector: (i+1,j+1)
            triCol.push_back(((float)heightmapImage->getPixel(i+1, j + 1, 0)) / 255.0f); // R
            triCol.push_back(((float)heightmapImage->getPixel(i+1, j + 1, 0)) / 255.0f); // G
            triCol.push_back(((float)heightmapImage->getPixel(i+1, j + 1, 0)) / 255.0f); // B
            triCol.push_back(1.0f); // Alpha
        }
    }
    // triangle (i,j), (i+1,j), (i+1,j+1)
    for (int i = 0; i < imgHeight - 1; ++i) { // for each row
        for (int j = 0; j < imgWidth - 1; ++j) { // for each col
            // fill in the triVerts vector: (i,j)
            triVerts.push_back((float)i); // Z
            triVerts.push_back((float)heightmapImage->getPixel(i, j, 0) * heightscale); // Y
            triVerts.push_back(-(float)j); // X
            // fill in the triVerts vector: (i+1,j)
            triVerts.push_back((float)(i + 1)); // Z + 1
            triVerts.push_back((float)heightmapImage->getPixel(i + 1, j, 0) * heightscale); // Y
            triVerts.push_back(-(float)j); // X
            // fill in the triVerts vector: (i+1,j+1)
            triVerts.push_back((float)(i + 1)); // Z + 1
            triVerts.push_back((float)heightmapImage->getPixel(i + 1, j + 1, 0) * heightscale); // Y
            triVerts.push_back(-(float)(j + 1)); // X + 1
            // fill in the triCol vector: (i,j)
            triCol.push_back(((float)heightmapImage->getPixel(i, j, 0)) / 255.0f); // R
            triCol.push_back(((float)heightmapImage->getPixel(i, j, 0)) / 255.0f); // G
            triCol.push_back(((float)heightmapImage->getPixel(i, j, 0)) / 255.0f); // B
            triCol.push_back(1.0f); // Alpha
            // fill in the triCol vector: (i+1,j)
            triCol.push_back(((float)heightmapImage->getPixel(i + 1, j, 0)) / 255.0f); // R
            triCol.push_back(((float)heightmapImage->getPixel(i + 1, j, 0)) / 255.0f); // G
            triCol.push_back(((float)heightmapImage->getPixel(i + 1, j, 0)) / 255.0f); // B
            triCol.push_back(1.0f); // Alpha
            // fill in the triCol vector: (i+1,j+1)
            triCol.push_back(((float)heightmapImage->getPixel(i + 1, j + 1, 0)) / 255.0f); // R
            triCol.push_back(((float)heightmapImage->getPixel(i + 1, j + 1, 0)) / 255.0f); // G
            triCol.push_back(((float)heightmapImage->getPixel(i + 1, j + 1, 0)) / 255.0f); // B
            triCol.push_back(1.0f); // Alpha
        }
    }

    numTriVerts = (int)(triVerts.size()) / 3;
    // bind vao
    glGenVertexArrays(1, &triVao);
    glBindVertexArray(triVao);
    // for position
    // bind vbo
    glGenBuffers(1, &triPosVbo);
    glBindBuffer(GL_ARRAY_BUFFER, triPosVbo);
    glBufferData(GL_ARRAY_BUFFER, triVerts.size() * sizeof(float), triVerts.data(), GL_STATIC_DRAW);
    // get location index of the “position” shader variable
    GLuint loc = glGetAttribLocation(pipelineProgram->GetProgramHandle(), "position");
    glEnableVertexAttribArray(loc); // enable the “position” attribute
    // set the layout of the “position” attribute data
    const void* offset = (const void*)(0);
    GLsizei stride = 0;
    GLboolean normalized = GL_FALSE;
    glVertexAttribPointer(loc, 3, GL_FLOAT, normalized, stride, offset);
    // for color
    // bind vbo
    glGenBuffers(1, &triColVbo);
    glBindBuffer(GL_ARRAY_BUFFER, triColVbo);
    glBufferData(GL_ARRAY_BUFFER, triCol.size() * sizeof(float), triCol.data(), GL_STATIC_DRAW);
    // get the location index of the “color” shader variable
    loc = glGetAttribLocation(pipelineProgram->GetProgramHandle(), "color");
    glEnableVertexAttribArray(loc); // enable the “color” attribute
    // set the layout of the “color” attribute data
    glVertexAttribPointer(loc, 4, GL_FLOAT, normalized, stride, offset);
    glBindVertexArray(0); // unbind the VAO
}

void setSmoVerts(int i, int j)
{
    // left
    if (i == 0) {
        leftSmoTriVerts.push_back((float)i+1); // Z
        leftSmoTriVerts.push_back((float)heightmapImage->getPixel(i+1, j, 0) * heightscale); // Y
        leftSmoTriVerts.push_back(-(float)j); // X
    }
    else {
        leftSmoTriVerts.push_back((float)i-1); // Z
        leftSmoTriVerts.push_back((float)heightmapImage->getPixel(i-1, j, 0) * heightscale); // Y
        leftSmoTriVerts.push_back(-(float)j); // X
    }
    // right
    if (i == imgWidth - 1) {
        rightSmoTriVerts.push_back((float)i - 1); // Z
        rightSmoTriVerts.push_back((float)heightmapImage->getPixel(i - 1, j, 0) * heightscale); // Y
        rightSmoTriVerts.push_back(-(float)j); // X
    }
    else {
        rightSmoTriVerts.push_back((float)i + 1); // Z
        rightSmoTriVerts.push_back((float)heightmapImage->getPixel(i + 1, j, 0) * heightscale); // Y
        rightSmoTriVerts.push_back(-(float)j); // X
    }

    // up
    if (j == imgHeight - 1) {
        upSmoTriVerts.push_back((float)i); // Z
        upSmoTriVerts.push_back((float)heightmapImage->getPixel(i, j - 1, 0) * heightscale); // Y
        upSmoTriVerts.push_back(-(float)(j - 1)); // X
    }
    else {
        upSmoTriVerts.push_back((float)i); // Z
        upSmoTriVerts.push_back((float)heightmapImage->getPixel(i, j + 1, 0) * heightscale); // Y
        upSmoTriVerts.push_back(-(float)(j + 1)); // X
    }

    // down
    if (j == 0) {
        downSmoTriVerts.push_back((float)i); // Z
        downSmoTriVerts.push_back((float)heightmapImage->getPixel(i, j+1, 0) * heightscale); // Y
        downSmoTriVerts.push_back(-(float)(j+1)); // X
    }
    else {
        downSmoTriVerts.push_back((float)i); // Z
        downSmoTriVerts.push_back((float)heightmapImage->getPixel(i, j-1, 0) * heightscale); // Y
        downSmoTriVerts.push_back(-(float)(j-1)); // X
    }
}

void initSmoTri()
{
    // triangle (i,j), (i,j+1), (i+1,j+1)
    for (int i = 0; i < (imgHeight - 1); ++i) { // for each row
        for (int j = 0; j < (imgWidth - 1); ++j) { // for each col
            setSmoVerts(i, j);
            setSmoVerts(i, j + 1);
            setSmoVerts(i + 1, j + 1);
        }
    }
    // triangle (i,j), (i+1,j), (i+1,j+1)
    for (int i = 0; i < (imgHeight - 1); ++i) { // for each row
        for (int j = 0; j < (imgWidth - 1); ++j) { // for each col
            setSmoVerts(i, j);
            setSmoVerts(i+1, j);
            setSmoVerts(i + 1, j + 1);
        }
    }
    
    // bind vao
    glGenVertexArrays(1, &smoTriVao);
    glBindVertexArray(smoTriVao);

    const void* offset = (const void*)(0);
    GLsizei stride = 0;
    GLboolean normalized = GL_FALSE;
    // bind vbo
    // get location index of the “position” shader variable
    // set the layout of the “position” attribute data

    // left
    glGenBuffers(1, &smoTriVboLeft);
    glBindBuffer(GL_ARRAY_BUFFER, smoTriVboLeft);
    glBufferData(GL_ARRAY_BUFFER, leftSmoTriVerts.size() * sizeof(float), leftSmoTriVerts.data(), GL_STATIC_DRAW);
    GLuint loc = glGetAttribLocation(pipelineProgram->GetProgramHandle(), "p_left");
    glEnableVertexAttribArray(loc); // enable the “position” attribute
    glVertexAttribPointer(loc, 3, GL_FLOAT, normalized, stride, offset);
    // right
    glGenBuffers(1, &smoTriVboRight);
    glBindBuffer(GL_ARRAY_BUFFER, smoTriVboRight);
    glBufferData(GL_ARRAY_BUFFER, rightSmoTriVerts.size() * sizeof(float), rightSmoTriVerts.data(), GL_STATIC_DRAW);
    loc = glGetAttribLocation(pipelineProgram->GetProgramHandle(), "p_right");
    glEnableVertexAttribArray(loc); // enable the “position” attribute
    glVertexAttribPointer(loc, 3, GL_FLOAT, normalized, stride, offset);
    // up
    glGenBuffers(1, &smoTriVboUp);
    glBindBuffer(GL_ARRAY_BUFFER, smoTriVboUp);
    glBufferData(GL_ARRAY_BUFFER, upSmoTriVerts.size() * sizeof(float), upSmoTriVerts.data(), GL_STATIC_DRAW);
    loc = glGetAttribLocation(pipelineProgram->GetProgramHandle(), "p_up");
    glEnableVertexAttribArray(loc); // enable the “position” attribute
    glVertexAttribPointer(loc, 3, GL_FLOAT, normalized, stride, offset);
    // down
    glGenBuffers(1, &smoTriVboDown);
    glBindBuffer(GL_ARRAY_BUFFER, smoTriVboDown);
    glBufferData(GL_ARRAY_BUFFER, downSmoTriVerts.size() * sizeof(float), downSmoTriVerts.data(), GL_STATIC_DRAW);
    loc = glGetAttribLocation(pipelineProgram->GetProgramHandle(), "p_down");
    glEnableVertexAttribArray(loc); // enable the “position” attribute
    glVertexAttribPointer(loc, 3, GL_FLOAT, normalized, stride, offset);
    // position
    glGenBuffers(1, &triPosVbo);
    glBindBuffer(GL_ARRAY_BUFFER, triPosVbo);
    glBufferData(GL_ARRAY_BUFFER, triVerts.size() * sizeof(float), triVerts.data(), GL_STATIC_DRAW);
    loc = glGetAttribLocation(pipelineProgram->GetProgramHandle(), "position");
    glEnableVertexAttribArray(loc); // enable the “position” attribute
    glVertexAttribPointer(loc, 3, GL_FLOAT, normalized, stride, offset);
    // color
    glGenBuffers(1, &triColVbo);
    glBindBuffer(GL_ARRAY_BUFFER, triColVbo);
    glBufferData(GL_ARRAY_BUFFER, triCol.size() * sizeof(float), triCol.data(), GL_STATIC_DRAW);
    loc = glGetAttribLocation(pipelineProgram->GetProgramHandle(), "color");
    glEnableVertexAttribArray(loc); // enable the “color” attribute
    glVertexAttribPointer(loc, 4, GL_FLOAT, normalized, stride, offset);

    glBindVertexArray(0); // unbind the VAO
}

void initScene(int argc, char *argv[])
{
  // load the image from a jpeg disk file to main memory
  heightmapImage = new ImageIO();
  if (heightmapImage->loadJPEG(argv[1]) != ImageIO::OK)
  {
    cout << "Error reading image " << argv[1] << "." << endl;
    exit(EXIT_FAILURE);
  }

  imgWidth = heightmapImage->getWidth();
  imgHeight = heightmapImage->getHeight();

  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

  pipelineProgram = new BasicPipelineProgram;
  int ret = pipelineProgram->Init(shaderBasePath);
  if (ret != 0) abort();

  InitPoints();
  InitLines();
  InitTri();
  initSmoTri();

  glEnable(GL_DEPTH_TEST);

  std::cout << "GL error: " << glGetError() << std::endl;
}

int main(int argc, char *argv[])
{
  if (argc != 2)
  {
    cout << "The arguments are incorrect." << endl;
    cout << "usage: ./hw1 <heightmap file>" << endl;
    exit(EXIT_FAILURE);
  }

  cout << "Initializing GLUT..." << endl;
  glutInit(&argc,argv);

  cout << "Initializing OpenGL..." << endl;

  #ifdef __APPLE__
    glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
  #else
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
  #endif

  glutInitWindowSize(windowWidth, windowHeight);
  glutInitWindowPosition(0, 0);  
  glutCreateWindow(windowTitle);

  cout << "OpenGL Version: " << glGetString(GL_VERSION) << endl;
  cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << endl;
  cout << "Shading Language Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;

  #ifdef __APPLE__
    // This is needed on recent Mac OS X versions to correctly display the window.
    glutReshapeWindow(windowWidth - 1, windowHeight - 1);
  #endif

  // tells glut to use a particular display function to redraw 
  glutDisplayFunc(displayFunc);
  // perform animation inside idleFunc
  glutIdleFunc(idleFunc);
  // callback for mouse drags
  glutMotionFunc(mouseMotionDragFunc);
  // callback for idle mouse movement
  glutPassiveMotionFunc(mouseMotionFunc);
  // callback for mouse button changes
  glutMouseFunc(mouseButtonFunc);
  // callback for resizing the window
  glutReshapeFunc(reshapeFunc);
  // callback for pressing the keys on the keyboard
  glutKeyboardFunc(keyboardFunc);

  // init glew
  #ifdef __APPLE__
    // nothing is needed on Apple
  #else
    // Windows, Linux
    GLint result = glewInit();
    if (result != GLEW_OK)
    {
      cout << "error: " << glewGetErrorString(result) << endl;
      exit(EXIT_FAILURE);
    }
  #endif

  // do initialization
  initScene(argc, argv);

  // sink forever into the glut loop
  glutMainLoop();
}


