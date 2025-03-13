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
  auto pl = PointLight(position, color, currentShadowIndex, castsShadow);

  if (castsShadow) {
    currentShadowIndex += 6;
  }

  return pl;
}

SpotLight
LightManager::createSpotLight(glm::vec3 position,
                              glm::vec3 direction,
                              glm::vec3 color,
                              float innerCutoff,
                              float outerCutoff,
                              bool castsShadow)
{

  if (currentShadowIndex > maximumAtlasTiles && castsShadow) {
    std::cout << "Out of available slots for shadow maps " << maximumAtlasTiles
              << " disabling light's shadow casting capabilities" << std::endl;
    castsShadow = false;
  }

  auto sl = SpotLight(position,
                      direction,
                      color,
                      innerCutoff,
                      outerCutoff,
                      currentShadowIndex,
                      castsShadow);

  if (castsShadow) {
    currentShadowIndex += 1;
  }

  return sl;
}
