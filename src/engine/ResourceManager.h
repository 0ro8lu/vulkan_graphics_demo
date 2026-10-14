#pragma once

class GpuDevice;
class GpuAllocator;
class ImmediateCommands;

class ResourceManager final
{
public:
  explicit ResourceManager(const GpuDevice& device,
                           const GpuAllocator& allocator,
                           const ImmediateCommands& immediateCommands) noexcept;

private:
  [[maybe_unused]] const GpuDevice& m_device;
  [[maybe_unused]] const GpuAllocator& m_allocator;
  [[maybe_unused]] const ImmediateCommands& m_immediateCommands;
};
