#ifndef _BUFFERS_H_
#define _BUFFERS_H_

#include <glm.hpp>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

enum BufferType
{
  STAGING_BUFFER,
  GPU_BUFFER,
};

// --------------------- Vulkan Buffer Definition ---------------------
struct VulkanBufferDefinition
{
  VkBuffer buffer;
  VmaAllocation allocation;
  void* mapped;
  size_t size;
};

#endif
