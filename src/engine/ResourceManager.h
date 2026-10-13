#pragma once

#include "engine/VulkanContext.h"

class ResourceManager final
{
public:
  ResourceManager(VulkanContext* vulkanContext) noexcept;

private:
  [[maybe_unused]] VulkanContext* m_vkContext;
};
