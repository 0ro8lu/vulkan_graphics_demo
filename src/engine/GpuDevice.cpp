#include "GpuDevice.h"

#include "engine/Error.h"
#include "engine/VulkanCheck.h"

#include <cstring>
#include <set>
#include <string_view>
#include <vector>

static QueueFamilyIndices
findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
{
  QueueFamilyIndices indices;

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(
    device, &queueFamilyCount, queueFamilies.data());

  int i = 0;
  for (const auto& queueFamily : queueFamilies) {
    if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphicsFamily = i;
    }

    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

    if (presentSupport) {
      indices.presentFamily = i;
    }

    if (indices.isComplete()) {
      break;
    }

    i++;
  }

  return indices;
}

GpuDevice::GpuDevice(Key, VkInstance instance, VkSurfaceKHR surface)
  : m_instance(instance)
{
  selectPhysicalDevice(surface);
  createLogicalDevice(surface);
}

GpuDevice::~GpuDevice()
{
  if (m_device != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(m_device);
    vkDestroyDevice(m_device, nullptr);
    m_device = VK_NULL_HANDLE;
  }
}

void
GpuDevice::waitIdle() const noexcept
{
  if (m_device != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(m_device);
  }
}

void
GpuDevice::selectPhysicalDevice(VkSurfaceKHR surface)
{
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

  if (deviceCount == 0) {
    ReportFatalError("Failed to find GPUs with Vulkan support!");
  }

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

  for (const auto& device : devices) {
    m_queueFamilies = findQueueFamilies(device, surface);
    if (isDeviceSuitable(device, surface)) {
      m_physicalDevice = device;
      break;
    }
  }

  if (m_physicalDevice == VK_NULL_HANDLE) {
    ReportFatalError("Failed to find a suitable GPU!");
  }

  vkGetPhysicalDeviceProperties(m_physicalDevice, &m_deviceProperties);
  vkGetPhysicalDeviceFeatures(m_physicalDevice, &m_deviceFeatures);
  vkGetPhysicalDeviceMemoryProperties(m_physicalDevice,
                                      &m_deviceMemoryProperties);
}

bool
GpuDevice::isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface)
{
  if (!m_queueFamilies.isComplete()) {
    return false;
  }

  bool extensionsSupported = checkDeviceExtensionSupport(device);

  bool swapChainAdequate = false;
  if (extensionsSupported) {
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(
      device, surface, &formatCount, nullptr);

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
      device, surface, &presentModeCount, nullptr);

    swapChainAdequate = (formatCount != 0) && (presentModeCount != 0);
  }

  VkPhysicalDeviceFeatures supportedFeatures;
  vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

  return extensionsSupported && swapChainAdequate &&
         supportedFeatures.samplerAnisotropy;
}

bool
GpuDevice::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
  uint32_t extensionCount = 0;
  vkEnumerateDeviceExtensionProperties(
    device, nullptr, &extensionCount, nullptr);

  std::vector<VkExtensionProperties> availableExtensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(
    device, nullptr, &extensionCount, availableExtensions.data());

  std::set<std::string_view> requiredExtensions(kDeviceExtensions.begin(),
                                                kDeviceExtensions.end());

  for (const auto& extension : availableExtensions) {
    requiredExtensions.erase(extension.extensionName);
  }

  return requiredExtensions.empty();
}

void
GpuDevice::createLogicalDevice(VkSurfaceKHR)
{
  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> uniqueQueueFamilies = {
    m_queueFamilies.graphicsFamily.value(),
    m_queueFamilies.presentFamily.value()
  };

  float queuePriority = 1.0f;
  for (uint32_t queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
  }

  VkPhysicalDeviceFeatures enabledDeviceFeatures{};
  enabledDeviceFeatures.samplerAnisotropy = VK_TRUE;

  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.pEnabledFeatures = &enabledDeviceFeatures;
  createInfo.queueCreateInfoCount =
    static_cast<uint32_t>(queueCreateInfos.size());
  createInfo.pQueueCreateInfos = queueCreateInfos.data();
  createInfo.enabledExtensionCount =
    static_cast<uint32_t>(kDeviceExtensions.size());
  createInfo.ppEnabledExtensionNames = kDeviceExtensions.data();
  createInfo.enabledLayerCount = 0;

  VK_CHECK(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device));

  vkGetDeviceQueue(
    m_device, m_queueFamilies.graphicsFamily.value(), 0, &m_graphicsQueue);
  vkGetDeviceQueue(
    m_device, m_queueFamilies.presentFamily.value(), 0, &m_presentQueue);
}

VkFormat
GpuDevice::findSupportedFormat(std::span<const VkFormat> candidates,
                               VkImageTiling tiling,
                               VkFormatFeatureFlags features) const
{
  for (VkFormat format : candidates) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);

    if (tiling == VK_IMAGE_TILING_LINEAR &&
        (props.linearTilingFeatures & features) == features) {
      return format;
    } else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
               (props.optimalTilingFeatures & features) == features) {
      return format;
    }
  }

  ReportFatalError("Failed to find supported format!");
}

VkImageView
GpuDevice::createImageView(VkImage image,
                           VkFormat format,
                           uint32_t layerCount,
                           VkImageAspectFlags aspectFlags,
                           VkImageViewType viewType,
                           uint32_t levelCount,
                           uint32_t baseMipLevel) const
{
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = image;
  viewInfo.viewType = viewType;
  viewInfo.format = format;
  viewInfo.subresourceRange.aspectMask = aspectFlags;
  viewInfo.subresourceRange.baseMipLevel = baseMipLevel;
  viewInfo.subresourceRange.levelCount = levelCount;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = layerCount;

  VkImageView imageView = VK_NULL_HANDLE;
  VK_CHECK(vkCreateImageView(m_device, &viewInfo, nullptr, &imageView));

  return imageView;
}

void
GpuDevice::destroyImageView(VkImageView view) const noexcept
{
  if (view != VK_NULL_HANDLE) {
    vkDestroyImageView(m_device, view, nullptr);
  }
}
