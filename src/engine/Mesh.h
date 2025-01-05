#ifndef MESH_H_
#define MESH_H_

#include "engine/Texture.h"
#include "engine/VulkanDevice.h"

#include <array>

struct Vertex;

class Mesh
{
public:
  Mesh(VulkanDevice* vulkanDevice,
       size_t indexCount,
       size_t startIndex,
       Texture&& diffuseTexture,
       Texture&& specularTexture);

  ~Mesh();

  Mesh(const Mesh&) = delete;
  Mesh& operator=(const Mesh&) = delete;

  Mesh(Mesh&&) noexcept = default;
  Mesh& operator=(Mesh&&) noexcept = default;

  // probably embed this into a Material object and add normal mapping and other fancy textures.
  Texture diffuseTexture;
  Texture specularTexture;
  
  void draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, bool shouldRenderTexture);
  void createDescriptorSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorLayout);

private:
  size_t indexCount;
  size_t startIndex;
  
  VulkanDevice* vulkanDevice;
  VkDescriptorSet descriptorSet;
};

#endif
