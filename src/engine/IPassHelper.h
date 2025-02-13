#ifndef _I_PASS_HELPER_H_
#define _I_PASS_HELPER_H_

#include "engine/FramebufferAttachment.h"
#include "engine/VulkanContext.h"

#include "engine/Scene.h"
#include "engine/VulkanSwapchain.h"

#include <array>
#include <fstream>
#include <string>
#include <vector>

struct AttachmentData
{
  VkImageView view;
  VkFormat format;
};

class IPassHelper
{
public:
  IPassHelper(VulkanContext* vkContext, const Scene& scene)
    : vkContext(vkContext)
    , scene(scene) {};

  virtual ~IPassHelper() {};

  virtual void draw(VulkanSwapchain* vkSwapchain, const Scene& scene) = 0;
  virtual void recreateAttachments(
    int width,
    int height,
    std::array<AttachmentData, 16> attachmentData) = 0;
  virtual void updateDescriptors(
    std::array<FramebufferAttachment*, 16> attachments) = 0;

  std::vector<char> readFile(const std::string& filename)
  {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
      throw std::runtime_error("failed to open file! " + filename);
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
  };

protected:
  const Scene& scene;
  VulkanContext* vkContext;
};

#endif
