#include "engine/LightManager.h"
#include "engine/Lights.h"

#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

uint32_t LightManager::s_currentShadowIndex = 0;
const uint32_t LightManager::s_maximumAtlasTiles =
  SPOT_POINT_ATLAS_TILES * SPOT_POINT_ATLAS_TILES;

DirectionalLight
LightManager::createDirectionalLight(glm::vec3 direction,
                                     glm::vec3 color,
                                     bool castsShadow)
{
  return DirectionalLight(direction, color, castsShadow);
}

PointLight
LightManager::createPointLight(glm::vec3 position,
                               glm::vec3 color,
                               bool castsShadow)
{
  if (castsShadow) {
    if (s_currentShadowIndex + 6 <= s_maximumAtlasTiles) {
      s_currentShadowIndex += 6;
    } else {
      std::cout << "Out of available slots for shadow maps "
                << s_maximumAtlasTiles
                << " disabling light's shadow casting capabilities"
                << std::endl;
      castsShadow = false;
    }
  }

  auto pointLight =
    PointLight(position, color, s_currentShadowIndex - 6, castsShadow);

  return pointLight;
}

SpotLight
LightManager::createSpotLight(glm::vec3 position,
                              glm::vec3 direction,
                              glm::vec3 color,
                              float innerCutoff,
                              float outerCutoff,
                              bool castsShadow)
{

  if (castsShadow) {
    if (s_currentShadowIndex + 1 <= s_maximumAtlasTiles) {
      s_currentShadowIndex += 1;
    } else {
      std::cout << "Out of available slots for shadow maps "
                << s_maximumAtlasTiles
                << " disabling light's shadow casting capabilities"
                << std::endl;
      castsShadow = false;
    }
  }

  auto spotLight = SpotLight(position,
                             direction,
                             color,
                             innerCutoff,
                             outerCutoff,
                             s_currentShadowIndex - 1,
                             castsShadow);

  return spotLight;
}
