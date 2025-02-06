#ifndef MESH_H_
#define MESH_H_

#include "engine/ModelLoading/TextreRefactored.h"
#include "engine/VulkanContext.h"

struct Vertex;

class MeshDO
{
public:
  MeshDO(VulkanContext* vkContext,
       size_t indexCount,
       size_t startIndex,
       TextureDO&& diffuseTexture,
       TextureDO&& specularTexture);

  ~MeshDO();

  MeshDO(const MeshDO&) = delete;
  MeshDO& operator=(const MeshDO&) = delete;

  MeshDO(MeshDO&&) noexcept = default;
  MeshDO& operator=(MeshDO&&) noexcept = default;

  // probably embed this into a Material object and add normal mapping and other
  // fancy textures.
  TextureDO diffuseTexture;
  TextureDO specularTexture;

  void createDescriptorSet(VkDescriptorPool descriptorPool,
                           VkDescriptorSetLayout descriptorLayout);

  VkDescriptorSet descriptorSet;
  size_t indexCount;
  size_t startIndex;
private:

  VulkanContext* vkContext;
};

#endif
