#pragma once

#include <memory>
#include <span>
#include <string>

#include "engine/VulkanTypes.h"
#include <glm/glm.hpp>

class Model;
class VulkanContext;

class Skybox
{
public:
  Skybox(VulkanContext* vkContext, std::span<const std::string, 6> filePaths);
  ~Skybox();

  // Non-copyable and non-movable to safely manage Vulkan resources
  Skybox(const Skybox&) = delete;
  Skybox& operator=(const Skybox&) = delete;
  Skybox(Skybox&&) = delete;
  Skybox& operator=(Skybox&&) = delete;

  static VkDescriptorSetLayout s_skyboxLayout;

  VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

  std::unique_ptr<Model> m_cube;

private:
  VulkanContext* m_vkContext;

  VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;

  VkImage m_image = VK_NULL_HANDLE;
  VkImageView m_view = VK_NULL_HANDLE;
  VkSampler m_sampler = VK_NULL_HANDLE;

  VmaAllocation m_imageAllocation = VK_NULL_HANDLE;

  void transitionImageLayout(VkImage image,
                             std::span<const size_t> offsets,
                             VkImageLayout oldLayout,
                             VkImageLayout newLayout);

  void copyBufferToImage(VkBuffer buffer,
                         VkImage image,
                         std::span<const size_t> offsets,
                         uint32_t width,
                         uint32_t height);

  void setupDescriptors();
};
