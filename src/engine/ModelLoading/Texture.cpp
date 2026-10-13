#include "engine/ModelLoading/Texture.h"
#include "engine/VulkanCheck.h"
#include <iostream>
#include <stb_image.h>

Texture::Texture()
  : m_vkContext(nullptr)
{
  m_image = VK_NULL_HANDLE;
  m_view = VK_NULL_HANDLE;
  m_sampler = VK_NULL_HANDLE;
}

Texture::Texture(VulkanContext* vkContext, unsigned char* data, size_t size)
{
  this->m_vkContext = vkContext;

  int texWidth, texHeight, texChannels;

  stbi_uc* pixels = stbi_load_from_memory(
    data, size, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
  if (!pixels) {
    std::cerr << "[Texture]: Failed to load image from memory: "
              << stbi_failure_reason() << '\n';
    ENGINE_DEBUG_BREAK();
    std::abort();
  }

  createTextureImageFromPixels(pixels, texWidth, texHeight);

  stbi_image_free(pixels);

  // Create the image view and sampler.
  m_view = vkContext->createImageView(
    m_image, VK_FORMAT_R8G8B8A8_SRGB, 1, VK_IMAGE_ASPECT_COLOR_BIT);
  m_sampler = vkContext->getSampler(SamplerType::LinearRepeat);
}

Texture::Texture(VulkanContext* vkContext, std::string filePath)
{
  this->m_vkContext = vkContext;

  int texWidth, texHeight, texChannels;

  stbi_uc* pixels = stbi_load(
    filePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
  if (!pixels) {
    std::cerr << "[Texture]: Failed to load image file '" << filePath
              << "': " << stbi_failure_reason() << '\n';
    ENGINE_DEBUG_BREAK();
    std::abort();
  }

  createTextureImageFromPixels(pixels, texWidth, texHeight);

  stbi_image_free(pixels);

  m_view = vkContext->createImageView(
    m_image, VK_FORMAT_R8G8B8A8_SRGB, 1, VK_IMAGE_ASPECT_COLOR_BIT);

  m_sampler = vkContext->getSampler(SamplerType::LinearRepeat);
}

Texture::~Texture()
{
  cleanup();
}

void
Texture::createTextureImageFromPixels(unsigned char* pixels,
                                      int texWidth,
                                      int texHeight)
{
  VkDeviceSize imageSize = texWidth * texHeight * 4;

  // -------------------- STAGING BUFFER --------------------
  VulkanBuffer stagingBuffer = m_vkContext->createBuffer(
    imageSize,
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    BufferType::STAGING_BUFFER);

  memcpy(stagingBuffer.mapped, pixels, static_cast<size_t>(imageSize));

  // Create the Vulkan image
  m_image = m_vkContext->createImage(texWidth,
                                     texHeight,
                                     VK_FORMAT_R8G8B8A8_SRGB,
                                     1,
                                     VK_IMAGE_TILING_OPTIMAL,
                                     VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                       VK_IMAGE_USAGE_SAMPLED_BIT,
                                     m_allocation);

  // Copy data from staging buffer to image
  transitionImageLayout(
    m_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  copyBufferToImage(stagingBuffer,
                    m_image,
                    static_cast<uint32_t>(texWidth),
                    static_cast<uint32_t>(texHeight));
  transitionImageLayout(m_image,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  m_vkContext->destroyBuffer(stagingBuffer);
}

void
Texture::transitionImageLayout(VkImage image,
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
  barrier.subresourceRange.layerCount = 1;

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
    std::cerr << "[Texture]: Unsupported image layout transition from "
              << oldLayout << " to " << newLayout << '\n';
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

void
Texture::copyBufferToImage(VkBuffer buffer,
                           VkImage image,
                           uint32_t width,
                           uint32_t height)
{
  VkCommandBuffer commandBuffer = m_vkContext->beginSingleTimeCommands();

  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = { 0, 0, 0 };
  region.imageExtent = { width, height, 1 };

  vkCmdCopyBufferToImage(commandBuffer,
                         buffer,
                         image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         1,
                         &region);

  m_vkContext->endSingleTimeCommands(commandBuffer);
}

void
Texture::cleanup()
{
  if (m_view != VK_NULL_HANDLE && m_sampler != VK_NULL_HANDLE &&
      m_image != VK_NULL_HANDLE) {
    m_vkContext->destroyImageView(m_view);
    m_vkContext->destroyImage(m_image, m_allocation);

    m_view = VK_NULL_HANDLE;
    m_sampler = VK_NULL_HANDLE;
    m_image = VK_NULL_HANDLE;
    m_allocation = VK_NULL_HANDLE;
    m_vkContext = nullptr;
  }
}
