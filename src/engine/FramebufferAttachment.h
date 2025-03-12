#ifndef _FRAMEBUFFER_ATTACHMENT_H_
#define _FRAMEBUFFER_ATTACHMENT_H_

#include "engine/VulkanContext.h"

class FramebufferAttachment
{
public:
  FramebufferAttachment(VkFormat format,
                        uint32_t layerCount,
                        VkImageUsageFlags usage,
                        uint32_t width,
                        uint32_t height,
                        VulkanContext* vkContext);
  ~FramebufferAttachment();

  void resize(uint32_t width, uint32_t height);

  VkFormat format;
  VkImage image = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;
  VmaAllocation allocation;
  VkImageUsageFlags usage;

  VulkanContext* vkContext;

  uint32_t width;
  uint32_t height;
  uint32_t layerCount;

private:
  void cleanup();
  void create();
};

#endif
