#include "GpuAllocator.h"

#include "GpuDevice.h"
#include "engine/VulkanCheck.h"

#include <vk_mem_alloc.h>

GpuAllocator::GpuAllocator(const GpuDevice& device)
  : m_device(device)
{
  VmaAllocatorCreateInfo createInfo{};
  createInfo.physicalDevice = m_device.physicalDevice();
  createInfo.device = m_device.device();
  createInfo.instance = m_device.instance();

  VK_CHECK(vmaCreateAllocator(&createInfo, &m_allocator));
}

GpuAllocator::~GpuAllocator()
{
  if (m_allocator != VK_NULL_HANDLE) {
    vmaDestroyAllocator(m_allocator);
    m_allocator = VK_NULL_HANDLE;
  }
}

VulkanBuffer
GpuAllocator::createBuffer(VkDeviceSize size,
                           VkBufferUsageFlags usage,
                           BufferType bufferType) const
{
  VulkanBuffer result{};
  result.size = size;

  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocCreateInfo{};
  allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

  switch (bufferType) {
    case BufferType::STAGING_BUFFER:
      allocCreateInfo.flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT;
      break;
    case BufferType::GPU_BUFFER:
      allocCreateInfo.flags = 0;
      break;
  }

  VmaAllocationInfo allocInfo{};
  VK_CHECK(vmaCreateBuffer(m_allocator,
                           &bufferInfo,
                           &allocCreateInfo,
                           &result.buffer,
                           &result.allocation,
                           &allocInfo));

  vmaSetAllocationName(m_allocator, result.allocation, "bufferAllocation");
  result.mapped = allocInfo.pMappedData;
  return result;
}

void
GpuAllocator::destroyBuffer(VulkanBuffer& buffer) const noexcept
{
  if (buffer.buffer != VK_NULL_HANDLE) {
    vmaDestroyBuffer(m_allocator, buffer.buffer, buffer.allocation);
    buffer.buffer = VK_NULL_HANDLE;
    buffer.allocation = VK_NULL_HANDLE;
    buffer.mapped = nullptr;
    buffer.size = 0;
  }
}

void
GpuAllocator::destroyBuffer(VkBuffer buffer,
                            VmaAllocation allocation) const noexcept
{
  if (buffer != VK_NULL_HANDLE) {
    vmaDestroyBuffer(m_allocator, buffer, allocation);
  }
}

VkImage
GpuAllocator::createImage(uint32_t width,
                          uint32_t height,
                          VkFormat format,
                          uint32_t layerCount,
                          VkImageTiling tiling,
                          VkImageUsageFlags usage,
                          VmaAllocation& allocation,
                          VkImageCreateFlags imageCreateFlags,
                          uint32_t mipLevels) const
{
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.flags = imageCreateFlags;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = width;
  imageInfo.extent.height = height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = mipLevels;
  imageInfo.arrayLayers = layerCount;
  imageInfo.format = format;
  imageInfo.tiling = tiling;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = usage;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocCreateInfo{};
  allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
  allocCreateInfo.flags = 0;

  VkImage image = VK_NULL_HANDLE;
  VK_CHECK(vmaCreateImage(
    m_allocator, &imageInfo, &allocCreateInfo, &image, &allocation, nullptr));

  vmaSetAllocationName(m_allocator, allocation, "imageAllocation");
  return image;
}

void
GpuAllocator::destroyImage(VkImage image,
                           VmaAllocation allocation) const noexcept
{
  if (image != VK_NULL_HANDLE) {
    vmaDestroyImage(m_allocator, image, allocation);
  }
}
