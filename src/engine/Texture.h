#ifndef _TEXTURE_H_
#define _TEXTURE_H_

#include <stb_image.h>

#include <string>
#include <stdexcept>

#include "VulkanDevice.h"

class Texture
{
public:
  Texture();
  Texture(VulkanDevice* vulkanDevice, unsigned char* data, size_t size);
  Texture(VulkanDevice* vulkanDevice, std::string filePath);

  // try to delete copy constructor
  Texture(Texture& texture) = delete;
  Texture& operator=(Texture& texture) = delete;

  // Move constructor
  Texture(Texture&& other) noexcept
  {
    view = other.view;
    sampler = other.sampler;
    image = other.image;
    allocation = other.allocation;
    vulkanDevice = other.vulkanDevice;

    other.view = VK_NULL_HANDLE;
    other.sampler = VK_NULL_HANDLE;
    other.image = VK_NULL_HANDLE;
    other.allocation = VK_NULL_HANDLE;
    other.vulkanDevice = nullptr;
  }

  // Move assignment operator
  Texture& operator=(Texture&& other)
  {
    if (this != &other) {
      cleanup();

      view = other.view;
      sampler = other.sampler;
      image = other.image;
      allocation = other.allocation;
      vulkanDevice = other.vulkanDevice;

      other.view = VK_NULL_HANDLE;
      other.sampler = VK_NULL_HANDLE;
      other.image = VK_NULL_HANDLE;
      other.allocation = VK_NULL_HANDLE;
      other.vulkanDevice = nullptr;
    }
    return *this;
  }

  ~Texture();

  VkImageView view = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;

private:
  VkImage image = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;

  VulkanDevice* vulkanDevice = nullptr;

  void cleanup();

  void createVulkanImage(int width, int height);
  void createTextureImageFromPixels(stbi_uc* pixels,
                                    int texWidth,
                                    int texHeight);

  void createTextureImageView();

  void transitionImageLayout(VkImage image,
                             VkFormat format,
                             VkImageLayout oldLayout,
                             VkImageLayout newLayout);

  void copyBufferToImage(VkBuffer buffer,
                         VkImage image,
                         uint32_t width,
                         uint32_t height);

  VkCommandBuffer beginSingleTimeCommands();
  void endSingleTimeCommands(VkCommandBuffer commandBuffer);
  void createTextureSampler();
};

#endif
