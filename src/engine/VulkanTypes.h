#pragma once

#include <cstdint>
#include <vulkan/vulkan_core.h>

VK_DEFINE_HANDLE(VmaAllocation);

enum class SamplerType : uint8_t
{
  LinearRepeat = 0,
  LinearClampToEdge,
  LinearClampToBorder,
  NearestClampToEdge,
  Count
};
