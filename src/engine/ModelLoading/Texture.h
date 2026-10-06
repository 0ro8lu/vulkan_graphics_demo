#pragma once

#include <string>

#include "engine/VulkanContext.h"

class Texture
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
    view = other.view;
    sampler = other.sampler;
    image = other.image;
    allocation = other.allocation;
    vkContext = other.vkContext;

    other.view = VK_NULL_HANDLE;
    other.sampler = VK_NULL_HANDLE;
    other.image = VK_NULL_HANDLE;
    other.allocation = VK_NULL_HANDLE;
    other.vkContext = nullptr;
  }

  // Move assignment operator
  Texture& operator=(Texture&& other) noexcept
  {
    if (this != &other) {
      cleanup();

      view = other.view;
      sampler = other.sampler;
      image = other.image;
      allocation = other.allocation;
      vkContext = other.vkContext;

      other.view = VK_NULL_HANDLE;
      other.sampler = VK_NULL_HANDLE;
      other.image = VK_NULL_HANDLE;
      other.allocation = VK_NULL_HANDLE;
      other.vkContext = nullptr;
    }
    return *this;
  }

  ~Texture();

  VkImageView view = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;

private:
  VkImage image = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;

  VulkanContext* vkContext;

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
