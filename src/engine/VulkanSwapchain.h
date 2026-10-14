#pragma once

#include <optional>
#include <span>
#include <vector>
#include <vulkan/vulkan.h>

#include "engine/GpuAllocator.h"
#include "engine/GpuDevice.h"

class Renderer;

struct SwapChainSupportDetails
{
  VkSurfaceCapabilitiesKHR capabilities{};
  std::vector<VkSurfaceFormatKHR> formats{};
  std::vector<VkPresentModeKHR> presentModes{};
};

class VulkanSwapchain final
{
public:
  class Key
  {
    friend class Renderer;
    Key() = default;
  };

  explicit VulkanSwapchain(Key,
                           const GpuDevice& device,
                           const GpuAllocator& allocator,
                           VkSurfaceKHR surface,
                           VkExtent2D initialExtent);
  ~VulkanSwapchain();

  VulkanSwapchain(const VulkanSwapchain&) = delete;
  VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;
  VulkanSwapchain(VulkanSwapchain&&) = delete;
  VulkanSwapchain& operator=(VulkanSwapchain&&) = delete;

  void recreateSwapChain(VkExtent2D newExtent);
  void createSwapChainFrameBuffer();

  [[nodiscard]] std::optional<uint32_t> acquireNextImage(
    VkSemaphore imageAvailableSemaphore);

  [[nodiscard]] bool present(uint32_t imageIndex, VkSemaphore waitSemaphore);

  [[nodiscard]] VkFramebuffer framebuffer(uint32_t imageIndex) const noexcept
  {
    return swapChainFramebuffers[imageIndex];
  }

  [[nodiscard]] size_t imageCount() const noexcept
  {
    return m_swapChainImages.size();
  }

  [[nodiscard]] VkExtent2D extent() const noexcept { return swapChainExtent; }
  [[nodiscard]] uint32_t width() const noexcept { return swapChainExtent.width; }
  [[nodiscard]] uint32_t height() const noexcept { return swapChainExtent.height; }

  VkRenderPass drawingPass{ VK_NULL_HANDLE };

  VkImageView depthImageView{ VK_NULL_HANDLE };
  VkFormat depthFormat{ VK_FORMAT_UNDEFINED };
  VkFormat swapChainImageFormat{ VK_FORMAT_UNDEFINED };

private:
  std::vector<VkFramebuffer> swapChainFramebuffers{};
  VkExtent2D swapChainExtent{};

  const GpuDevice& m_device;
  const GpuAllocator& m_allocator;
  VkSurfaceKHR m_surface{ VK_NULL_HANDLE };

  [[nodiscard]] SwapChainSupportDetails querySwapChainSupport(
    VkPhysicalDevice device) const;

  VkImage m_depthImage{ VK_NULL_HANDLE };
  VmaAllocation m_depthAllocation{ VK_NULL_HANDLE };

  VkSwapchainKHR m_swapChain{ VK_NULL_HANDLE };
  std::vector<VkImage> m_swapChainImages{};
  std::vector<VkImageView> m_swapChainImageViews{};

  void createVulkanSwapChain(VkExtent2D extent,
                             VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);
  void cleanTransientResources();

  [[nodiscard]] static VkSurfaceFormatKHR chooseSwapSurfaceFormat(
    std::span<const VkSurfaceFormatKHR> availableFormats);
  [[nodiscard]] static VkPresentModeKHR chooseSwapPresentMode(
    std::span<const VkPresentModeKHR> availablePresentModes);
  [[nodiscard]] static VkExtent2D chooseSwapExtent(
    const VkSurfaceCapabilitiesKHR& capabilities,
    VkExtent2D requestedExtent);
};
