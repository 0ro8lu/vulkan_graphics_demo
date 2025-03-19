#include "engine/LightManager.h"
#include "engine/Lights.h"

#include <iostream>

uint32_t LightManager::currentShadowIndex = 0;
const uint32_t LightManager::maximumAtlasTiles = ATLAS_TILES * ATLAS_TILES;

DirectionalLight*
LightManager::createDirectionalLight(glm::vec3 direction,
                                     glm::vec3 color,
                                     bool castsShadow)
{
  return new DirectionalLight(direction, color, castsShadow);
}

PointLight
LightManager::createPointLight(glm::vec3 position,
                               glm::vec3 color,
                               bool castsShadow)
{
  if (castsShadow) {
    if (currentShadowIndex + 6 <= maximumAtlasTiles) {
      currentShadowIndex += 6;
    } else {
      std::cout << "Out of available slots for shadow maps "
                << maximumAtlasTiles
                << " disabling light's shadow casting capabilities"
                << std::endl;
      castsShadow = false;
    }
  }

  auto pointLight =
    PointLight(position, color, currentShadowIndex - 6, castsShadow);

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
    if (currentShadowIndex + 1 <= maximumAtlasTiles) {
      currentShadowIndex += 1;
    } else {
      std::cout << "Out of available slots for shadow maps "
                << maximumAtlasTiles
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
                             currentShadowIndex - 1,
                             castsShadow);

  return spotLight;
}
