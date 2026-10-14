#pragma once

#include <source_location>
#include <string_view>

#if defined(__clang__) || defined(__GNUC__)
  #define ENGINE_DEBUG_BREAK() __builtin_debugtrap()
#elif defined(_MSC_VER)
  #define ENGINE_DEBUG_BREAK() __debugbreak()
#else
  #define ENGINE_DEBUG_BREAK() ((void)0)
#endif

// Cold path: completely out-of-line, never inlined into call sites
[[noreturn]] void ReportFatalError(
  std::string_view message,
  std::source_location loc = std::source_location::current());
