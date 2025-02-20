#include "engine/Lights.h"

DirectionalLight::DirectionalLight(glm::vec4 direction, glm::vec4 color)
  : directionalLightBuffer{ direction, color }
{
  glm::mat4 lightProjection =
    glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
  glm::vec3 lightPos = { directionalLightBuffer.direction.x,
                         directionalLightBuffer.direction.y,
                         directionalLightBuffer.direction.z };

  glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0), glm::vec3(0, 1, 0));

  directionalLightTransform = { lightProjection * lightView };
}
