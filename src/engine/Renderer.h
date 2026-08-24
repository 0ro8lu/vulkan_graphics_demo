#ifndef RENDERER_H
#define RENDERER_H

#include "engine/Passes/BlinnPhongPass.h"
#include "engine/Passes/HDRPass.h"
#include "engine/Passes/ShadowMapPass.h"
#include "engine/Scene.h"
#include "engine/VulkanInitializer.h"

#include "engine/RenderUtils.h"
#include <memory>

class Renderer
{
public:
  Renderer(GLFWwindow* window);

  ~Renderer();

  void update(const Scene& scene);
  void draw(const Scene& scene, GLFWwindow* window);

  // TODO: re-make this private one day, after having built the asset manager
  // that will take care of the rest of the descriptor sets
  void buildRenderGraph();

  Renderer(Renderer&&) = delete;
  Renderer& operator=(Renderer&&) = delete;
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;
  VulkanContext* m_vkContext;

private:
  VulkanSwapchain* m_vkSwapchain;
  std::unique_ptr<VulkanInitializer> m_vkInitializer;
  CamLightShadowBundle m_camLightShadowBundle;

  std::unique_ptr<ShadowMapPass> m_shadowMapPass;
  std::unique_ptr<BlinnPhongPass> m_blinnPhongPass;
  std::unique_ptr<HDRPass> m_hdrPass;
};

#endif
