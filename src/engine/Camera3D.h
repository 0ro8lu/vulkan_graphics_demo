#pragma once

#include <glm/glm.hpp>

class Camera3D
{
public:
  Camera3D(glm::vec3 cameraPos, glm::vec3 cameraFront);

  void setPos(glm::vec3 newPosition)
  {
    m_cameraPos = newPosition;
    m_needsUpdating = true;
  }
  void setDirection(glm::vec3 newDirection)
  {
    m_cameraFront = newDirection;
    m_needsUpdating = true;
  }

  void resizeCamera(uint32_t width, uint32_t height);

  void update();

  const glm::mat4& getCameraMatrix() const { return m_cameraMatrix; }
  const glm::mat4& getCameraProjectionMatrix() const
  {
    return m_cameraProjection;
  }

  const glm::vec3& getCameraPos() const { return m_cameraPos; }
  const glm::vec3& getCameraFront() const { return m_cameraFront; }

  float m_pitch;
  float m_yaw = -90.0f;

private:
  glm::mat4 m_cameraMatrix;
  glm::mat4 m_cameraProjection;

  glm::vec3 m_cameraPos;
  glm::vec3 m_cameraFront;

  bool m_needsUpdating;
};

struct CameraBuffer
{
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
  alignas(16) glm::vec4 cameraPos;
};
