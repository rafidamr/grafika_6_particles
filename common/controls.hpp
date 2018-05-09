#ifndef CONTROLS_HPP
#define CONTROLS_HPP

void computeMatricesFromInputs();
glm::mat4 getViewMatrix();
glm::mat4 getProjectionMatrix();
GLfloat getAmbienceFactor();
GLfloat getDiffuseFactor();
GLfloat getSpecularFactor();
#endif