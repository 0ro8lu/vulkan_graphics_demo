#ifndef VULKAN_INITIALIZER_H_
#define VULKAN_INITIALIZER_H_

#include "engine/VulkanContext.h"

#include "engine/VulkanSwapchain.h"

#include <iostream>

struct QueueFamilyIndices;
struct SwapChainSupportDetails;

class VulkanInitializer
{
public:
  VulkanInitializer(GLFWwindow* window);

  VulkanContext* vkContext;
  VulkanSwapchain* vkSwapchain;

  ~VulkanInitializer();

private:
  VkDebugUtilsMessengerEXT debugMessenger;
  void setupDebugMessenger();

  VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger);

  void createInstance();

  void selectPhysicalDevice();

  bool isDeviceSuitable(VkPhysicalDevice device);

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
  bool checkDeviceExtensionSupport(VkPhysicalDevice device);

  VkPhysicalDeviceProperties deviceProperties{};
  VkPhysicalDeviceFeatures deviceFeatures{};
  VkPhysicalDeviceMemoryProperties deviceMemoryProperties{};

  void createLogicalDevice();

  void createCommandPool();

  const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
  };

#ifdef NDEBUG
  const bool enableValidationLayers = false;
#else
  const bool enableValidationLayers = true;
#endif
  bool checkValidationLayerSupport();

  void populateDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT& createInfo);

  std::vector<const char*> getRequiredExtensions();

  void createVMAAllocator();
};

static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
              void* pUserData)
{
  std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

  return VK_FALSE;
}

#endif
