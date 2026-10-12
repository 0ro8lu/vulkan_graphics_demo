#pragma once

#include <memory>
#include "engine/VulkanTypes.h"

class VulkanContext;

class FramebufferAttachment
{
public:
  struct CreateInfo
  {
    uint32_t width = 0;
    uint32_t height = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    uint32_t layerCount = 1;
    VkImageUsageFlags usage = 0;
    VulkanContext* vkContext = nullptr;
    VkSamplerAddressMode samplerAddressMode =
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  };

  [[nodiscard]] static std::unique_ptr<FramebufferAttachment> create(
    const CreateInfo& info);

  ~FramebufferAttachment() noexcept;

  FramebufferAttachment(const FramebufferAttachment&) = delete;
  FramebufferAttachment& operator=(const FramebufferAttachment&) = delete;

  FramebufferAttachment(FramebufferAttachment&& other) noexcept;
  FramebufferAttachment& operator=(FramebufferAttachment&& other) noexcept;

  bool resize(uint32_t width, uint32_t height);

  [[nodiscard]] VkFormat getFormat() const noexcept { return m_format; }
  [[nodiscard]] VkImage getImage() const noexcept { return m_image; }
  [[nodiscard]] VkImageView getView() const noexcept { return m_view; }
  [[nodiscard]] VkSampler getSampler() const noexcept { return m_sampler; }
  [[nodiscard]] bool hasSampler() const noexcept { return m_sampler != VK_NULL_HANDLE; }
  [[nodiscard]] uint32_t getWidth() const noexcept { return m_width; }
  [[nodiscard]] uint32_t getHeight() const noexcept { return m_height; }
  [[nodiscard]] uint32_t getLayerCount() const noexcept { return m_layerCount; }
  [[nodiscard]] VkImageUsageFlags getUsage() const noexcept { return m_usage; }

private:
  FramebufferAttachment(const CreateInfo& info,
                        VkImage image,
                        VkImageView view,
                        VkSampler sampler,
                        VmaAllocation allocation) noexcept;

  void destroyResources() noexcept;
  [[nodiscard]] static VkImageAspectFlags determineAspectMask(
    VkImageUsageFlags usage) noexcept;

  VkFormat m_format = VK_FORMAT_UNDEFINED;
  VkImage m_image = VK_NULL_HANDLE;
  VkImageView m_view = VK_NULL_HANDLE;
  VkSampler m_sampler = VK_NULL_HANDLE;
  VmaAllocation m_allocation = VK_NULL_HANDLE;
  VkImageUsageFlags m_usage = 0;

  VulkanContext* m_vkContext = nullptr;

  uint32_t m_width = 0;
  uint32_t m_height = 0;
  uint32_t m_layerCount = 0;
};

