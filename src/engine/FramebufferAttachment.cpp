#include "engine/FramebufferAttachment.h"
#include "engine/VulkanContext.h"

#include <stdexcept>
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

  try {
    image = info.vkContext->createImage(
      info.width,
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
      } else if (info.samplerAddressMode == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER) {
        sampler = info.vkContext->getSampler(SamplerType::LinearClampToBorder);
      } else {
        sampler = info.vkContext->getSampler(SamplerType::LinearRepeat);
      }
    }
  } catch (...) {
    if (view != VK_NULL_HANDLE) {
      info.vkContext->destroyImageView(view);
    }
    if (image != VK_NULL_HANDLE) {
      info.vkContext->destroyImage(image, allocation);
    }
    throw;
  }

  return std::unique_ptr<FramebufferAttachment>(
    new FramebufferAttachment(info, image, view, sampler, allocation));
}

FramebufferAttachment::FramebufferAttachment(const CreateInfo& info,
                                             VkImage image,
                                             VkImageView view,
                                             VkSampler sampler,
                                             VmaAllocation allocation) noexcept
  : m_Format(info.format)
  , m_Image(image)
  , m_View(view)
  , m_Sampler(sampler)
  , m_Allocation(allocation)
  , m_Usage(info.usage)
  , m_VkContext(info.vkContext)
  , m_Width(info.width)
  , m_Height(info.height)
  , m_LayerCount(info.layerCount)
{
}

FramebufferAttachment::~FramebufferAttachment() noexcept
{
  destroyResources();
}

void
FramebufferAttachment::destroyResources() noexcept
{
  if (m_VkContext) {
    m_Sampler = VK_NULL_HANDLE; // We don't own the sampler anymore
    if (m_View != VK_NULL_HANDLE) {
      m_VkContext->destroyImageView(m_View);
      m_View = VK_NULL_HANDLE;
    }
    if (m_Image != VK_NULL_HANDLE) {
      m_VkContext->destroyImage(m_Image, m_Allocation);
      m_Image = VK_NULL_HANDLE;
      m_Allocation = VK_NULL_HANDLE;
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
  : m_Format(std::exchange(other.m_Format, VK_FORMAT_UNDEFINED))
  , m_Image(std::exchange(other.m_Image, VK_NULL_HANDLE))
  , m_View(std::exchange(other.m_View, VK_NULL_HANDLE))
  , m_Sampler(std::exchange(other.m_Sampler, VK_NULL_HANDLE))
  , m_Allocation(std::exchange(other.m_Allocation, VK_NULL_HANDLE))
  , m_Usage(std::exchange(other.m_Usage, 0))
  , m_VkContext(std::exchange(other.m_VkContext, nullptr))
  , m_Width(std::exchange(other.m_Width, 0))
  , m_Height(std::exchange(other.m_Height, 0))
  , m_LayerCount(std::exchange(other.m_LayerCount, 0))
{
}

FramebufferAttachment&
FramebufferAttachment::operator=(FramebufferAttachment&& other) noexcept
{
  if (this != &other) {
    destroyResources();

    m_Format = std::exchange(other.m_Format, VK_FORMAT_UNDEFINED);
    m_Image = std::exchange(other.m_Image, VK_NULL_HANDLE);
    m_View = std::exchange(other.m_View, VK_NULL_HANDLE);
    m_Sampler = std::exchange(other.m_Sampler, VK_NULL_HANDLE);
    m_Allocation = std::exchange(other.m_Allocation, VK_NULL_HANDLE);
    m_Usage = std::exchange(other.m_Usage, 0);
    m_VkContext = std::exchange(other.m_VkContext, nullptr);
    m_Width = std::exchange(other.m_Width, 0);
    m_Height = std::exchange(other.m_Height, 0);
    m_LayerCount = std::exchange(other.m_LayerCount, 0);
  }
  return *this;
}

bool
FramebufferAttachment::resize(uint32_t width, uint32_t height)
{
  if (m_Width == width && m_Height == height) {
    return true;
  }

  if (width == 0 || height == 0 || !m_VkContext) {
    return false;
  }

  const VkImageAspectFlags aspectMask = determineAspectMask(m_Usage);

  VkImageUsageFlags imageUsage = m_Usage;
  if (m_Usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
    imageUsage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
  }

  VkImage newImage = VK_NULL_HANDLE;
  VmaAllocation newAllocation = VK_NULL_HANDLE;
  VkImageView newView = VK_NULL_HANDLE;

  try {
    newImage = m_VkContext->createImage(
      width,
      height,
      m_Format,
      m_LayerCount,
      VK_IMAGE_TILING_OPTIMAL,
      imageUsage,
      newAllocation);

    newView = m_VkContext->createImageView(
      newImage, m_Format, m_LayerCount, aspectMask);
  } catch (...) {
    if (newView != VK_NULL_HANDLE) {
      m_VkContext->destroyImageView(newView);
    }
    if (newImage != VK_NULL_HANDLE) {
      m_VkContext->destroyImage(newImage, newAllocation);
    }
    return false;
  }

  // Destroy old resources in correct Vulkan order: View before Image!
  if (m_View != VK_NULL_HANDLE) {
    m_VkContext->destroyImageView(m_View);
  }
  if (m_Image != VK_NULL_HANDLE) {
    m_VkContext->destroyImage(m_Image, m_Allocation);
  }

  m_Image = newImage;
  m_Allocation = newAllocation;
  m_View = newView;
  m_Width = width;
  m_Height = height;

  return true;
}

