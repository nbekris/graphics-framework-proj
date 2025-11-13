////////////////////////////////////////////////////////////////////////
// A small library of 4x4 matrix operations needed for graphics
// transformations.  glm::mat4 is a 4x4 float matrix class with indexing
// and printing methods.  A small list or procedures are supplied to
// create Rotate, Scale, Translate, and Perspective matrices and to
// return the product of any two such.

#include <glm/glm.hpp>

#include "math.h"
#include "transform.h"

float* Pntr(glm::mat4& M)
{
    return &(M[0][0]);
}

//@@ The following procedures should calculate and return 4x4
//transformation matrices instead of the identity.

// Return a rotation matrix around an axis (0:X, 1:Y, 2:Z) by an angle
// measured in degrees.  NOTE: Make sure to convert degrees to radians
// before using sin and cos.  HINT: radians = degrees*PI/180
const float pi = 3.14159f;
glm::mat4 Rotate(const int i, const float theta)
{
    const float radians = theta * (pi / 180);

    glm::mat4 R(1.0);

    const int j = (i + 1) % 3;
    const int k = (i + 2) % 3;

    R[j][j] = cos(radians);
    R[k][k] = cos(radians);
    //R[][] = 1;
    R[k][j] = -1 * sin(radians);
    R[j][k] = sin(radians);

    return R;
}

// Return a scale matrix
glm::mat4 Scale(const float x, const float y, const float z)
{
    glm::mat4 S(1.0);

    S[0][0] = x;
    S[1][1] = y;
    S[2][2] = z;

    return S;
}

// Return a translation matrix
glm::mat4 Translate(const float x, const float y, const float z)
{
    glm::mat4 T(1.0);

    T[3][0] = x;
    T[3][1] = y;
    T[3][2] = z;

    return T;
}

// Returns a perspective projection matrix
glm::mat4 Perspective(const float rx, const float ry,
    const float front, const float back)
{
    glm::mat4 P(1.0);

    P[0][0] = 1 / rx;
    P[1][1] = 1 / ry;
    P[2][2] = -1 * (back + front) / (back - front);
    P[3][2] = -1 * (2 * front * back) / (back - front);
    P[2][3] = -1;
    P[3][3] = 0;

    return P;
}


