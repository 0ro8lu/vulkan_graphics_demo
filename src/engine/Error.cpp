#include "engine/Error.h"

#include <cstdlib>
#include <iostream>

[[noreturn]] void
ReportFatalError(std::string_view message, std::source_location loc)
{
  std::cerr << "\n========================================\n"
            << "[FATAL ERROR]: " << message << "\n"
            << "Location: " << loc.file_name() << ":" << loc.line() << "\n"
            << "Function: " << loc.function_name() << "\n"
            << "========================================\n"
            << std::flush;

  ENGINE_DEBUG_BREAK();
  std::abort();
}
