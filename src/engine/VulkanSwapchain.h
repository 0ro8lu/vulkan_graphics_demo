#ifndef _VULKAN_SWAPCHAIN_H_
#define _VULKAN_SWAPCHAIN_H_

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vk_mem_alloc.h>

#include <vector>

#include "engine/Camera3D.h"

#include "engine/VulkanContext.h"

class VulkanInitializer;
struct SwapChainSupportDetails;

class VulkanSwapchain
{
public:
  // void present
  VkFormat getSwapChainImageFormat() { return swapChainImageFormat; }
  VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
                               VkImageTiling tiling,
                               VkFormatFeatureFlags features);
  void recreateSwapChain();
  void createSwapChainFrameBuffer();

  VkCommandBuffer commandBuffer;
  void createCommandBuffer();

  Camera3D* camera;
  void setMainRenderCamera(Camera3D* camera);

  void prepareFrame();
  void submitFrame();

  bool resized;
  int width;
  int height;

  VkRenderPass drawingPass;

  uint32_t imageIndex;
  std::vector<VkFramebuffer> swapChainFramebuffers;
  VkExtent2D swapChainExtent;

private:
  friend VulkanInitializer;

  VulkanSwapchain(VulkanContext* vkContext, GLFWwindow* window);
  ~VulkanSwapchain();

  static void framebufferResizeCallback(GLFWwindow* window,
                                        int width,
                                        int height);
  GLFWwindow* window;
  VulkanContext* vkContext;

  VkSurfaceKHR surface;
  void createSurface();

  SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

  VkImage depthImage;
  VkImageView depthImageView;
  VmaAllocation depthAllocation;

  VkSwapchainKHR swapChain;
  std::vector<VkImage> swapChainImages;
  std::vector<VkImageView> swapChainImageViews;
  VkFormat swapChainImageFormat;
  void createSwapChain();
  void cleanSwapChain();

  VkSurfaceFormatKHR chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& availableFormats);
  VkPresentModeKHR chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR>& availablePresentModes);
  VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                              GLFWwindow* window);

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
