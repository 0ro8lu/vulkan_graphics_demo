#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "engine/FrameContext.h"
#include "engine/VulkanTypes.h"

class GpuDevice;
class VulkanSwapchain;

class FrameScheduler final
{
public:
  explicit FrameScheduler(const GpuDevice& device);
  ~FrameScheduler();

  FrameScheduler(const FrameScheduler&) = delete;
  FrameScheduler& operator=(const FrameScheduler&) = delete;
  FrameScheduler(FrameScheduler&&) = delete;
  FrameScheduler& operator=(FrameScheduler&&) = delete;

  [[nodiscard]] std::optional<FrameContext> beginFrame(
    VulkanSwapchain& swapchain);

  [[nodiscard]] bool submitFrame(VulkanSwapchain& swapchain,
                                 const FrameContext& frameContext);

  [[nodiscard]] uint32_t currentFrameIndex() const noexcept
  {
    return m_currentFrame;
  }

private:
  struct FrameSlot
  {
    VkSemaphore imageAvailableSemaphore{ VK_NULL_HANDLE };
    VkFence inFlightFence{ VK_NULL_HANDLE };
    VkCommandBuffer commandBuffer{ VK_NULL_HANDLE };
  };

  void createSyncObjects();
  void createCommandBuffers();
  void ensureRenderFinishedSemaphores(size_t imageCount);

  const GpuDevice& m_device;
  VkCommandPool m_commandPool{ VK_NULL_HANDLE };

  std::array<FrameSlot, MAX_FRAMES_IN_FLIGHT> m_slots{};
  std::vector<VkSemaphore> m_renderFinishedSemaphores{};

  uint32_t m_currentFrame{ 0 };
};
