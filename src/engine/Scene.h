#pragma once

#include "engine/Camera3D.h"
#include "engine/Input.h"
#include "engine/LightManager.h"
#include "engine/ModelLoading/Model.h"
#include "engine/SceneRenderData.h"
#include "engine/Skybox.h"

#include <memory>
#include <optional>

class GpuDevice;
class GpuAllocator;
class SamplerCache;
class ImmediateCommands;

class Scene final
{
public:
  Scene(const GpuDevice& device,
        const GpuAllocator& allocator,
        const SamplerCache& samplerCache,
        const ImmediateCommands& immediateCommands);
  ~Scene();

  void update(float deltaTime, const GameEvent& events) noexcept;

  [[nodiscard]] SceneRenderData getRenderData() const noexcept
  {
    return SceneRenderData{
      .models = m_models,
      .lightCubes = m_lightCubes,
      .skybox = m_skybox.get(),
      .camera = m_camera.get(),
      .pointLights = m_pointLights,
      .directionalLight =
        m_directionalLight.has_value() ? &m_directionalLight.value() : nullptr,
      .spotLights = m_spotLights,
    };
  }

  std::vector<Model> m_models;
  std::vector<Model> m_lightCubes;

  std::unique_ptr<Skybox> m_skybox;
  std::unique_ptr<Camera3D> m_camera;

  std::vector<PointLight> m_pointLights;
  std::optional<DirectionalLight> m_directionalLight;
  std::vector<SpotLight> m_spotLights;

  Scene(Scene&&) = delete;
  Scene& operator=(Scene&&) = delete;
  Scene(const Scene&) = delete;
  Scene& operator=(const Scene&) = delete;

private:
  const GpuDevice& m_device;
  const GpuAllocator& m_allocator;
  const SamplerCache& m_samplerCache;
  const ImmediateCommands& m_immediateCommands;
};
