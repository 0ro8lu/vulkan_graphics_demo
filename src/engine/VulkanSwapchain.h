#ifndef _VULKAN_SWAPCHAIN_H_
#define _VULKAN_SWAPCHAIN_H_

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <functional>
#include <vector>
#include <vk_mem_alloc.h>

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

  VkCommandBuffer commandBuffer;
  void createCommandBuffer();

  void prepareFrame(GLFWwindow* window);
  void submitFrame(GLFWwindow* window);

  bool resized;
  int width;
  int height;

  VkRenderPass drawingPass;

  VkImageView depthImageView;

  uint32_t imageIndex;
  uint32_t currentFrame = 0;
  std::vector<VkFramebuffer> swapChainFramebuffers;
  VkExtent2D swapChainExtent;
  VkFormat depthFormat;
  VkFormat swapChainImageFormat;

private:
  static void framebufferResizeCallback(GLFWwindow* window,
                                        int width,
                                        int height);
  VulkanContext* vkContext;

  VkSurfaceKHR surface;
  void createSurface(GLFWwindow* window);

  SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

  VkImage depthImage;
  VmaAllocation depthAllocation;

  VkSwapchainKHR swapChain;
  std::vector<VkImage> swapChainImages;
  std::vector<VkImageView> swapChainImageViews;
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

  std::vector<VkSemaphore> m_imageAvailableSemaphores;
  std::vector<VkSemaphore> m_renderFinishedSemaphores;
  VkFence inFlightFence;
  void createSyncObjects();
};

struct SwapChainSupportDetails
{
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};

#endif
