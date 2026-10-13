#pragma once

#include "engine/VulkanTypes.h"

#include <array>
#include <vector>

struct GLFWwindow;
class Renderer;

class VulkanContext
{
public:
  class Key
  {
    friend class Renderer;
    Key() = default;
  };

  explicit VulkanContext(Key, GLFWwindow* window);
  ~VulkanContext();

  VulkanContext(const VulkanContext&) = delete;
  VulkanContext& operator=(const VulkanContext&) = delete;
  VulkanContext(VulkanContext&&) = delete;
  VulkanContext& operator=(VulkanContext&&) = delete;

  VkInstance instance{ VK_NULL_HANDLE };

  VkPhysicalDevice physicalDevice{ VK_NULL_HANDLE };
  VkDevice logicalDevice{ VK_NULL_HANDLE };

  VmaAllocator allocator{ VK_NULL_HANDLE };

  VkQueue graphicsQueue{ VK_NULL_HANDLE };
  VkQueue presentQueue{ VK_NULL_HANDLE };

  VkCommandPool commandPool{ VK_NULL_HANDLE };

  QueueFamilyIndices queueFamilies{};

  // create vulkan primitives
  VkImage createImage(uint32_t width,
                      uint32_t height,
                      VkFormat format,
                      uint32_t layerCount,
                      VkImageTiling tiling,
                      VkImageUsageFlags usage,
                      VmaAllocation& allocation,
                      VkImageCreateFlags imageCreateFlags = 0);

  [[nodiscard]] VkShaderModule createShaderModule(
    const std::vector<char>& code);

  VkImageView createImageView(VkImage image,
                              VkFormat format,
                              uint32_t layerCount,
                              VkImageAspectFlags aspectFlags,
                              VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D);

  [[nodiscard]] VulkanBuffer createBuffer(VkDeviceSize size,
                                          VkBufferUsageFlags usage,
                                          BufferType bufferType);

  VkCommandBuffer beginSingleTimeCommands();
  void endSingleTimeCommands(VkCommandBuffer commandBuffer);

  // void copyBuffer
  void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

  // destroy vulkan primitives
  void destroyImage(VkImage image, VmaAllocation allocation) noexcept;
  void destroyImageView(VkImageView view) noexcept;
  void destroyBuffer(VulkanBuffer& buffer) noexcept;
  void destroyBuffer(VkBuffer buffer, VmaAllocation allocation) noexcept;

  [[nodiscard]] VkSampler getSampler(SamplerType type) const noexcept
  {
    return m_samplers[static_cast<size_t>(type)];
  }

private:
  VkDebugUtilsMessengerEXT m_debugMessenger{ VK_NULL_HANDLE };
  void setupDebugMessenger();

  VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger);

  void createInstance();
  void selectPhysicalDevice(VkSurfaceKHR surface);
  bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface);

#ifdef __APPLE__
  const std::vector<const char*> m_deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    "VK_KHR_portability_subset"
  };
#else
  const std::vector<const char*> m_deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
  };
#endif
  bool checkDeviceExtensionSupport(VkPhysicalDevice device);

  VkPhysicalDeviceProperties m_deviceProperties{};
  VkPhysicalDeviceFeatures m_deviceFeatures{};
  VkPhysicalDeviceMemoryProperties m_deviceMemoryProperties{};

  void createLogicalDevice(VkSurfaceKHR surface);
  void createCommandPool();
  void createVMAAllocator();

  const std::vector<const char*> m_validationLayers = {
    "VK_LAYER_KHRONOS_validation"
  };

#ifdef NDEBUG
  const bool m_enableValidationLayers = false;
#else
  const bool m_enableValidationLayers = true;
#endif
  bool checkValidationLayerSupport();

  void populateDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT& createInfo);

  std::vector<const char*> getRequiredExtensions();

  void createSamplers();

  std::array<VkSampler, static_cast<size_t>(SamplerType::Count)> m_samplers{
    VK_NULL_HANDLE
  };
};
