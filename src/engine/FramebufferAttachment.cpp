#include "engine/FramebufferAttachment.h"

#include <stdexcept>

FramebufferAttachment::FramebufferAttachment(VkFormat format,
                                             VkImageUsageFlags usage,
                                             uint32_t width,
                                             uint32_t height,
                                             VulkanContext* vkContext)
  : vkContext(vkContext)
  , format(format)
  , width(width)
  , height(height)
  , usage(usage)
{
  create();
}

void
FramebufferAttachment::resize(uint32_t width, uint32_t height)
{
  this->width = width;
  this->height = height;

  vmaDestroyImage(vkContext->allocator, image, allocation);
  vkDestroyImageView(vkContext->logicalDevice, view, nullptr);

  create();
}

void
FramebufferAttachment::create()
{
  VkImageAspectFlags aspectMask = 0;
  VkImageLayout imageLayout;

  if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
    aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }
  if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  }

  image = vkContext->createImage(width,
                                 height,
                                 format,
                                 VK_IMAGE_TILING_OPTIMAL,
                                 usage | VK_IMAGE_USAGE_SAMPLED_BIT |
                                   VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
                                 // usage | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
                                 VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                                 allocation);

  view = vkContext->createImageView(image, format, aspectMask);

  // create sampler
  if (sampler == VK_NULL_HANDLE) {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 1.0;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(
          vkContext->logicalDevice, &samplerInfo, nullptr, &sampler) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to create texture sampler!");
    }
  }
}

FramebufferAttachment::~FramebufferAttachment()
{
  vmaDestroyImage(vkContext->allocator, image, allocation);
  vkDestroyImageView(vkContext->logicalDevice, view, nullptr);
  vkDestroySampler(vkContext->logicalDevice, sampler, nullptr);
  vkContext = nullptr;
}
