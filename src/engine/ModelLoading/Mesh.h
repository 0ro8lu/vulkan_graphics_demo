#ifndef _MESH_H_
#define _MESH_H_

#include "engine/ModelLoading/Texture.h"
#include "engine/VulkanContext.h"

struct Vertex;

class Mesh
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
  Texture diffuseTexture;
  Texture specularTexture;

  void createDescriptorSet(VkDescriptorPool descriptorPool,
                           VkDescriptorSetLayout descriptorLayout);

  VkDescriptorSet descriptorSet;
  size_t indexCount;
  size_t startIndex;
private:

  VulkanContext* vkContext;
};

#endif
