#include "FrameBufferAttachment.h"
#include "engine/VulkanDevice.h"
#include <vulkan/vulkan_core.h>

FrameBufferAttachment::FrameBufferAttachment(VkFormat format,
                                             VkImageUsageFlags usage,
                                             uint32_t width,
                                             uint32_t height,
                                             VulkanDevice* vulkanDevice)
  : vulkanDevice(vulkanDevice)
{
  VkImageAspectFlags aspectMask = 0;
  VkImageLayout imageLayout;

  if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
    aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    // imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }
  if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    // imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  }

  vulkanDevice->createImage(width,
                            height,
                            format,
                            VK_IMAGE_TILING_OPTIMAL,
                            usage | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
                            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                            image,
                            allocation);

  view = vulkanDevice->createImageView(image, format, aspectMask);
}

FrameBufferAttachment::~FrameBufferAttachment()
{
  vmaDestroyImage(vulkanDevice->allocator, image, allocation);
  vkDestroyImageView(vulkanDevice->logicalDevice, view, nullptr);
  vulkanDevice = nullptr;
}
