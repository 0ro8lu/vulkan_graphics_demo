#ifndef _LIGHTS_H_
#define _LIGHTS_H_

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

  void move(glm::vec3 direction);
  void follow(glm::vec3 position);

private:
  DirectionalLight(glm::vec3 direction, glm::vec3 color, bool castsShadow);

  alignas(16) glm::vec4 direction;
  alignas(16) glm::vec4 color;
  alignas(16) glm::mat4 transform;
};

struct PointLight
{
  friend class LightManager;

  enum Side
  {
    UP,
    DOWN,
    LEFT,
    RIGHT,
    FORWARD,
    BACK,
  };

  const glm::vec4& getPosition() const { return position; }
  const glm::vec4& getColor() const { return color; }
  const glm::mat4& getTransform(Side side) const
  {
    return transform[sideToIndex(side)];
  }
  const glm::vec4& getAtlasCoordinatesPixel(Side side) const
  {
    return atlasCoordsPixel[sideToIndex(side)];
  }
  const glm::vec4& getAtlasCoordinatesNormalized(Side side) const
  {
    return atlasCoordsNormalized[sideToIndex(side)];
  }

  bool castsShadow() const { return color.w == 1.0; }

private:
  PointLight(glm::vec3 position,
             glm::vec3 color,
             uint32_t shadowMapIndex,
             bool castsShadow);

  const uint32_t sideToIndex(Side side) const
  {
    switch (side) {
      case UP:
        return 0;
        break;
      case DOWN:
        return 1;
        break;
      case LEFT:
        return 2;
        break;
      case RIGHT:
        return 3;
        break;
      case FORWARD:
        return 4;
        break;
      case BACK:
        return 5;
        break;
    }
  }

  alignas(16) glm::vec4 position;
  alignas(16) glm::vec4 color;
  alignas(16) glm::mat4 transform[6];
  alignas(16) glm::vec4 atlasCoordsPixel[6];
  alignas(16) glm::vec4 atlasCoordsNormalized[6];
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

  void move(glm::vec3 position, glm::vec3 direction);

private:
  SpotLight(glm::vec3 position,
            glm::vec3 direction,
            glm::vec3 color,
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
