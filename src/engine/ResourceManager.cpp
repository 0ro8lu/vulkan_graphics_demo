#include "engine/ResourceManager.h"

ResourceManager::ResourceManager(
  const GpuDevice& device,
  const GpuAllocator& allocator,
  const ImmediateCommands& immediateCommands) noexcept
  : m_device(device)
  , m_allocator(allocator)
  , m_immediateCommands(immediateCommands)
{
}
