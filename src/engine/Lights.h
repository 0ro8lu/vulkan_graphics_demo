#ifndef _LIGHTS_H_
#define _LIGHTS_H_

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

// struct PointLightBuffer {
//   alignas(16) glm::vec4 position;
//   alignas(16) glm::vec4 color;
// };

// struct PointLight
// {
// public:
//   PointLight(glm::vec4 position, glm::vec4 color) {
//     // calculate lightTransform
//     pointLightBuffer = { position, color };

//     glm::mat4 lightProjection = glm::perspective(glm::radians(45.0f), 1.0f,
//     0.1f, 100.0f); glm::vec3 lightPos = {pointLightBuffer.position.x,
//     pointLightBuffer.position.y, pointLightBuffer.position.z};

//     std::cout << lightPos.x << " " << lightPos.y << " " << lightPos.z <<
//     std::endl; glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0),
//     glm::vec3(0, 1, 0));

//     lightSpaceMatrix = lightProjection * lightView;
//   }

//   PointLightBuffer pointLightBuffer;
//   glm::mat4 lightSpaceMatrix;
// };

struct PointLight
{
  alignas(16) glm::vec4 position;
  alignas(16) glm::vec4 color;
};

struct DirectionalLight
{
  struct DirectionalLightBuffer
  {
    alignas(16) glm::vec4 direction;
    alignas(16) glm::vec4 color;
  };

  struct DirectionalLightTransfrom
  {
    alignas(16) glm::mat4 lightSpaceMatrix;
  };

  DirectionalLight(glm::vec4 direction, glm::vec4 color);

  DirectionalLightBuffer directionalLightBuffer;
  DirectionalLightTransfrom directionalLightTransform;
};

struct SpotLight
{
  alignas(16) glm::vec4 direction;
  alignas(16) glm::vec4 position;
  alignas(16) glm::vec4 color;
};

#endif
