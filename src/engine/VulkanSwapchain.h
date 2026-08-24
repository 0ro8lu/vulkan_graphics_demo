#ifndef _VULKAN_SWAPCHAIN_H_
#define _VULKAN_SWAPCHAIN_H_

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <functional>
#include <vector>
#include <vk_mem_alloc.h>

#include "engine/VulkanContext.h"

class VulkanInitializer;
class Renderer;
struct SwapChainSupportDetails;

class VulkanSwapchain
{
public:
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
  std::vector<VkFramebuffer> swapChainFramebuffers;
  VkExtent2D swapChainExtent;

private:
  friend VulkanInitializer;
  friend Renderer;

  VulkanSwapchain(VulkanContext* vkContext, GLFWwindow* window);
  ~VulkanSwapchain();

  static void framebufferResizeCallback(GLFWwindow* window,
                                        int width,
                                        int height);
  VulkanContext* vkContext;

  VkSurfaceKHR surface;
  void createSurface(GLFWwindow* window);

  SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

  VkImage depthImage;
  VmaAllocation depthAllocation;
  VkFormat depthFormat;

  VkSwapchainKHR swapChain;
  std::vector<VkImage> swapChainImages;
  std::vector<VkImageView> swapChainImageViews;
  VkFormat swapChainImageFormat;
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

  VkSemaphore imageAvailableSemaphore;
  VkSemaphore renderFinishedSemaphore;
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
