#include "engine/Skybox.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "engine/VulkanCheck.h"
#include "engine/VulkanContext.h"

#include <stb_image.h>

VkDescriptorSetLayout Skybox::skyboxLayout = VK_NULL_HANDLE;

Skybox::Skybox(VulkanContext* vkContext, std::array<std::string, 6> filePaths)
  : vkContext(vkContext)
{
  std::string modelPath = MODEL_PATH;
  cube = std::make_unique<Model>(modelPath + "cube.glb", vkContext);

  int texWidth, texHeight, texChannels;

  if (image != VK_NULL_HANDLE) {
    throw std::runtime_error("Skybox already present!");
  }

  std::vector<stbi_uc*> pixels;
  pixels.resize(filePaths.size());
  for (size_t i = 0; i < filePaths.size(); i++) {
    pixels[i] = stbi_load(filePaths[i].c_str(),
                          &texWidth,
                          &texHeight,
                          &texChannels,
                          STBI_rgb_alpha);
    if (!pixels[i]) {
      throw std::runtime_error("Failed to load texture image from memory!");
    }
  }

  VkDeviceSize imageSize = static_cast<uint64_t>(texWidth) *
                           static_cast<uint64_t>(texHeight) * 4 *
                           sizeof(stbi_uc);

  VkBuffer stagingBuffer;
  VmaAllocation stagingBufferAllocation;

  char* data =
    static_cast<char*>(vkContext->createBuffer(imageSize * filePaths.size(),
                                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                               BufferType::STAGING_BUFFER,
                                               stagingBuffer,
                                               stagingBufferAllocation));

  size_t byteOffset = 0;
  std::vector<size_t> byteOffsets;
  byteOffsets.resize(filePaths.size());

  // Put all the image data into a single buffer
  for (size_t i = 0; i < pixels.size(); i++) {
    data += byteOffset;
    memcpy(data, pixels[i], imageSize * sizeof(stbi_uc));
    byteOffset = imageSize;
  }

  // populate the offsets vector
  for (size_t i = 0; i < byteOffsets.size(); i++) {
    if (i == 0) {
      byteOffsets[i] = 0;
    } else {
      byteOffsets[i] += imageSize + byteOffsets[i - 1];
    }
  }

  for (auto& currentPixeData : pixels) {
    stbi_image_free(currentPixeData);
  }

  // -------------------- CREATE IMAGE --------------------
  image = vkContext->createImage(static_cast<uint32_t>(texWidth),
                                 static_cast<uint32_t>(texHeight),
                                 VK_FORMAT_R8G8B8A8_SRGB,
                                 6,
                                 VK_IMAGE_TILING_OPTIMAL,
                                 VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                   VK_IMAGE_USAGE_SAMPLED_BIT,
                                 imageAllocation,
                                 VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

  // Copy data from staging buffer to image
  transitionImageLayout(image,
                        byteOffsets,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  copyBufferToImage(stagingBuffer,
                    image,
                    byteOffsets,
                    static_cast<uint32_t>(texWidth),
                    static_cast<uint32_t>(texHeight));
  transitionImageLayout(image,
                        byteOffsets,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  // -------------------- CREATE IMAGE VIEW --------------------
  view = vkContext->createImageView(image,
                                    VK_FORMAT_R8G8B8A8_SRGB,
                                    6,
                                    VK_IMAGE_ASPECT_COLOR_BIT,
                                    VK_IMAGE_VIEW_TYPE_CUBE);

  // -------------------- CREATE SAMPLER --------------------
  sampler = vkContext->getSampler(SamplerType::LinearClampToEdge);

  vkContext->destroyBuffer(stagingBuffer, stagingBufferAllocation);

  setupDescriptors();
}

Skybox::~Skybox()
{
  vkContext->destroyImageView(view);
  vkContext->destroyImage(image, imageAllocation);

  vkDestroyDescriptorSetLayout(vkContext->logicalDevice, skyboxLayout, nullptr);

  vkDestroyDescriptorPool(vkContext->logicalDevice, descriptorPool, nullptr);
}

// TODO: probably move this inside vkContext class
void
Skybox::transitionImageLayout(VkImage image,
                              const std::vector<size_t>& offsets,
                              VkImageLayout oldLayout,
                              VkImageLayout newLayout)
{
  VkCommandBuffer commandBuffer = vkContext->beginSingleTimeCommands();

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
  barrier.subresourceRange.layerCount = offsets.size();

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
    throw std::invalid_argument("unsupported layout transition!");
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

  vkContext->endSingleTimeCommands(commandBuffer);
}

// TODO: probably move this inside vkContext class
void
Skybox::copyBufferToImage(VkBuffer buffer,
                          VkImage image,
                          const std::vector<size_t>& offsets,
                          uint32_t width,
                          uint32_t height)
{
  VkCommandBuffer commandBuffer = vkContext->beginSingleTimeCommands();

  std::vector<VkBufferImageCopy> bufferCopyRegions;
  for (size_t i = 0; i < offsets.size(); i++) {
    VkBufferImageCopy bufferCopyRegion = {};
    bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferCopyRegion.imageSubresource.mipLevel = 0;
    bufferCopyRegion.imageSubresource.baseArrayLayer = i;
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

  vkContext->endSingleTimeCommands(commandBuffer);
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

  if (vkCreateDescriptorPool(
        vkContext->logicalDevice, &poolInfo, nullptr, &descriptorPool) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor pool!");
  }

  // -------------------- DESCRIPTOR LAYOUT --------------------
  if (Skybox::skyboxLayout == VK_NULL_HANDLE) {
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

    if (vkCreateDescriptorSetLayout(vkContext->logicalDevice,
                                    &layoutInfo,
                                    nullptr,
                                    &Skybox::skyboxLayout) != VK_SUCCESS) {
      throw std::runtime_error("failed to create descriptor set layout!");
    }
  }

  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = descriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &Skybox::skyboxLayout;

  if (vkAllocateDescriptorSets(
        vkContext->logicalDevice, &allocInfo, &descriptorSet) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate descriptor sets!");
  }

  // -------------------- DESCRIPTOR SET --------------------
  VkDescriptorImageInfo cubemapImageInfo{};
  cubemapImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  cubemapImageInfo.imageView = view;
  cubemapImageInfo.sampler = sampler;

  std::array<VkWriteDescriptorSet, 1> descriptorWrites{};

  descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrites[0].dstSet = descriptorSet;
  descriptorWrites[0].dstBinding = 0;
  descriptorWrites[0].dstArrayElement = 0;
  descriptorWrites[0].descriptorType =
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptorWrites[0].descriptorCount = 1;
  descriptorWrites[0].pImageInfo = &cubemapImageInfo;

  vkUpdateDescriptorSets(vkContext->logicalDevice,
                         static_cast<uint32_t>(descriptorWrites.size()),
                         descriptorWrites.data(),
                         0,
                         nullptr);
}
