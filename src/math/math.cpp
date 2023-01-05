#include "math.hpp"

void decomposeModel(const glm::mat4& model, glm::vec3& position, glm::vec3& rotation, glm::vec3& scale) {
	//Position
	position = model[3];

	//Scale
	for(int i = 0; i < 3; i++)
		scale[i] = glm::length(glm::vec3(model[i]));

	//Rotation
	const glm::mat3 rotMtx(
		glm::vec3(model[0]) / scale[0],
		glm::vec3(model[1]) / scale[1],
		glm::vec3(model[2]) / scale[2]);
	rotation = glm::degrees(glm::eulerAngles(glm::quat_cast(rotMtx)));
}

float lerp(float a, float b, float f) {
  	return a + f * (b - a);
}

float getLengthSquared(glm::vec3 a) {
  	return a.x * a.x + a.y * a.y + a.z * a.z;
}
