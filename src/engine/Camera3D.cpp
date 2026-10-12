#include "Camera3D.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>

Camera3D::Camera3D(glm::vec3 cameraPos, glm::vec3 cameraFront)
  : m_cameraPos(cameraPos)
  , m_cameraFront(cameraFront)
  , m_needsUpdating(true)
{
  m_cameraMatrix = glm::lookAt(
    cameraPos, cameraPos + cameraFront, glm::vec3(0.0f, 1.0f, 0.0f));
}

void
Camera3D::resizeCamera(uint32_t width, uint32_t height)
{
  m_cameraProjection = glm::perspective(
    glm::radians(45.0f), (float)width / (float)height, 0.1f, 100.0f);
}

void
Camera3D::update()
{
  if (!m_needsUpdating)
    return;

  glm::vec3 direction;
  direction.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
  direction.y = sin(glm::radians(m_pitch));
  direction.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
  m_cameraFront = glm::normalize(direction);

  m_cameraMatrix = glm::lookAt(
    m_cameraPos, m_cameraPos + m_cameraFront, glm::vec3(0.0f, 1.0f, 0.0f));
  m_needsUpdating = false;
}
