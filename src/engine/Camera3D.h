#ifndef _CAMERA3D_H_
#define _CAMERA3D_H_

#include <ext/matrix_clip_space.hpp>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

class Camera3D
{
public:
  Camera3D(glm::vec3 cameraPos, glm::vec3 cameraFront);

  void setPos(glm::vec3 newPosition)
  {
    cameraPos = newPosition;
    needsUpdating = true;
  }
  void setDirection(glm::vec3 newDirection)
  {
    cameraFront = newDirection;
    needsUpdating = true;
  }

  void update();

  const glm::mat4& getCameraMatrix() const { return cameraMatrix; }

  const glm::vec3& getCameraPos() const { return cameraPos; }
  const glm::vec3& getCameraFront() const { return cameraFront; }

  float pitch;
  float yaw = -90.0f;

private:
  glm::mat4 cameraMatrix;

  glm::vec3 cameraPos;
  glm::vec3 cameraFront;

  bool needsUpdating;
};

#endif
