#include "ImmediateCommands.h"

#include "GpuDevice.h"
#include "engine/VulkanCheck.h"

ImmediateCommands::ImmediateCommands(const GpuDevice& device)
  : m_device(device)
{
  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = m_device.queueFamilies().graphicsFamily.value();

  VK_CHECK(vkCreateCommandPool(
    m_device.device(), &poolInfo, nullptr, &m_commandPool));
}

ImmediateCommands::~ImmediateCommands()
{
  if (m_commandPool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(m_device.device(), m_commandPool, nullptr);
    m_commandPool = VK_NULL_HANDLE;
  }
}

VkCommandBuffer
ImmediateCommands::beginSingleTimeCommands() const
{
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = m_commandPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
  VK_CHECK(
    vkAllocateCommandBuffers(m_device.device(), &allocInfo, &commandBuffer));

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
  return commandBuffer;
}

void
ImmediateCommands::endSingleTimeCommands(VkCommandBuffer commandBuffer) const
{
  VK_CHECK(vkEndCommandBuffer(commandBuffer));

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  VK_CHECK(
    vkQueueSubmit(m_device.graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE));
  VK_CHECK(vkQueueWaitIdle(m_device.graphicsQueue()));

  vkFreeCommandBuffers(m_device.device(), m_commandPool, 1, &commandBuffer);
}

void
ImmediateCommands::copyBuffer(VkBuffer srcBuffer,
                              VkBuffer dstBuffer,
                              VkDeviceSize size) const
{
  execute([&](VkCommandBuffer commandBuffer) {
    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
  });
}
