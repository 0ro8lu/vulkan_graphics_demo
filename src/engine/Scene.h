#pragma once

#include "engine/Camera3D.h"
#include "engine/Input.h"
#include "engine/LightManager.h"
#include "engine/ModelLoading/Model.h"
#include "engine/Skybox.h"
#include "engine/VulkanContext.h"

#include <memory>
#include <optional>

class Scene
{
public:
  Scene(VulkanContext* vkContext);
  ~Scene();

  void update(float deltaTime, GameEvent events);

  std::vector<Model> models;
  std::vector<Model> lightCubes;

  std::unique_ptr<Skybox> skybox;
  std::unique_ptr<Camera3D> camera;

  std::vector<PointLight> pointLights;
  std::optional<DirectionalLight> directionalLight;
  std::vector<SpotLight> spotLights;

  Scene(Scene&&) = delete;
  Scene& operator=(Scene&&) = delete;
  Scene(const Scene&) = delete;
  Scene& operator=(const Scene&) = delete;

private:
  VulkanContext* vkContext;
};
