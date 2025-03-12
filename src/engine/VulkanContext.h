#ifndef _VULKAN_CONTEXT_H_
#define _VULKAN_CONTEXT_H_

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <vector>

#include "engine/Buffers.h"

class VulkanInitializer;

class VulkanContext
{
  friend class VulkanInitializer;

  VulkanContext() {};

public:
  VkInstance instance;

  VkPhysicalDevice physicalDevice;
  VkDevice logicalDevice;

  VmaAllocator allocator;

  VkQueue graphicsQueue;
  VkQueue presentQueue;

  VkCommandPool commandPool;

  // create vulkan primitives
  VkImage createImage(uint32_t width,
                      uint32_t height,
                      VkFormat format,
                      uint32_t layerCount,
                      VkImageTiling tiling,
                      VkImageUsageFlags usage,
                      VmaAllocationCreateFlagBits flags,
                      VmaAllocation& allocation);

  VkShaderModule createShaderModule(const std::vector<char>& code);

  VkImageView createImageView(VkImage image,
                              VkFormat format,
                              uint32_t layerCount,
                              VkImageAspectFlags aspectFlags);

  void* createBuffer(VkDeviceSize size,
                     VkBufferUsageFlags usage,
                     BufferType bufferType,
                     VkBuffer& buffer,
                     VmaAllocation& allocation);

  VkCommandBuffer beginSingleTimeCommands();
  void endSingleTimeCommands(VkCommandBuffer commandBuffer);

  // void copyBuffer
  void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

  // destroy vulkan primitives
  void destroyImageView(VkImageView view);

  VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
                               VkImageTiling tiling,
                               VkFormatFeatureFlags features);
};

#endif
