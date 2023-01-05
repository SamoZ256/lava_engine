#ifndef MATH_H
#define MATH_H

#include "lvutils/libraries/glm.hpp"

void decomposeModel(const glm::mat4& model, glm::vec3& position, glm::vec3& rotation, glm::vec3& scale);

float lerp(float a, float b, float f);

float getLengthSquared(glm::vec3 a);

#endif
