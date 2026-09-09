#pragma once

#include "engine/Lights.h"

// TODO: move these constants into CMAKE
const uint32_t MAX_POINT_LIGHTS = 5;
const uint32_t MAX_SPOT_LIGHTS = 2;

class LightManager
{
public:
  static DirectionalLight createDirectionalLight(glm::vec3 direction,
                                                 glm::vec3 color,
                                                 bool castsShadow);
  static PointLight createPointLight(glm::vec3 position,
                                     glm::vec3 color,
                                     bool castsShadow);
  static SpotLight createSpotLight(glm::vec3 position,
                                   glm::vec3 direction,
                                   glm::vec3 color,
                                   float innerCutoff,
                                   float outerCutoff,
                                   bool castsShadow);

private:
  static uint32_t currentShadowIndex;
  const static uint32_t maximumAtlasTiles;
};

