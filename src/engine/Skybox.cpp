#include "engine/Skybox.h"
#include "engine/ModelLoading/Model.h"
#include "engine/VulkanCheck.h"
#include "engine/VulkanContext.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>

#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

VkDescriptorSetLayout Skybox::s_skyboxLayout = VK_NULL_HANDLE;

Skybox::Skybox(VulkanContext* vkContext, std::span<const std::string, 6> filePaths)
  : m_vkContext(vkContext)
{
  std::string modelPath = MODEL_PATH;
  m_cube = std::make_unique<Model>(modelPath + "cube.glb", vkContext);

  int texWidth, texHeight, texChannels;

  assert(m_image == VK_NULL_HANDLE && "Skybox image already created!");

  std::array<stbi_uc*, 6> pixels{};
  for (size_t i = 0; i < 6; i++) {
    pixels[i] = stbi_load(filePaths[i].c_str(),
                          &texWidth,
                          &texHeight,
                          &texChannels,
                          STBI_rgb_alpha);
    if (!pixels[i]) {
      for (size_t j = 0; j < i; ++j) {
        stbi_image_free(pixels[j]);
      }
      std::cerr << "\n========================================\n"
                << "[FATAL ERROR]: Failed to load skybox texture!\n"
                << "  Face: " << i << "\n"
                << "  File: " << filePaths[i] << "\n"
                << "========================================\n"
                << std::flush;
      ENGINE_DEBUG_BREAK();
      std::abort();
    }
  }

  const VkDeviceSize imageSize = static_cast<VkDeviceSize>(texWidth) * texHeight * 4;

  VkBuffer stagingBuffer;
  VmaAllocation stagingBufferAllocation;

  char* data =
    static_cast<char*>(m_vkContext->createBuffer(imageSize * 6,
                                                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                 BufferType::STAGING_BUFFER,
                                                 stagingBuffer,
                                                 stagingBufferAllocation));

  std::array<size_t, 6> byteOffsets{};
  for (size_t i = 0; i < 6; ++i) {
    byteOffsets[i] = i * imageSize;
    std::memcpy(data + byteOffsets[i], pixels[i], imageSize);
    stbi_image_free(pixels[i]);
  }

  // -------------------- CREATE IMAGE --------------------
  m_image = m_vkContext->createImage(static_cast<uint32_t>(texWidth),
                                     static_cast<uint32_t>(texHeight),
                                     VK_FORMAT_R8G8B8A8_SRGB,
                                     6,
                                     VK_IMAGE_TILING_OPTIMAL,
                                     VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                       VK_IMAGE_USAGE_SAMPLED_BIT,
                                     m_imageAllocation,
                                     VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

  // Copy data from staging buffer to image
  transitionImageLayout(m_image,
                        byteOffsets,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  copyBufferToImage(stagingBuffer,
                    m_image,
                    byteOffsets,
                    static_cast<uint32_t>(texWidth),
                    static_cast<uint32_t>(texHeight));
  transitionImageLayout(m_image,
                        byteOffsets,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  // -------------------- CREATE IMAGE VIEW --------------------
  m_view = m_vkContext->createImageView(m_image,
                                        VK_FORMAT_R8G8B8A8_SRGB,
                                        6,
                                        VK_IMAGE_ASPECT_COLOR_BIT,
                                        VK_IMAGE_VIEW_TYPE_CUBE);

  // -------------------- CREATE SAMPLER --------------------
  m_sampler = m_vkContext->getSampler(SamplerType::LinearClampToEdge);

  m_vkContext->destroyBuffer(stagingBuffer, stagingBufferAllocation);

  setupDescriptors();
}

Skybox::~Skybox()
{
  m_vkContext->destroyImageView(m_view);
  m_vkContext->destroyImage(m_image, m_imageAllocation);

  vkDestroyDescriptorSetLayout(m_vkContext->logicalDevice, s_skyboxLayout, nullptr);

  vkDestroyDescriptorPool(m_vkContext->logicalDevice, m_descriptorPool, nullptr);
}

// TODO: probably move this inside vkContext class
void
Skybox::transitionImageLayout(VkImage image,
                              std::span<const size_t> offsets,
                              VkImageLayout oldLayout,
                              VkImageLayout newLayout)
{
  VkCommandBuffer commandBuffer = m_vkContext->beginSingleTimeCommands();

  // -------------------- TRANSITION LAYOUT --------------------
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = static_cast<uint32_t>(offsets.size());

  VkPipelineStageFlags sourceStage;
  VkPipelineStageFlags destinationStage;

  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
      newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else {
    // End the command buffer before aborting to avoid leaking it
    m_vkContext->endSingleTimeCommands(commandBuffer);
    std::cerr << "\n========================================\n"
              << "[FATAL ERROR]: Unsupported layout transition!\n"
              << "  Old layout: " << oldLayout << "\n"
              << "  New layout: " << newLayout << "\n"
              << "========================================\n"
              << std::flush;
    ENGINE_DEBUG_BREAK();
    std::abort();
  }

  vkCmdPipelineBarrier(commandBuffer,
                       sourceStage,
                       destinationStage,
                       0,
                       0,
                       nullptr,
                       0,
                       nullptr,
                       1,
                       &barrier);

  m_vkContext->endSingleTimeCommands(commandBuffer);
}

// TODO: probably move this inside vkContext class
void
Skybox::copyBufferToImage(VkBuffer buffer,
                          VkImage image,
                          std::span<const size_t> offsets,
                          uint32_t width,
                          uint32_t height)
{
  VkCommandBuffer commandBuffer = m_vkContext->beginSingleTimeCommands();

  std::vector<VkBufferImageCopy> bufferCopyRegions;
  bufferCopyRegions.reserve(offsets.size());
  for (size_t i = 0; i < offsets.size(); i++) {
    VkBufferImageCopy bufferCopyRegion = {};
    bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferCopyRegion.imageSubresource.mipLevel = 0;
    bufferCopyRegion.imageSubresource.baseArrayLayer = static_cast<uint32_t>(i);
    bufferCopyRegion.imageSubresource.layerCount = 1;
    bufferCopyRegion.imageExtent.width = width;
    bufferCopyRegion.imageExtent.height = height;
    bufferCopyRegion.imageExtent.depth = 1;
    bufferCopyRegion.bufferOffset = offsets[i];
    bufferCopyRegions.push_back(bufferCopyRegion);
  }

  vkCmdCopyBufferToImage(commandBuffer,
                         buffer,
                         image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         static_cast<uint32_t>(bufferCopyRegions.size()),
                         bufferCopyRegions.data());

  m_vkContext->endSingleTimeCommands(commandBuffer);
}

void
Skybox::setupDescriptors()
{
  // -------------------- DESCRIPTOR POOL --------------------
  VkDescriptorPoolSize poolSize;

  poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSize.descriptorCount = 1;

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = 1;
  poolInfo.pPoolSizes = &poolSize;
  poolInfo.maxSets = 1;

  VK_CHECK(vkCreateDescriptorPool(
    m_vkContext->logicalDevice, &poolInfo, nullptr, &m_descriptorPool));

  // -------------------- DESCRIPTOR LAYOUT --------------------
  if (Skybox::s_skyboxLayout == VK_NULL_HANDLE) {
    VkDescriptorSetLayoutBinding cubemapLayoutBinding{};
    cubemapLayoutBinding.binding = 0;
    cubemapLayoutBinding.descriptorCount = 1;
    cubemapLayoutBinding.descriptorType =
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    cubemapLayoutBinding.pImmutableSamplers = nullptr;
    cubemapLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 1> bindings = {
      cubemapLayoutBinding,
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(m_vkContext->logicalDevice,
                                         &layoutInfo,
                                         nullptr,
                                         &Skybox::s_skyboxLayout));
  }

  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = m_descriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &Skybox::s_skyboxLayout;

  VK_CHECK(vkAllocateDescriptorSets(
    m_vkContext->logicalDevice, &allocInfo, &m_descriptorSet));

  // -------------------- DESCRIPTOR SET --------------------
  VkDescriptorImageInfo cubemapImageInfo{};
  cubemapImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  cubemapImageInfo.imageView = m_view;
  cubemapImageInfo.sampler = m_sampler;

  std::array<VkWriteDescriptorSet, 1> descriptorWrites{};

  descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrites[0].dstSet = m_descriptorSet;
  descriptorWrites[0].dstBinding = 0;
  descriptorWrites[0].dstArrayElement = 0;
  descriptorWrites[0].descriptorType =
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptorWrites[0].descriptorCount = 1;
  descriptorWrites[0].pImageInfo = &cubemapImageInfo;

  vkUpdateDescriptorSets(m_vkContext->logicalDevice,
                         static_cast<uint32_t>(descriptorWrites.size()),
                         descriptorWrites.data(),
                         0,
                         nullptr);
}
