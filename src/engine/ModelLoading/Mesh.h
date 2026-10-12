#pragma once

#include "engine/ModelLoading/Texture.h"

class VulkanContext;

class Mesh final
{
public:
  Mesh(VulkanContext* vkContext,
       size_t indexCount,
       size_t startIndex,
       Texture&& diffuseTexture,
       Texture&& specularTexture);

  ~Mesh();

  Mesh(const Mesh&) = delete;
  Mesh& operator=(const Mesh&) = delete;

  Mesh(Mesh&&) noexcept = default;
  Mesh& operator=(Mesh&&) noexcept = default;

  // probably embed this into a Material object and add normal mapping and other
  // fancy textures.
  Texture m_diffuseTexture;
  Texture m_specularTexture;

  void createDescriptorSet(VkDescriptorPool descriptorPool,
                           VkDescriptorSetLayout descriptorLayout);

  VkDescriptorSet m_descriptorSet;
  size_t m_indexCount;
  size_t m_startIndex;

private:
  VulkanContext* m_vkContext;
};
