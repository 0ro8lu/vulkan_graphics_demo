#include "engine/Lights.h"

DirectionalLight::DirectionalLight(glm::vec4 direction,
                                   glm::vec4 color,
                                   bool castsShadow)
{
  // float near_plane = 1.0f, far_plane = 9.5f;
  // glm::mat4 lightProjection =
  //   glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);
  glm::mat4 lightProjection =
    glm::perspective(glm::radians(60.0f), 1.0f, 1.0f, 20.0f);
  glm::vec3 lightPos = { -direction.x, -direction.y, -direction.z };

  color.w = 0;
  if (castsShadow) {
    color.w = 1.0f;
  }

  glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0), glm::vec3(0, 1, 0));

  this->direction = direction;
  this->color = color;
  this->transform = lightProjection * lightView;
}

PointLight::PointLight(glm::vec4 position,
                       glm::vec4 color,
                       uint32_t shadowLightIndex,
                       bool castsShadow)
{

  color.w = 0;
  if (castsShadow) {
    color.w = 1.0f;
  }

  // glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0), glm::vec3(0, 1,
  // 0));

  this->position = position;
  this->color = color;
  // this->transform = lightProjection * lightView;
}

SpotLight::SpotLight(glm::vec4 position,
                     glm::vec4 direction,
                     glm::vec4 color,
                     float innerCutoff,
                     float outerCutoff,
                     uint32_t shadowMapIndex,
                     bool castsShadow)
{
  glm::mat4 projection =
    glm::perspective(glm::radians(outerCutoff), 1.0f, 1.0f, 20.0f);

  glm::mat4 lightView = glm::lookAt(
    glm::vec3(position), glm::vec3(position + direction), glm::vec3(0, 1, 0));

  color.w = 0;

  // if we get to this point we are sure there is space in the shadow atlas
  // (lightmanager has taken care of that, hopefully XD)
  if (castsShadow) {
    uint32_t tileX = shadowMapIndex % ATLAS_TILES;
    uint32_t tileY = shadowMapIndex / ATLAS_TILES;

    float tileSize = (float)ATLAS_SIZE / (float)ATLAS_TILES;

    this->atlasCoordsPixel =
      glm::vec4(tileX * tileSize, tileY * tileSize, tileSize, tileSize);

    this->atlasCoordsNormalized =
      glm::vec4(atlasCoordsPixel.x / (float)ATLAS_SIZE,
                atlasCoordsPixel.y / (float)ATLAS_SIZE,
                atlasCoordsPixel.z / (float)ATLAS_SIZE,
                atlasCoordsPixel.w / (float)ATLAS_SIZE);

    color.w = 1.0;
  }

  this->position = position;
  this->direction = direction;
  this->color = color;
  this->cutoff = glm::vec4(innerCutoff, outerCutoff, 1, 1);
  this->transform = projection * lightView;
}

void
SpotLight::move(glm::vec4 position, glm::vec4 direction)
{
  this->position = position;
  this->direction = direction;

  glm::mat4 projection =
    glm::perspective(glm::radians(this->cutoff.y * 2), 1.0f, 1.0f, 20.0f);

  glm::mat4 lightView =
    glm::lookAt(glm::vec3(position), glm::vec3(position + direction), glm::vec3(0, 1, 0));

  this->transform = projection * lightView;
}
