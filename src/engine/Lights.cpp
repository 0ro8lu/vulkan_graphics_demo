#include "engine/Lights.h"

DirectionalLight::DirectionalLight(glm::vec4 direction, glm::vec4 color)
  : directionalLightBuffer{ direction, color }
{
  // float near_plane = 1.0f, far_plane = 9.5f;
  // glm::mat4 lightProjection =
  //   glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);
  glm::mat4 lightProjection =
    glm::perspective(glm::radians(45.0f), 1.0f, 1.0f, 96.0f);
  glm::vec3 lightPos = { -directionalLightBuffer.direction.x,
                         -directionalLightBuffer.direction.y,
                         -directionalLightBuffer.direction.z };

  glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0), glm::vec3(0, 1, 0));

  directionalLightTransform = { lightProjection * lightView };
}
