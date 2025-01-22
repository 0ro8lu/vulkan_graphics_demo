#include "VulkanDevice.h"

#include <set>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

VulkanDevice::VulkanDevice(VkPhysicalDevice physicalDevice,
                           VkDevice logicalDevice,
                           VkCommandPool commandPool)
  : physicalDevice(physicalDevice)
  , logicalDevice(logicalDevice)
  , commandPool(commandPool)
{
}
VulkanDevice::VulkanDevice(VkPhysicalDevice physicalDevice,
                           VkDevice logicalDevice,
                           VmaAllocator allocator,
                           VkCommandPool commandPool,
                           VkQueue queue)
  : physicalDevice(physicalDevice)
  , logicalDevice(logicalDevice)
  , allocator(allocator)
  , commandPool(commandPool)
  , queue(queue)
{
}

VulkanDevice::VulkanDevice(VkPhysicalDevice physicalDevice)
  : physicalDevice(physicalDevice)
{
}

VulkanDevice::~VulkanDevice()
{
  // TODO: cleanup allocator
}

void
VulkanDevice::createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures,
                                  std::vector<const char*> deviceExtensions,
                                  std::optional<uint32_t> graphicsIndex,
                                  std::optional<uint32_t> presentIndex)
{
  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> uniqueQueueFamilies = { graphicsIndex.value(),
                                             presentIndex.value() };

  float queuePriority = 1.0f;
  for (uint32_t queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
  }

  VkPhysicalDeviceFeatures deviceFeatures{};
  deviceFeatures.samplerAnisotropy = VK_TRUE;

  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.pEnabledFeatures = &deviceFeatures;
  createInfo.queueCreateInfoCount =
    static_cast<uint32_t>(queueCreateInfos.size());
  createInfo.pQueueCreateInfos = queueCreateInfos.data();
  createInfo.enabledExtensionCount =
    static_cast<uint32_t>(deviceExtensions.size());
  createInfo.ppEnabledExtensionNames = deviceExtensions.data();

  if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &logicalDevice) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create logical device!");
  }
}

void*
VulkanDevice::createBuffer(VkDeviceSize size,
                           VkBufferUsageFlags usage,
                           BufferType bufferType,
                           VkBuffer& buffer,
                           VmaAllocation& allocation)
{
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocCreateInfo = {};
  allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

  switch (bufferType) {
    case BufferType::STAGING_BUFFER:
      allocCreateInfo.flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT;
      break;
    case BufferType::GPU_BUFFER:
      allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
      allocCreateInfo.priority = 1.0f;
      break;
  }

  VmaAllocationInfo allocInfo;
  if (vmaCreateBuffer(allocator,
                      &bufferInfo,
                      &allocCreateInfo,
                      &buffer,
                      &allocation,
                      &allocInfo) != VK_SUCCESS) {
    throw std::runtime_error("failed to create buffer!");
  }

  return allocInfo.pMappedData;
}

void
VulkanDevice::copyBuffer(VkBuffer srcBuffer,
                         VkBuffer dstBuffer,
                         VkDeviceSize size)
{
  VkCommandBuffer commandBuffer = beginSingleTimeCommands();

  VkBufferCopy copyRegion{};
  copyRegion.size = size;
  vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

  endSingleTimeCommands(commandBuffer);
}

VkCommandBuffer
VulkanDevice::beginSingleTimeCommands()
{
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = commandPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(logicalDevice, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);
  return commandBuffer;
}

void
VulkanDevice::endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(queue);

  vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
}

void
VulkanDevice::createVMAAllocator(VkInstance instance)
{
  VmaAllocatorCreateInfo createInfo = {};
  createInfo.physicalDevice = physicalDevice;
  createInfo.device = logicalDevice;
  createInfo.instance = instance;

  if (vmaCreateAllocator(&createInfo, &allocator) != VK_SUCCESS) {
    throw std::runtime_error("failed to create vma allocator!");
  }
}

void
VulkanDevice::createImage(uint32_t width,
                          uint32_t height,
                          VkFormat format,
                          VkImageTiling tiling,
                          VkImageUsageFlags usage,
                          VmaAllocationCreateFlagBits flags,
                          VkImage& image,
                          VmaAllocation& allocation)
{
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = width;
  imageInfo.extent.height = height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = format;
  imageInfo.tiling = tiling;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = usage;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocCreateInfo = {};
  allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
  allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  allocCreateInfo.priority = 1.0f;

  if (vmaCreateImage(allocator,
                     &imageInfo,
                     &allocCreateInfo,
                     &image,
                     &allocation,
                     nullptr) != VK_SUCCESS) {
    throw std::runtime_error("failed to create image!");
  }
}

VkImageView
VulkanDevice::createImageView(VkImage image,
                              VkFormat format,
                              VkImageAspectFlags aspectFlags)
{
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = format;
  viewInfo.subresourceRange.aspectMask = aspectFlags;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  VkImageView imageView;
  if (vkCreateImageView(logicalDevice, &viewInfo, nullptr, &imageView) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create texture image view!");
  }

  return imageView;
}
