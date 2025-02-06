#ifndef _SKYBOX_H_
#define _SKYBOX_H_

#include <array>
#include <memory>
#include <string>

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include <vulkan/vulkan_core.h>

#include "VulkanDevice.h"
#include "engine/ModelLoading/Model.h"

class Skybox
{
public:
  Skybox(VulkanDevice* vulkanDevice, std::array<std::string, 6> filePaths);
  ~Skybox();

  static VkDescriptorSetLayout skyboxLayout;

  void draw(VkCommandBuffer commandBuffer,
            VkPipelineLayout pipelineLayout,
            glm::mat4 cameraView);

  void update(glm::mat4 viewMatrix);

private:
  VulkanDevice* vulkanDevice;

  VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
  VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

  VkImage image = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;

  VmaAllocation imageAllocation = VK_NULL_HANDLE;

  std::unique_ptr<Model> cube;

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
