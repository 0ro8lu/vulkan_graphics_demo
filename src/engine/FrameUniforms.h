#pragma once

#include <array>

#include "engine/VulkanTypes.h"

struct SceneRenderData;
class VulkanContext;

class FrameUniforms
{
public:
  explicit FrameUniforms(VulkanContext* vkContext);
  ~FrameUniforms();

  FrameUniforms(const FrameUniforms&) = delete;
  FrameUniforms& operator=(const FrameUniforms&) = delete;
  FrameUniforms(FrameUniforms&&) = delete;
  FrameUniforms& operator=(FrameUniforms&&) = delete;

  void update(uint32_t frameIndex, const SceneRenderData& sceneRenderData);

  [[nodiscard]] VkDescriptorSetLayout getCameraLayout() const noexcept
  {
    return m_cameraLayout;
  }

  [[nodiscard]] VkDescriptorSetLayout getLightsLayout() const noexcept
  {
    return m_lightsLayout;
  }

  [[nodiscard]] VkDescriptorSetLayout getShadowMapLayout() const noexcept
  {
    return m_shadowMapLayout;
  }

  [[nodiscard]] VkDescriptorSet getCameraDescriptorSet(
    uint32_t frameIndex) const noexcept;

  [[nodiscard]] VkDescriptorSet getLightsDescriptorSet(
    uint32_t frameIndex) const noexcept;

  [[nodiscard]] VkDescriptorSet getShadowMapDescriptorSet() const noexcept
  {
    return m_shadowMapDescriptorSet;
  }

private:
  struct FrameResources
  {
    VulkanBuffer cameraBuffer;
    VulkanBuffer directionalLightBuffer;
    VulkanBuffer pointLightsBuffer;
    VulkanBuffer spotLightsBuffer;

    VkDescriptorSet cameraDescriptorSet{ VK_NULL_HANDLE };
    VkDescriptorSet lightsDescriptorSet{ VK_NULL_HANDLE };
  };

  void createDescriptorPool();
  void createLayouts();
  void createFrameResources();
  void createShadowMapDescriptorSet();

  [[nodiscard]] VulkanBuffer createMappedBuffer(VkDeviceSize size,
                                                VkBufferUsageFlags usage);
  void destroyBuffer(VulkanBuffer& buffer) noexcept;

  VulkanContext* m_vkContext{ nullptr };

  VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };
  VkDescriptorSetLayout m_cameraLayout{ VK_NULL_HANDLE };
  VkDescriptorSetLayout m_lightsLayout{ VK_NULL_HANDLE };
  VkDescriptorSetLayout m_shadowMapLayout{ VK_NULL_HANDLE };

  std::array<FrameResources, MAX_FRAMES_IN_FLIGHT> m_frames{};
  VkDescriptorSet m_shadowMapDescriptorSet{ VK_NULL_HANDLE };
};
