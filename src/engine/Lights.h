#ifndef _LIGHTS_H_
#define _LIGHTS_H_

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

class LightManager;

struct DirectionalLight
{
  friend class LightManager;

  const glm::vec4& getDirection() const { return direction; }
  const glm::vec4& getColor() const { return color; }
  const glm::mat4& getTransform() const { return transform; }

  bool castsShadow() const { return color.w == 1.0; }

  // DirectionalLight(glm::vec4 direction,
  //                  glm::vec4 color,
  //                  bool castsShadow);

private:
  DirectionalLight(glm::vec4 direction, glm::vec4 color, bool castsShadow);

  alignas(16) glm::vec4 direction;
  alignas(16) glm::vec4 color;
  alignas(16) glm::mat4 transform;
};

struct PointLight
{
  friend class LightManager;

  const glm::vec4& getPosition() const { return position; }
  const glm::vec4& getColor() const { return color; }
  // const glm::mat4& getTransform() const { return transform; }

  bool castsShadow() const { return color.w == 1.0; }

private:
  PointLight(glm::vec4 position,
             glm::vec4 color,
             uint32_t shadowMapIndex,
             bool castsShadow);

  alignas(16) glm::vec4 position;
  alignas(16) glm::vec4 color;
  // alignas(16) glm::mat4 transform
  // alignas(16) glm::vec4 shadowAtlasCoordinates;
};

struct SpotLight
{
  friend class LightManager;

  const glm::vec4& getPosition() const { return position; }
  const glm::vec4& getDirection() const { return direction; }
  const glm::vec4& getColor() const { return color; }
  const glm::mat4& getTransform() const { return transform; }
  const glm::vec4& getAtlasCoordinatesPixel() const { return atlasCoordsPixel; }
  const glm::vec4& getAtlasCoordinatesNormalized() const
  {
    return atlasCoordsNormalized;
  }

  bool castsShadow() const { return color.w == 1.0; }

  void move(glm::vec4 position, glm::vec4 direction);

private:
  SpotLight(glm::vec4 position,
            glm::vec4 direction,
            glm::vec4 color,
            float innerCutoff,
            float outerCutoff,
            uint32_t shadowMapIndex,
            bool castsShadow);

  alignas(16) glm::vec4 position;
  alignas(16) glm::vec4 direction;
  alignas(16) glm::vec4 color;

  // x: inner cutoff, y: outer cutoff, z and w not utilized
  alignas(16) glm::vec4 cutoff;
  alignas(16) glm::mat4 transform;
  alignas(16) glm::vec4 atlasCoordsPixel;
  alignas(16) glm::vec4 atlasCoordsNormalized;
};

#endif
