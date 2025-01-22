#ifndef VULKAN_DEVICE_H_
#define VULKAN_DEVICE_H_

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <vector>

class VulkanDevice
{
public:
  VulkanDevice(VkPhysicalDevice physicalDevice,
               VkDevice logicalDevice,
               VkCommandPool commandPool);
  VulkanDevice(VkPhysicalDevice physicalDevice,
               VkDevice logicalDevice,
               VmaAllocator allocator,
               VkCommandPool commandPool,
               VkQueue queue);

  VulkanDevice(VkPhysicalDevice physicalDevice);

  ~VulkanDevice();

  enum BufferType
  {
    STAGING_BUFFER,
    GPU_BUFFER,
  };

  void createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures,
                           std::vector<const char*> deviceExtensions,
                           std::optional<uint32_t> graphicsIndex,
                           std::optional<uint32_t> presentIndex);
  void createVMAAllocator(VkInstance instance);

  void createImage(uint32_t width,
                   uint32_t height,
                   VkFormat format,
                   VkImageTiling tiling,
                   VkImageUsageFlags usage,
                   VmaAllocationCreateFlagBits flags,
                   VkImage& image,
                   VmaAllocation& allocation);

  VkImageView createImageView(VkImage image,
                              VkFormat format,
                              VkImageAspectFlags aspectFlags);

  void* createBuffer(VkDeviceSize size,
                     VkBufferUsageFlags usage,
                     BufferType bufferType,
                     VkBuffer& buffer,
                     VmaAllocation& allocation);

  void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
  VkCommandBuffer beginSingleTimeCommands();
  void endSingleTimeCommands(VkCommandBuffer commandBuffer);

  VkPhysicalDevice physicalDevice;
  VkDevice logicalDevice;
  VmaAllocator allocator;
  VkQueue queue;
  VkCommandPool commandPool;
};

#endif
