#include "Mesh.h"
#include "engine/Texture.h"
#include "engine/VulkanDevice.h"
#include <vulkan/vulkan_core.h>

#include <array>

Mesh::Mesh(VulkanDevice* vulkanDevice,
           size_t indexCount,
           size_t startIndex,
           Texture&& diffuseTexture,
           Texture&& specularTexture)
  : vulkanDevice(vulkanDevice)
  , indexCount(indexCount)
  , startIndex(startIndex)
  , diffuseTexture(std::move(diffuseTexture))
  , specularTexture(std::move(specularTexture))
{
}

Mesh::~Mesh() {}

void
Mesh::createDescriptorSet(VkDescriptorPool descriptorPool,
                          VkDescriptorSetLayout descriptorLayout)
{
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = descriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &descriptorLayout;

  if (vkAllocateDescriptorSets(vulkanDevice->logicalDevice,
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

  vkUpdateDescriptorSets(vulkanDevice->logicalDevice,
                         static_cast<uint32_t>(descriptorWrites.size()),
                         descriptorWrites.data(),
                         0,
                         nullptr);
}

void
Mesh::draw(VkCommandBuffer commandBuffer,
           VkPipelineLayout pipelineLayout,
           bool shouldRenderTexture)
{
  if (shouldRenderTexture) {
    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout,
                            1,
                            1,
                            &descriptorSet,
                            0,
                            nullptr);
  }

  vkCmdDrawIndexed(commandBuffer, indexCount, 1, startIndex, 0, 0);
}
