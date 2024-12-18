#ifndef VULKAN_DEVICE_H_
#define VULKAN_DEVICE_H_

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

class VulkanDevice
{
public:
  VulkanDevice(VkPhysicalDevice physicalDevice,
               VkDevice logicalDevice,
               VmaAllocator allocator,
               VkCommandPool commandPool,
               VkQueue queue);

  ~VulkanDevice();

  enum BufferType
  {
    STAGING_BUFFER,
    GPU_BUFFER,
  };

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
  VkCommandPool commandPool;
  VkQueue queue;
};

#endif
