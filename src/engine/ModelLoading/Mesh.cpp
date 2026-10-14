#include "engine/ModelLoading/Mesh.h"
#include "engine/GpuDevice.h"
#include "engine/VulkanCheck.h"

#include <array>

Mesh::Mesh(const GpuDevice& device,
           size_t indexCount,
           size_t startIndex,
           Texture&& diffuseTexture,
           Texture&& specularTexture)
  : m_diffuseTexture(std::move(diffuseTexture))
  , m_specularTexture(std::move(specularTexture))
  , m_indexCount(indexCount)
  , m_startIndex(startIndex)
  , m_device(&device)
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

  VK_CHECK(vkAllocateDescriptorSets(
    m_device->device(), &allocInfo, &m_descriptorSet));

  VkDescriptorImageInfo diffuseImageInfo{};
  diffuseImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  diffuseImageInfo.imageView = m_diffuseTexture.m_view;
  diffuseImageInfo.sampler = m_diffuseTexture.m_sampler;

  VkDescriptorImageInfo specularImageInfo{};
  specularImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  specularImageInfo.imageView = m_specularTexture.m_view;
  specularImageInfo.sampler = m_specularTexture.m_sampler;

  std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

  descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrites[0].dstSet = m_descriptorSet;
  descriptorWrites[0].dstBinding = 0;
  descriptorWrites[0].dstArrayElement = 0;
  descriptorWrites[0].descriptorType =
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptorWrites[0].descriptorCount = 1;
  descriptorWrites[0].pImageInfo = &diffuseImageInfo;

  descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrites[1].dstSet = m_descriptorSet;
  descriptorWrites[1].dstBinding = 1;
  descriptorWrites[1].dstArrayElement = 0;
  descriptorWrites[1].descriptorType =
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptorWrites[1].descriptorCount = 1;
  descriptorWrites[1].pImageInfo = &specularImageInfo;

  vkUpdateDescriptorSets(m_device->device(),
                         static_cast<uint32_t>(descriptorWrites.size()),
                         descriptorWrites.data(),
                         0,
                         nullptr);
}
