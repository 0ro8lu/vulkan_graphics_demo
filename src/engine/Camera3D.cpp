#include "Camera3D.h"

Camera3D::Camera3D(glm::vec3 cameraPos, glm::vec3 cameraFront) :
	cameraPos(cameraPos), cameraFront(cameraFront), needsUpdating(true), cameraMatrix(1.0f) {}

void Camera3D::update() {
	if(!needsUpdating)
		return;

	glm::vec3 direction;
    direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction.y = sin(glm::radians(pitch));
    direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(direction);

	cameraMatrix = glm::lookAt(cameraPos, cameraPos + cameraFront, glm::vec3(0.0f, 1.0f, 0.0f));
	needsUpdating = false;
}