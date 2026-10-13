#include "engine/ResourceManager.h"

ResourceManager::ResourceManager(VulkanContext* vulkanContext) noexcept
  : m_vkContext(vulkanContext)
{
}
