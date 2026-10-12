#pragma once

#include <string>

#include "engine/VulkanContext.h"

class Texture final
{
public:
  Texture();
  Texture(VulkanContext* vkContext, unsigned char* data, size_t size);
  Texture(VulkanContext* vkContext, std::string filePath);

  Texture(const Texture&) = delete;
  Texture& operator=(const Texture&) = delete;

  // Move constructor
  Texture(Texture&& other) noexcept
  {
    m_view = other.m_view;
    m_sampler = other.m_sampler;
    m_image = other.m_image;
    m_allocation = other.m_allocation;
    m_vkContext = other.m_vkContext;

    other.m_view = VK_NULL_HANDLE;
    other.m_sampler = VK_NULL_HANDLE;
    other.m_image = VK_NULL_HANDLE;
    other.m_allocation = VK_NULL_HANDLE;
    other.m_vkContext = nullptr;
  }

  // Move assignment operator
  Texture& operator=(Texture&& other) noexcept
  {
    if (this != &other) {
      cleanup();

      m_view = other.m_view;
      m_sampler = other.m_sampler;
      m_image = other.m_image;
      m_allocation = other.m_allocation;
      m_vkContext = other.m_vkContext;

      other.m_view = VK_NULL_HANDLE;
      other.m_sampler = VK_NULL_HANDLE;
      other.m_image = VK_NULL_HANDLE;
      other.m_allocation = VK_NULL_HANDLE;
      other.m_vkContext = nullptr;
    }
    return *this;
  }

  ~Texture();

  VkImageView m_view = VK_NULL_HANDLE;
  VkSampler m_sampler = VK_NULL_HANDLE;

private:
  VkImage m_image = VK_NULL_HANDLE;
  VmaAllocation m_allocation = VK_NULL_HANDLE;

  VulkanContext* m_vkContext;

  void cleanup();

  void createTextureImageFromPixels(unsigned char* pixels,
                                    int texWidth,
                                    int texHeight);

  void transitionImageLayout(VkImage image,
                             VkImageLayout oldLayout,
                             VkImageLayout newLayout);

  void copyBufferToImage(VkBuffer buffer,
                         VkImage image,
                         uint32_t width,
                         uint32_t height);
};
