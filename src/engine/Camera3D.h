#ifndef _CAMERA3D_H_
#define _CAMERA3D_H_

#include <ext/matrix_clip_space.hpp>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

class Camera3D
{
public:
  Camera3D(glm::vec3 cameraPos, glm::vec3 cameraFront);
  Camera3D(glm::vec3 cameraPos,
           glm::vec3 cameraFront,
           uint32_t width,
           uint32_t height);

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

  void resizeCamera(uint32_t width, uint32_t height);

  void update();

  const glm::mat4& getCameraMatrix() const { return cameraMatrix; }
  const glm::mat4& getCameraProjectionMatrix() const
  {
    return cameraProjection;
  }

  const glm::vec3& getCameraPos() const { return cameraPos; }
  const glm::vec3& getCameraFront() const { return cameraFront; }

  float pitch;
  float yaw = -90.0f;

private:
  glm::mat4 cameraMatrix;
  glm::mat4 cameraProjection;

  glm::vec3 cameraPos;
  glm::vec3 cameraFront;

  bool needsUpdating;
};

#endif
