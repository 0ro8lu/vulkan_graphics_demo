#pragma once

#include <source_location>
#include <vulkan/vulkan_core.h>

#if defined(__clang__) || defined(__GNUC__)
  #define ENGINE_DEBUG_BREAK() __builtin_debugtrap()
#elif defined(_MSC_VER)
  #define ENGINE_DEBUG_BREAK() __debugbreak()
#else
  #define ENGINE_DEBUG_BREAK() ((void)0)
#endif

// Cold path: completely out-of-line, never inlined into call sites
[[noreturn]] void ReportVulkanFatalError(VkResult result,
                                        std::source_location loc);

inline void
CheckVulkanResult(VkResult result,
                  std::source_location loc = std::source_location::current())
{
  if (result != VK_SUCCESS) [[unlikely]] {
    ReportVulkanFatalError(result, loc);
  }
}

#define VK_CHECK(expr) CheckVulkanResult(expr)
