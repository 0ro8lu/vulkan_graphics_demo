#pragma once

#include "engine/VulkanTypes.h"

class GpuDevice;

class ImmediateCommands final
{
public:
  explicit ImmediateCommands(const GpuDevice& device);
  ~ImmediateCommands();

  ImmediateCommands(const ImmediateCommands&) = delete;
  ImmediateCommands& operator=(const ImmediateCommands&) = delete;
  ImmediateCommands(ImmediateCommands&&) = delete;
  ImmediateCommands& operator=(ImmediateCommands&&) = delete;

  [[nodiscard]] VkCommandBuffer beginSingleTimeCommands() const;
  void endSingleTimeCommands(VkCommandBuffer commandBuffer) const;

  template<typename F>
    requires std::invocable<F, VkCommandBuffer>
  void execute(F&& func) const
  {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    func(commandBuffer);
    endSingleTimeCommands(commandBuffer);
  }

  void copyBuffer(VkBuffer srcBuffer,
                  VkBuffer dstBuffer,
                  VkDeviceSize size) const;

private:
  const GpuDevice& m_device;
  VkCommandPool m_commandPool{ VK_NULL_HANDLE };
};
