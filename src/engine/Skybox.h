#ifndef _SKYBOX_H_
#define _SKYBOX_H_

#include <array>
#include <memory>
#include <string>

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include "engine/ModelLoading/Model.h"
#include "engine/VulkanContext.h"

class Skybox
{
public:
  Skybox(VulkanContext* vkContext,
                   std::array<std::string, 6> filePaths);
  ~Skybox();

  static VkDescriptorSetLayout skyboxLayout;

  VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
  void update(glm::mat4 viewMatrix);

  std::unique_ptr<Model> cube;

private:
  VulkanContext* vkContext;

  VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

  VkImage image = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;

  VmaAllocation imageAllocation = VK_NULL_HANDLE;

  void transitionImageLayout(VkImage image,
                             VkFormat format,
                             std::vector<size_t> offsets,
                             VkImageLayout oldLayout,
                             VkImageLayout newLayout);

  void copyBufferToImage(VkBuffer buffer,
                         VkImage image,
                         std::vector<size_t> offsets,
                         uint32_t width,
                         uint32_t height);

  void createTextureImageView();

  void setupDescriptors();
};

#endif
