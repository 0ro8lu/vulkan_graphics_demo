#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

struct FrameContext
{
  VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
  VkExtent2D extent = {};
  VkFramebuffer backbuffer = VK_NULL_HANDLE;
  uint32_t currentFrame = 0;
  uint32_t imageIndex = 0;
};
