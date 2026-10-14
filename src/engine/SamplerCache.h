#pragma once

#include "engine/VulkanTypes.h"

#include <array>

class GpuDevice;

class SamplerCache final
{
public:
  explicit SamplerCache(const GpuDevice& device);
  ~SamplerCache();

  SamplerCache(const SamplerCache&) = delete;
  SamplerCache& operator=(const SamplerCache&) = delete;
  SamplerCache(SamplerCache&&) = delete;
  SamplerCache& operator=(SamplerCache&&) = delete;

  [[nodiscard]] VkSampler get(SamplerType type) const noexcept
  {
    return m_samplers[static_cast<size_t>(type)];
  }

private:
  VkDevice m_device{ VK_NULL_HANDLE };
  std::array<VkSampler, static_cast<size_t>(SamplerType::Count)> m_samplers{
    VK_NULL_HANDLE
  };

  void createSamplers(float maxAnisotropy);
};
