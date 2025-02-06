#ifndef _I_PASS_HELPER_H_
#define _I_PASS_HELPER_H_

#include "engine/VulkanContext.h"

#include "engine/Scene.h"
#include "engine/VulkanSwapchain.h"

#include <fstream>
#include <string>
#include <vector>

class IPassHelper
{
public:
  IPassHelper(VulkanContext* vkContext)
    : vkContext(vkContext) {};
  virtual ~IPassHelper() {};

  virtual void init(VulkanSwapchain* vkSwapchain, const Scene& scene) = 0;
  virtual void draw(VulkanSwapchain* vkSwapchain, const Scene& scene) = 0;
  // virtual void draw(VulkanSwapchain* vkSwapchain) = 0;

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
  VulkanContext* vkContext;
};

#endif
