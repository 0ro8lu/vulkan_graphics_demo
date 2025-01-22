#ifndef _FRAMEBUFFER_ATTACHMENT_H_
#define _FRAMEBUFFER_ATTACHMENT_H_

#include "engine/VulkanDevice.h"

class FrameBufferAttachment
{
public:
  FrameBufferAttachment(VkFormat format,
                        VkImageUsageFlags usage,
                        uint32_t width,
                        uint32_t height,
                        VulkanDevice* vulkanDevice);
  ~FrameBufferAttachment();

  VkImage image = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VmaAllocation allocation;
  VulkanDevice* vulkanDevice;
};

#endif
