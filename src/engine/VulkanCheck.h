#pragma once

#include "engine/Error.h"
#include <source_location>
#include <vulkan/vulkan_core.h>

// Cold path: completely out-of-line, never inlined into call sites
[[noreturn]] void ReportVulkanFatalError(
  VkResult result,
  std::source_location loc = std::source_location::current());

inline void
CheckVulkanResult(VkResult result,
                  std::source_location loc = std::source_location::current())
{
  if (result != VK_SUCCESS) [[unlikely]] {
    ReportVulkanFatalError(result, loc);
  }
}

#define VK_CHECK(expr) CheckVulkanResult(expr)
