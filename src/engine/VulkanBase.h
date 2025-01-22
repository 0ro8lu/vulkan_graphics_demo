#ifndef VULKAN_BASE_H_
#define VULKAN_BASE_H_

#include "engine/VulkanDevice.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <vector>

class VulkanBase
{
public:
  VulkanBase();

  void init();

  void setupDebugMessenger();
  VkDebugUtilsMessengerEXT debugMessenger;
  VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger);

  void createInstance();
  VkInstance instance;

  void selectPhysicalDevice();
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  bool isDeviceSuitable(VkPhysicalDevice device);
  struct QueueFamilyIndices
  {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete()
    {
      return graphicsFamily.has_value() && presentFamily.has_value();
    }
  };
  QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
  bool checkDeviceExtensionSupport(VkPhysicalDevice device);
  struct SwapChainSupportDetails
  {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
  };
#ifdef __APPLE__
  const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    "VK_KHR_portability_subset"
  };
#else
  const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
  };
#endif
  SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
  VkPhysicalDeviceProperties deviceProperties{};
  VkPhysicalDeviceFeatures deviceFeatures{};
  VkPhysicalDeviceMemoryProperties deviceMemoryProperties{};

  VulkanDevice* vulkanDevice;
  VkDevice logicalDevice;
  VkQueue graphicsQueue;
  VkQueue presentQueue;
  void createLogicalDevice();

  const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
  };
  VkSurfaceKHR surface;

#ifdef NDEBUG
  const bool enableValidationLayers = false;
#else
  const bool enableValidationLayers = true;
#endif

  virtual void getEnabledFeatures() {};
  virtual void getEnabledExtensions() {};

  bool checkValidationLayerSupport();

  static VKAPI_ATTR VkBool32 VKAPI_CALL
  debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                VkDebugUtilsMessageTypeFlagsEXT messageType,
                const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                void* pUserData)
  {
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
  }

  VkFormat findDepthFormat();
  VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
                               VkImageTiling tiling,
                               VkFormatFeatureFlags features);

  void populateDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT& createInfo);

  std::vector<const char*> getRequiredExtensions();
};

#endif
