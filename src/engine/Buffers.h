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
};

// --------------------- Camera Buffer ---------------------
struct CameraBuffer
{
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
  alignas(16) glm::vec4 cameraPos;
};

// --------------------- Lights ---------------------
struct PointLight
{
  alignas(16) glm::vec4 position;
  alignas(16) glm::vec4 color;
};

struct DirectionalLight
{
  alignas(16) glm::vec4 direction;
  alignas(16) glm::vec4 color;
};

struct SpotLight
{
  alignas(16) glm::vec4 direction;
  alignas(16) glm::vec4 position;
  alignas(16) glm::vec4 color;
};

#endif
