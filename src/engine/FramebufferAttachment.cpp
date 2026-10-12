#include "engine/FramebufferAttachment.h"
#include "engine/VulkanContext.h"

#include <utility>

std::unique_ptr<FramebufferAttachment>
FramebufferAttachment::create(const CreateInfo& info)
{
  if (!info.vkContext) {
    return nullptr;
  }
  if (info.width == 0 || info.height == 0 || info.layerCount == 0) {
    return nullptr;
  }

  const VkImageAspectFlags aspectMask = determineAspectMask(info.usage);

  VkImageUsageFlags imageUsage = info.usage;
  if (info.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
    imageUsage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
  }

  VkImage image = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;

  image = info.vkContext->createImage(info.width,
                                      info.height,
                                      info.format,
                                      info.layerCount,
                                      VK_IMAGE_TILING_OPTIMAL,
                                      imageUsage,
                                      allocation);

  view = info.vkContext->createImageView(
    image, info.format, info.layerCount, aspectMask);

  // Only create a sampler if the attachment is actually sampled
  if (info.usage & VK_IMAGE_USAGE_SAMPLED_BIT) {
    if (info.samplerAddressMode == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE) {
      sampler = info.vkContext->getSampler(SamplerType::LinearClampToEdge);
    } else if (info.samplerAddressMode ==
               VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER) {
      sampler = info.vkContext->getSampler(SamplerType::LinearClampToBorder);
    } else {
      sampler = info.vkContext->getSampler(SamplerType::LinearRepeat);
    }
  }

  return std::unique_ptr<FramebufferAttachment>(
    new FramebufferAttachment(info, image, view, sampler, allocation));
}

FramebufferAttachment::FramebufferAttachment(const CreateInfo& info,
                                             VkImage image,
                                             VkImageView view,
                                             VkSampler sampler,
                                             VmaAllocation allocation) noexcept
  : m_format(info.format)
  , m_image(image)
  , m_view(view)
  , m_sampler(sampler)
  , m_allocation(allocation)
  , m_usage(info.usage)
  , m_vkContext(info.vkContext)
  , m_width(info.width)
  , m_height(info.height)
  , m_layerCount(info.layerCount)
{
}

FramebufferAttachment::~FramebufferAttachment() noexcept
{
  destroyResources();
}

void
FramebufferAttachment::destroyResources() noexcept
{
  if (m_vkContext) {
    m_sampler = VK_NULL_HANDLE; // We don't own the sampler anymore
    if (m_view != VK_NULL_HANDLE) {
      m_vkContext->destroyImageView(m_view);
      m_view = VK_NULL_HANDLE;
    }
    if (m_image != VK_NULL_HANDLE) {
      m_vkContext->destroyImage(m_image, m_allocation);
      m_image = VK_NULL_HANDLE;
      m_allocation = VK_NULL_HANDLE;
    }
  }
}

VkImageAspectFlags
FramebufferAttachment::determineAspectMask(VkImageUsageFlags usage) noexcept
{
  VkImageAspectFlags aspectMask = 0;
  if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
    aspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
  }
  if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
  }
  return aspectMask;
}

FramebufferAttachment::FramebufferAttachment(
  FramebufferAttachment&& other) noexcept
  : m_format(std::exchange(other.m_format, VK_FORMAT_UNDEFINED))
  , m_image(std::exchange(other.m_image, VK_NULL_HANDLE))
  , m_view(std::exchange(other.m_view, VK_NULL_HANDLE))
  , m_sampler(std::exchange(other.m_sampler, VK_NULL_HANDLE))
  , m_allocation(std::exchange(other.m_allocation, VK_NULL_HANDLE))
  , m_usage(std::exchange(other.m_usage, 0))
  , m_vkContext(std::exchange(other.m_vkContext, nullptr))
  , m_width(std::exchange(other.m_width, 0))
  , m_height(std::exchange(other.m_height, 0))
  , m_layerCount(std::exchange(other.m_layerCount, 0))
{
}

FramebufferAttachment&
FramebufferAttachment::operator=(FramebufferAttachment&& other) noexcept
{
  if (this != &other) {
    destroyResources();

    m_format = std::exchange(other.m_format, VK_FORMAT_UNDEFINED);
    m_image = std::exchange(other.m_image, VK_NULL_HANDLE);
    m_view = std::exchange(other.m_view, VK_NULL_HANDLE);
    m_sampler = std::exchange(other.m_sampler, VK_NULL_HANDLE);
    m_allocation = std::exchange(other.m_allocation, VK_NULL_HANDLE);
    m_usage = std::exchange(other.m_usage, 0);
    m_vkContext = std::exchange(other.m_vkContext, nullptr);
    m_width = std::exchange(other.m_width, 0);
    m_height = std::exchange(other.m_height, 0);
    m_layerCount = std::exchange(other.m_layerCount, 0);
  }
  return *this;
}

bool
FramebufferAttachment::resize(uint32_t width, uint32_t height)
{
  if (m_width == width && m_height == height) {
    return true;
  }

  if (width == 0 || height == 0 || !m_vkContext) {
    return false;
  }

  const VkImageAspectFlags aspectMask = determineAspectMask(m_usage);

  VkImageUsageFlags imageUsage = m_usage;
  if (m_usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
    imageUsage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
  }

  VkImage newImage = VK_NULL_HANDLE;
  VmaAllocation newAllocation = VK_NULL_HANDLE;
  VkImageView newView = VK_NULL_HANDLE;

  newImage = m_vkContext->createImage(width,
                                      height,
                                      m_format,
                                      m_layerCount,
                                      VK_IMAGE_TILING_OPTIMAL,
                                      imageUsage,
                                      newAllocation);

  newView = m_vkContext->createImageView(
    newImage, m_format, m_layerCount, aspectMask);

  // Destroy old resources in correct Vulkan order: View before Image!
  if (m_view != VK_NULL_HANDLE) {
    m_vkContext->destroyImageView(m_view);
  }
  if (m_image != VK_NULL_HANDLE) {
    m_vkContext->destroyImage(m_image, m_allocation);
  }

  m_image = newImage;
  m_allocation = newAllocation;
  m_view = newView;
  m_width = width;
  m_height = height;

  return true;
}
