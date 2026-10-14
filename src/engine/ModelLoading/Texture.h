#pragma once

#include <string>
#include <utility>

#include "engine/VulkanTypes.h"

class GpuDevice;
class GpuAllocator;
class SamplerCache;
class ImmediateCommands;

class Texture final
{
public:
  Texture() = default;
  Texture(const GpuDevice& device,
          const GpuAllocator& allocator,
          const SamplerCache& samplerCache,
          const ImmediateCommands& immediateCommands,
          unsigned char* data,
          size_t size);
  Texture(const GpuDevice& device,
          const GpuAllocator& allocator,
          const SamplerCache& samplerCache,
          const ImmediateCommands& immediateCommands,
          std::string filePath);

  Texture(const Texture&) = delete;
  Texture& operator=(const Texture&) = delete;

  // Move constructor
  Texture(Texture&& other) noexcept
    : m_view(std::exchange(other.m_view, VK_NULL_HANDLE))
    , m_sampler(std::exchange(other.m_sampler, VK_NULL_HANDLE))
    , m_image(std::exchange(other.m_image, VK_NULL_HANDLE))
    , m_allocation(std::exchange(other.m_allocation, VK_NULL_HANDLE))
    , m_device(std::exchange(other.m_device, nullptr))
    , m_allocator(std::exchange(other.m_allocator, nullptr))
    , m_immediateCommands(std::exchange(other.m_immediateCommands, nullptr))
  {
  }

  // Move assignment operator
  Texture& operator=(Texture&& other) noexcept
  {
    if (this != &other) {
      cleanup();

      m_view = std::exchange(other.m_view, VK_NULL_HANDLE);
      m_sampler = std::exchange(other.m_sampler, VK_NULL_HANDLE);
      m_image = std::exchange(other.m_image, VK_NULL_HANDLE);
      m_allocation = std::exchange(other.m_allocation, VK_NULL_HANDLE);
      m_device = std::exchange(other.m_device, nullptr);
      m_allocator = std::exchange(other.m_allocator, nullptr);
      m_immediateCommands =
        std::exchange(other.m_immediateCommands, nullptr);
    }
    return *this;
  }

  ~Texture();

  VkImageView m_view = VK_NULL_HANDLE;
  VkSampler m_sampler = VK_NULL_HANDLE;

private:
  VkImage m_image = VK_NULL_HANDLE;
  VmaAllocation m_allocation = VK_NULL_HANDLE;

  const GpuDevice* m_device = nullptr;
  const GpuAllocator* m_allocator = nullptr;
  const ImmediateCommands* m_immediateCommands = nullptr;

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
