#pragma once

#include "glm/glm.hpp"

struct CameraUBO
{
	glm::mat4 proj;
	glm::mat4 view;
	glm::mat4 model;
	glm::mat4 lightSpaceMatrix;
};