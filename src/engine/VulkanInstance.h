#pragma once

#include <array>
#include <vector>
#include <vulkan/vulkan_core.h>

class Window;

class VulkanInstance
{
public:
  VulkanInstance();
  ~VulkanInstance();

  VulkanInstance(const VulkanInstance&) = delete;
  VulkanInstance& operator=(const VulkanInstance&) = delete;
  VulkanInstance(VulkanInstance&&) = delete;
  VulkanInstance& operator=(VulkanInstance&&) = delete;

  [[nodiscard]] VkInstance instance() const noexcept { return m_instance; }

  [[nodiscard]] VkSurfaceKHR createSurface(const Window& window) const;
  void destroySurface(VkSurfaceKHR surface) const noexcept;

private:
  VkInstance m_instance{ VK_NULL_HANDLE };
  VkDebugUtilsMessengerEXT m_debugMessenger{ VK_NULL_HANDLE };

  void createInstance();
  void setupDebugMessenger();

  static constexpr auto kValidationLayers = std::to_array<const char*>({
    "VK_LAYER_KHRONOS_validation",
  });

  static constexpr bool kEnableValidationLayers =
#ifdef NDEBUG
    false;
#else
    true;
#endif

  bool checkValidationLayerSupport();
  void populateDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT& createInfo);
  std::vector<const char*> getRequiredExtensions();

  VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger);
};
