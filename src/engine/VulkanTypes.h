#pragma once

#include <cstdint>
#include <optional>
#include <vulkan/vulkan_core.h>

VK_DEFINE_HANDLE(VmaAllocation);
VK_DEFINE_HANDLE(VmaAllocator);
using VmaAllocationCreateFlags = VkFlags;

inline constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

enum BufferType
{
  STAGING_BUFFER,
  GPU_BUFFER,
};

struct VulkanBuffer
{
  VkBuffer buffer{ VK_NULL_HANDLE };
  VmaAllocation allocation{ VK_NULL_HANDLE };
  void* mapped{ nullptr };
  VkDeviceSize size{ 0 };

  [[nodiscard]] constexpr bool isValid() const noexcept
  {
    return buffer != VK_NULL_HANDLE;
  }

  [[nodiscard]] constexpr operator VkBuffer() const noexcept
  {
    return buffer;
  }
};

enum class SamplerType : uint8_t
{
  LinearRepeat = 0,
  LinearClampToEdge,
  LinearClampToBorder,
  NearestClampToEdge,
  Count
};

struct QueueFamilyIndices
{
  std::optional<uint32_t> graphicsFamily;
  std::optional<uint32_t> presentFamily;

  [[nodiscard]] bool isComplete() const noexcept
  {
    return graphicsFamily.has_value() && presentFamily.has_value();
  }
};
