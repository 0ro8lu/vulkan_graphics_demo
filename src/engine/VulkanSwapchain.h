#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <array>
#include <functional>
#include <optional>
#include <vector>

#include "engine/FrameContext.h"
#include "engine/VulkanContext.h"

class Renderer;
struct SwapChainSupportDetails;

class VulkanSwapchain
{
public:
  class Key
  {
    friend class Renderer;
    Key() = default;
  };

  explicit VulkanSwapchain(Key, VulkanContext* vkContext, GLFWwindow* window);
  ~VulkanSwapchain();

  VulkanSwapchain(const VulkanSwapchain&) = delete;
  VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;
  VulkanSwapchain(VulkanSwapchain&&) = delete;
  VulkanSwapchain& operator=(VulkanSwapchain&&) = delete;

  std::function<void(int, int)> onResize;
  void recreateSwapChain(GLFWwindow* window);
  void createSwapChainFrameBuffer();

  [[nodiscard]] std::optional<FrameContext> beginFrame(GLFWwindow* window);
  void submitFrame(const FrameContext& frameContext, GLFWwindow* window);

  bool resized;
  int width;
  int height;

  VkRenderPass drawingPass;

  VkImageView depthImageView;
  VkFormat depthFormat;
  VkFormat swapChainImageFormat;

private:
  struct FrameSyncData
  {
    VkSemaphore imageAvailableSemaphore{ VK_NULL_HANDLE };
    VkFence inFlightFence{ VK_NULL_HANDLE };
    VkCommandBuffer commandBuffer{ VK_NULL_HANDLE };
  };

  void createCommandBuffers();
  void createSyncObjects();

  uint32_t imageIndex = 0;
  uint32_t currentFrame = 0;
  std::vector<VkFramebuffer> swapChainFramebuffers;
  VkExtent2D swapChainExtent{};

  std::array<FrameSyncData, MAX_FRAMES_IN_FLIGHT> m_frameSync{};
  std::vector<VkSemaphore> m_renderFinishedSemaphores;

  static void framebufferResizeCallback(GLFWwindow* window,
                                        int width,
                                        int height);
  VulkanContext* m_vkContext;

  VkSurfaceKHR m_surface;
  void createSurface(GLFWwindow* window);

  SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

  VkImage m_depthImage;
  VmaAllocation m_depthAllocation;

  VkSwapchainKHR m_swapChain;
  std::vector<VkImage> m_swapChainImages;
  std::vector<VkImageView> m_swapChainImageViews;
  void createVulkanSwapChain(GLFWwindow* window);
  void cleanSwapChain();

  VkSurfaceFormatKHR chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& availableFormats);
  VkPresentModeKHR chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR>& availablePresentModes);
  VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                              GLFWwindow* window);

  VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
                               VkImageTiling tiling,
                               VkFormatFeatureFlags features);
};

struct SwapChainSupportDetails
{
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};
