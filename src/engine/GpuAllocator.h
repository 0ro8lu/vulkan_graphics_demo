#pragma once

#include "engine/VulkanTypes.h"

class GpuDevice;

class GpuAllocator final
{
public:
  explicit GpuAllocator(const GpuDevice& device);
  ~GpuAllocator();

  GpuAllocator(const GpuAllocator&) = delete;
  GpuAllocator& operator=(const GpuAllocator&) = delete;
  GpuAllocator(GpuAllocator&&) = delete;
  GpuAllocator& operator=(GpuAllocator&&) = delete;

  [[nodiscard]] VmaAllocator allocator() const noexcept { return m_allocator; }

  // --- Buffer management ---

  [[nodiscard]] VulkanBuffer createBuffer(VkDeviceSize size,
                                          VkBufferUsageFlags usage,
                                          BufferType bufferType) const;

  void destroyBuffer(VulkanBuffer& buffer) const noexcept;
  void destroyBuffer(VkBuffer buffer, VmaAllocation allocation) const noexcept;

  // --- Image management ---

  [[nodiscard]] VkImage createImage(uint32_t width,
                                    uint32_t height,
                                    VkFormat format,
                                    uint32_t layerCount,
                                    VkImageTiling tiling,
                                    VkImageUsageFlags usage,
                                    VmaAllocation& allocation,
                                    VkImageCreateFlags imageCreateFlags = 0,
                                    uint32_t mipLevels = 1) const;

  void destroyImage(VkImage image, VmaAllocation allocation) const noexcept;

private:
  const GpuDevice& m_device;
  VmaAllocator m_allocator{ VK_NULL_HANDLE };
};
