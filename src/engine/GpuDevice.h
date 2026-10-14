#pragma once

#include "engine/VulkanTypes.h"

#include <array>
#include <span>

class Renderer;

class GpuDevice final
{
public:
  class Key
  {
    friend class Renderer;
    Key() = default;
  };

  explicit GpuDevice(Key, VkInstance instance, VkSurfaceKHR surface);
  ~GpuDevice();

  GpuDevice(const GpuDevice&) = delete;
  GpuDevice& operator=(const GpuDevice&) = delete;
  GpuDevice(GpuDevice&&) = delete;
  GpuDevice& operator=(GpuDevice&&) = delete;

  // --- Core handle accessors ---

  [[nodiscard]] VkDevice device() const noexcept { return m_device; }
  [[nodiscard]] VkPhysicalDevice physicalDevice() const noexcept
  {
    return m_physicalDevice;
  }
  [[nodiscard]] VkInstance instance() const noexcept { return m_instance; }

  // --- Queues ---

  [[nodiscard]] VkQueue graphicsQueue() const noexcept
  {
    return m_graphicsQueue;
  }
  [[nodiscard]] VkQueue presentQueue() const noexcept { return m_presentQueue; }
  [[nodiscard]] const QueueFamilyIndices& queueFamilies() const noexcept
  {
    return m_queueFamilies;
  }

  // --- Device properties & features ---

  [[nodiscard]] const VkPhysicalDeviceProperties& properties() const noexcept
  {
    return m_deviceProperties;
  }
  [[nodiscard]] const VkPhysicalDeviceFeatures& features() const noexcept
  {
    return m_deviceFeatures;
  }
  [[nodiscard]] const VkPhysicalDeviceMemoryProperties& memoryProperties()
    const noexcept
  {
    return m_deviceMemoryProperties;
  }

  // --- Device operations ---

  void waitIdle() const noexcept;

  [[nodiscard]] VkFormat findSupportedFormat(
    std::span<const VkFormat> candidates,
    VkImageTiling tiling,
    VkFormatFeatureFlags features) const;

  [[nodiscard]] VkImageView createImageView(
    VkImage image,
    VkFormat format,
    uint32_t layerCount,
    VkImageAspectFlags aspectFlags,
    VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D,
    uint32_t levelCount = 1,
    uint32_t baseMipLevel = 0) const;

  void destroyImageView(VkImageView view) const noexcept;

private:
  VkInstance m_instance{ VK_NULL_HANDLE };
  VkPhysicalDevice m_physicalDevice{ VK_NULL_HANDLE };
  VkDevice m_device{ VK_NULL_HANDLE };
  VkQueue m_graphicsQueue{ VK_NULL_HANDLE };
  VkQueue m_presentQueue{ VK_NULL_HANDLE };
  QueueFamilyIndices m_queueFamilies{};

  VkPhysicalDeviceProperties m_deviceProperties{};
  VkPhysicalDeviceFeatures m_deviceFeatures{};
  VkPhysicalDeviceMemoryProperties m_deviceMemoryProperties{};

#ifdef __APPLE__
  static constexpr auto kDeviceExtensions = std::to_array<const char*>({
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    "VK_KHR_portability_subset",
  });
#else
  static constexpr auto kDeviceExtensions = std::to_array<const char*>({
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  });
#endif

  void selectPhysicalDevice(VkSurfaceKHR surface);
  bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface);
  bool checkDeviceExtensionSupport(VkPhysicalDevice device);
  void createLogicalDevice(VkSurfaceKHR surface);
};
