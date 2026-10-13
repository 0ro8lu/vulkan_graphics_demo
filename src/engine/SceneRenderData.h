#pragma once

#include <span>

#include "engine/Camera3D.h"
#include "engine/Lights.h"
#include "engine/ModelLoading/Model.h"
#include "engine/Skybox.h"

struct SceneRenderData
{
  std::span<const Model> models;
  std::span<const Model> lightCubes;
  const Skybox* skybox = nullptr;
  const Camera3D* camera = nullptr;
  std::span<const PointLight> pointLights;
  const DirectionalLight* directionalLight = nullptr;
  std::span<const SpotLight> spotLights;
};
