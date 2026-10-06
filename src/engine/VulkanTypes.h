#pragma once

#include <cstdint>
#include <optional>
#include <vulkan/vulkan_core.h>

VK_DEFINE_HANDLE(VmaAllocation);
VK_DEFINE_HANDLE(VmaAllocator);
using VmaAllocationCreateFlags = VkFlags;

enum BufferType
{
  STAGING_BUFFER,
  GPU_BUFFER,
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
