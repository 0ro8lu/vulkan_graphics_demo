#pragma once

#include "engine/FrameScheduler.h"
#include "engine/FrameUniforms.h"
#include "engine/GpuAllocator.h"
#include "engine/GpuDevice.h"
#include "engine/ImmediateCommands.h"
#include "engine/Passes/BlinnPhongPass.h"
#include "engine/Passes/HDRPass.h"
#include "engine/Passes/ShadowMapPass.h"
#include "engine/SamplerCache.h"
#include "engine/SceneRenderData.h"
#include "engine/VulkanInstance.h"
#include "engine/VulkanSwapchain.h"

#include <memory>

class Window;

class Renderer
{
public:
  explicit Renderer(const Window& window);
  ~Renderer();

  void draw(const SceneRenderData& sceneRenderData);
  void waitIdle() const;

  void buildRenderGraph();
  void onResize(uint32_t width, uint32_t height);

  Renderer(Renderer&&) = delete;
  Renderer& operator=(Renderer&&) = delete;
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  [[nodiscard]] GpuDevice& device() noexcept { return *m_device; }
  [[nodiscard]] const GpuDevice& device() const noexcept { return *m_device; }

  [[nodiscard]] GpuAllocator& allocator() noexcept { return *m_allocator; }
  [[nodiscard]] const GpuAllocator& allocator() const noexcept
  {
    return *m_allocator;
  }

  [[nodiscard]] SamplerCache& samplerCache() noexcept { return *m_samplerCache; }
  [[nodiscard]] const SamplerCache& samplerCache() const noexcept
  {
    return *m_samplerCache;
  }

  [[nodiscard]] ImmediateCommands& immediateCommands() noexcept
  {
    return *m_immediateCommands;
  }
  [[nodiscard]] const ImmediateCommands& immediateCommands() const noexcept
  {
    return *m_immediateCommands;
  }

  [[nodiscard]] VulkanSwapchain& swapchain() noexcept { return *m_vkSwapchain; }
  [[nodiscard]] const VulkanSwapchain& swapchain() const noexcept
  {
    return *m_vkSwapchain;
  }

  [[nodiscard]] FrameScheduler& frameScheduler() noexcept
  {
    return *m_frameScheduler;
  }
  [[nodiscard]] const FrameScheduler& frameScheduler() const noexcept
  {
    return *m_frameScheduler;
  }

private:
  std::unique_ptr<VulkanInstance> m_instance;
  VkSurfaceKHR m_surface{ VK_NULL_HANDLE };
  std::unique_ptr<GpuDevice> m_device;
  std::unique_ptr<GpuAllocator> m_allocator;
  std::unique_ptr<SamplerCache> m_samplerCache;
  std::unique_ptr<ImmediateCommands> m_immediateCommands;
  std::unique_ptr<VulkanSwapchain> m_vkSwapchain;
  std::unique_ptr<FrameScheduler> m_frameScheduler;
  std::unique_ptr<FrameUniforms> m_frameUniforms;

  std::unique_ptr<ShadowMapPass> m_shadowMapPass;
  std::unique_ptr<BlinnPhongPass> m_blinnPhongPass;
  std::unique_ptr<HDRPass> m_hdrPass;
};
