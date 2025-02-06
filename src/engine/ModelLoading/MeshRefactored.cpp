#include "engine/ModelLoading/MeshRefactored.h"

#include <array>

MeshDO::MeshDO(VulkanContext* vkContext,
           size_t indexCount,
           size_t startIndex,
           TextureDO&& diffuseTexture,
           TextureDO&& specularTexture)
  : vkContext(vkContext)
  , indexCount(indexCount)
  , startIndex(startIndex)
  , diffuseTexture(std::move(diffuseTexture))
  , specularTexture(std::move(specularTexture))
{
}

MeshDO::~MeshDO() {}

void
MeshDO::createDescriptorSet(VkDescriptorPool descriptorPool,
                          VkDescriptorSetLayout descriptorLayout)
{
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = descriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &descriptorLayout;

  if (vkAllocateDescriptorSets(vkContext->logicalDevice,
                               &allocInfo,
                               &descriptorSet) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate descriptor sets!");
  }

  VkDescriptorImageInfo diffuseImageInfo{};
  diffuseImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  diffuseImageInfo.imageView = diffuseTexture.view;
  diffuseImageInfo.sampler = diffuseTexture.sampler;

  VkDescriptorImageInfo specularImageInfo{};
  specularImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  specularImageInfo.imageView = specularTexture.view;
  specularImageInfo.sampler = specularTexture.sampler;

  std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

  descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrites[0].dstSet = descriptorSet;
  descriptorWrites[0].dstBinding = 0;
  descriptorWrites[0].dstArrayElement = 0;
  descriptorWrites[0].descriptorType =
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptorWrites[0].descriptorCount = 1;
  descriptorWrites[0].pImageInfo = &diffuseImageInfo;

  descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrites[1].dstSet = descriptorSet;
  descriptorWrites[1].dstBinding = 1;
  descriptorWrites[1].dstArrayElement = 0;
  descriptorWrites[1].descriptorType =
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptorWrites[1].descriptorCount = 1;
  descriptorWrites[1].pImageInfo = &specularImageInfo;

  vkUpdateDescriptorSets(vkContext->logicalDevice,
                         static_cast<uint32_t>(descriptorWrites.size()),
                         descriptorWrites.data(),
                         0,
                         nullptr);
}
