#include "engine/Lights.h"

DirectionalLight::DirectionalLight(glm::vec3 direction,
                                   glm::vec3 color,
                                   bool castsShadow)
{
  glm::mat4 lightProjection =
    glm::ortho(-15.0f, 15.0f, -15.0f, 15.0f, 0.1f, 20.0f);
  glm::vec3 lightPos = { -direction.x, -direction.y, -direction.z };

  float shadow = (castsShadow) ? 1.0 : 0.0;

  glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0), glm::vec3(0, 1, 0));

  this->direction = glm::vec4(direction, 1.0);
  this->color = glm::vec4(color, shadow);
  this->transform = lightProjection * lightView;
}

void
DirectionalLight::move(glm::vec3 direction)
{
  glm::mat4 lightProjection =
    glm::ortho(-15.0f, 15.0f, -15.0f, 15.0f, 0.1f, 20.0f);

  glm::vec3 lightPos = { -direction.x, -direction.y, -direction.z };

  glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0), glm::vec3(0, 1, 0));

  this->direction = glm::vec4(direction, 1.0);
  this->transform = lightProjection * lightView;
}

void
DirectionalLight::follow(glm::vec3 position)
{
  glm::mat4 lightProjection =
    glm::ortho(-15.0f, 15.0f, -15.0f, 15.0f, 0.1f, 20.0f);

  glm::vec3 lightPos = { position.x, -direction.y, position.z };

  glm::mat4 lightView =
    glm::lookAt(lightPos, lightPos + glm::vec3(direction), glm::vec3(0, 1, 0));

  this->transform = lightProjection * lightView;
}

PointLight::PointLight(glm::vec3 position,
                       glm::vec3 color,
                       uint32_t shadowMapIndex,
                       bool castsShadow)
{
  float shadow = (castsShadow) ? 1.0 : 0.0;

  // calculate 6 transform matrices.
  glm::mat4 projection =
    glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

  // this->transform[0] = projection * glm::lookAt(position, position +
  // glm::vec3( 1.0, 0.0, 0.0), glm::vec3(0.0, 1.0, 0.0)); this->transform[1] =
  // projection * glm::lookAt(position, position + glm::vec3(-1.0, 0.0, 0.0),
  // glm::vec3(0.0, 1.0, 0.0)); this->transform[2] = projection *
  // glm::lookAt(position, position + glm::vec3( 0.0, 1.0, 0.0),
  // glm::vec3(0.0, 1.0, 0.0)); this->transform[3] = projection *
  // glm::lookAt(position, position + glm::vec3( 0.0,-1.0, 0.0),
  // glm::vec3(0.0, 1.0, 0.0)); this->transform[4] = projection *
  // glm::lookAt(position, position + glm::vec3( 0.0, 0.0, 1.0),
  // glm::vec3(0.0, 1.0, 0.0)); this->transform[5] = projection *
  // glm::lookAt(position, position + glm::vec3( 0.0, 0.0,-1.0),
  // glm::vec3(0.0, 1.0, 0.0));

  this->transform[0] =
    projection * glm::lookAt(position,
                             position + glm::vec3(0.0, 1.0, 0.0),
                             glm::vec3(0.0, 0.0, -1.0)); // UP
  this->transform[1] =
    projection * glm::lookAt(position,
                             position + glm::vec3(0.0, -1.0, 0.0),
                             glm::vec3(0.0, 0.0, 1.0)); // DOWN
  this->transform[2] =
    projection * glm::lookAt(position,
                             position + glm::vec3(-1.0, 0.0, 0.0),
                             glm::vec3(0.0, 1.0, 0.0)); // LEFT
  this->transform[3] =
    projection * glm::lookAt(position,
                             position + glm::vec3(1.0, 0.0, 0.0),
                             glm::vec3(0.0, 1.0, 0.0)); // RIGHT
  this->transform[4] =
    projection * glm::lookAt(position,
                             position + glm::vec3(0.0, 0.0, 1.0),
                             glm::vec3(0.0, 1.0, 0.0)); // FORWARD
  this->transform[5] =
    projection * glm::lookAt(position,
                             position + glm::vec3(0.0, 0.0, -1.0),
                             glm::vec3(0.0, 1.0, 0.0)); // BACK

  // calculate 6 coordinates.
  if (castsShadow) {
    for (int i = 0; i < 6; i++) {
      uint32_t tileX = (shadowMapIndex + i) % ATLAS_TILES;
      uint32_t tileY = (shadowMapIndex + i) / ATLAS_TILES;

      float tileSize = (float)ATLAS_SIZE / (float)ATLAS_TILES;

      this->atlasCoordsPixel[i] =
        glm::vec4(tileX * tileSize, tileY * tileSize, tileSize, tileSize);

      this->atlasCoordsNormalized[i] =
        glm::vec4(atlasCoordsPixel[i].x / (float)ATLAS_SIZE,
                  atlasCoordsPixel[i].y / (float)ATLAS_SIZE,
                  atlasCoordsPixel[i].z / (float)ATLAS_SIZE,
                  atlasCoordsPixel[i].w / (float)ATLAS_SIZE);

      shadow = 1.0;
    }
  }

  this->position = glm::vec4(position, 1);
  this->color = glm::vec4(color, shadow);
}

SpotLight::SpotLight(glm::vec3 position,
                     glm::vec3 direction,
                     glm::vec3 color,
                     float innerCutoff,
                     float outerCutoff,
                     uint32_t shadowMapIndex,
                     bool castsShadow)
{
  glm::mat4 projection =
    glm::perspective(glm::radians(50.0f), 1.0f, 0.1f, 10.0f);

  glm::mat4 lightView =
    glm::lookAt(glm::vec3(position), glm::vec3(direction), glm::vec3(0, 1, 0));

  float shadow = 0.0;

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

    shadow = 1.0;
  }

  this->position = glm::vec4(position, 1.0);
  this->direction = glm::vec4(direction, 1.0);
  this->color = glm::vec4(color, shadow);
  this->cutoff = glm::vec4(innerCutoff, outerCutoff, 1, 1);
  this->transform = projection * lightView;
}

void
SpotLight::move(glm::vec3 position, glm::vec3 direction)
{
  this->position = glm::vec4(position, 1.0);
  this->direction = glm::vec4(direction, 1.0);

  glm::mat4 projection =
    glm::perspective(glm::radians(50.0f), 1.0f, 1.0f, 20.0f);

  glm::mat4 lightView =
    glm::lookAt(glm::vec3(position), glm::vec3(direction), glm::vec3(0, 1, 0));

  this->transform = projection * lightView;
}
