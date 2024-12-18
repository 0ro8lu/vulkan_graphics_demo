#include "VulkanDevice.h"
#include <stdexcept>

VulkanDevice::VulkanDevice(VkPhysicalDevice physicalDevice,
                           VkDevice logicalDevice,
                           VmaAllocator allocator,
                           VkCommandPool commandPool,
                           VkQueue queue)
  : physicalDevice(physicalDevice)
  , logicalDevice(logicalDevice)
  , allocator(allocator)
  , commandPool(commandPool)
  , queue(queue)
{
}

VulkanDevice::~VulkanDevice() {}

void*
VulkanDevice::createBuffer(VkDeviceSize size,
                           VkBufferUsageFlags usage,
                           BufferType bufferType,
                           VkBuffer& buffer,
                           VmaAllocation& allocation)
{
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocCreateInfo = {};
  allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

  switch (bufferType) {
    case BufferType::STAGING_BUFFER:
      allocCreateInfo.flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT;
      break;
    case BufferType::GPU_BUFFER:
      allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
      allocCreateInfo.priority = 1.0f;
      break;
  }

  VmaAllocationInfo allocInfo;
  if (vmaCreateBuffer(allocator,
                      &bufferInfo,
                      &allocCreateInfo,
                      &buffer,
                      &allocation,
                      &allocInfo) != VK_SUCCESS) {
    throw std::runtime_error("failed to create buffer!");
  }

  return allocInfo.pMappedData;
}

void
VulkanDevice::copyBuffer(VkBuffer srcBuffer,
                         VkBuffer dstBuffer,
                         VkDeviceSize size)
{
  VkCommandBuffer commandBuffer = beginSingleTimeCommands();

  VkBufferCopy copyRegion{};
  copyRegion.size = size;
  vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

  endSingleTimeCommands(commandBuffer);
}

VkCommandBuffer
VulkanDevice::beginSingleTimeCommands()
{
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = commandPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(logicalDevice, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);
  return commandBuffer;
}

void
VulkanDevice::endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(queue);

  vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
}
