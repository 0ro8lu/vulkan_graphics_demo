#include "engine/ShaderLoader.h"
#include "engine/VulkanCheck.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace ShaderLoader {

std::string
resolvePath(std::string_view shaderName)
{
  if (std::filesystem::path(shaderName).is_absolute()) {
    return std::string(shaderName);
  }
#ifdef SHADER_PATH
  return std::string(SHADER_PATH) + std::string(shaderName);
#else
  return std::string(shaderName);
#endif
}

std::vector<char>
readSPIRV(const std::string& filepath)
{
  std::ifstream file(filepath, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    std::cerr << "\n========================================\n"
              << "[FATAL SHADER ERROR]: Failed to open shader file!\n"
              << "  File: " << filepath << "\n"
              << "========================================\n"
              << std::flush;
    ENGINE_DEBUG_BREAK();
    std::abort();
  }

  const auto fileSize = static_cast<size_t>(file.tellg());
  std::vector<char> buffer(fileSize);

  file.seekg(0);
  file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
  file.close();

  return buffer;
}

VkShaderModule
createShaderModule(VkDevice device, std::span<const char> code)
{
  VkShaderModuleCreateInfo createInfo{
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = code.size(),
    .pCode = reinterpret_cast<const uint32_t*>(code.data()),
  };

  VkShaderModule shaderModule = VK_NULL_HANDLE;
  VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));
  return shaderModule;
}

VkShaderModule
loadShaderModule(VkDevice device, std::string_view shaderName)
{
  const std::string resolved = resolvePath(shaderName);
  const std::vector<char> code = readSPIRV(resolved);
  return createShaderModule(device, code);
}

} // namespace ShaderLoader
