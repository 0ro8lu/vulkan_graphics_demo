#pragma once

#include <vulkan/vulkan.h>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ShaderLoader {

[[nodiscard]] std::string resolvePath(std::string_view shaderName);

[[nodiscard]] std::vector<char> readSPIRV(const std::string& filepath);

[[nodiscard]] VkShaderModule createShaderModule(VkDevice device,
                                                std::span<const char> code);

[[nodiscard]] VkShaderModule loadShaderModule(VkDevice device,
                                              std::string_view shaderName);

} // namespace ShaderLoader
