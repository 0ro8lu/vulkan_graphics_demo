#include "GLFW/glfw3.h"

#include "engine/ShadowMapPass.h"
#include "engine/BlinnPhongPass.h"
#include "engine/HDRPass.h"

#include "engine/Scene.h"
#include "engine/VulkanInitializer.h"
#include <vulkan/vulkan_core.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// #ifndef VMA_DEBUG_LOG_FORMAT
//    #define VMA_DEBUG_LOG_FORMAT(format, ...) do { \
//        printf((format), __VA_ARGS__); \
//        printf("\n"); \
//    } while(false)
// #endif

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <thread>

std::chrono::microseconds
calculateFrameDuration(int target_fps);
void
moveCamera(GLFWwindow* window, float deltaTime, Camera3D* camera);

int
main()
{
  GLFWwindow* window;

  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);

  VulkanInitializer vkInitializer(window);

  VulkanContext* vkContext = vkInitializer.vkContext;
  VulkanSwapchain* vkSwapchain = vkInitializer.vkSwapchain;

  Scene scene(vkContext);

  int width, height;
  glfwGetFramebufferSize(window, &width, &height);

  ShadowMapPass shadowMapPass(vkContext, {}, scene, 1024, 1024); // <- 1024 is the fixed resolution i've chosen for the shadowmaps
  BlinnPhongPass blinnPhongPass(vkContext, {vkSwapchain->depthImageView, vkSwapchain->getDepthImageFormat()}, scene, width, height);
  HDRPass hdrPass(vkContext, {VK_NULL_HANDLE, vkSwapchain->getSwapChainImageFormat()}, scene, width, height);

  // update descriptors
  hdrPass.updateDescriptors({blinnPhongPass.hdrAttachment});

  // swapchain create framebuffer with pass associated with rendering the final
  // step
  vkSwapchain->drawingPass = hdrPass.presentationRenderPass;
  vkSwapchain->createSwapChainFrameBuffer();
  vkSwapchain->onResize = [&blinnPhongPass, &hdrPass, vkSwapchain](int width, int height) {
    blinnPhongPass.recreateAttachments(width, height, {vkSwapchain->depthImageView, vkSwapchain->getDepthImageFormat()});
    hdrPass.recreateAttachments(width, height, {VK_NULL_HANDLE, vkSwapchain->getSwapChainImageFormat()});

    hdrPass.updateDescriptors({blinnPhongPass.hdrAttachment});
  };

  scene.camera->resizeCamera(width, height);

  auto frame_duration = calculateFrameDuration(60.0f);
  frame_duration = std::chrono::microseconds(8333);
  auto lastFrameTime = std::chrono::steady_clock::now();

  while (!glfwWindowShouldClose(window)) {
    auto frameStart = std::chrono::steady_clock::now();
    std::chrono::duration<float> deltaTime = frameStart - lastFrameTime;
    lastFrameTime = frameStart;

    glfwPollEvents();

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, true);
    }

    vkSwapchain->prepareFrame();
    moveCamera(window, deltaTime.count(), scene.camera);
    scene.update();

    shadowMapPass.draw(vkSwapchain, scene);
    blinnPhongPass.draw(vkSwapchain, scene);
    hdrPass.draw(vkSwapchain, scene);

    vkSwapchain->submitFrame();

    auto frame_end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
      frame_end - frameStart);

    auto sleep_duration = frame_duration - elapsed;

    if (sleep_duration > std::chrono::microseconds::zero()) {
      std::this_thread::sleep_for(sleep_duration);
    }
  }
  vkDeviceWaitIdle(vkContext->logicalDevice);

  return 0;
}

std::chrono::microseconds
calculateFrameDuration(int target_fps)
{
  float microseconds_per_frame = 1.0f / static_cast<float>(target_fps);

  return std::chrono::duration_cast<std::chrono::microseconds>(
    std::chrono::duration<float>(microseconds_per_frame));
}

void
moveCamera(GLFWwindow* window, float deltaTime, Camera3D* camera)
{
  const float cameraSpeed = deltaTime * 2.5f; // adjust accordingly

  glm::vec3 cameraPos = camera->getCameraPos();
  glm::vec3 cameraFront = camera->getCameraFront();
  glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    cameraPos += cameraSpeed * cameraFront;
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    cameraPos -= cameraSpeed * cameraFront;
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    cameraPos -=
      glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    cameraPos +=
      glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;

  if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
    camera->pitch -= 40 * deltaTime;
  if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
    camera->pitch += 40 * deltaTime;
  if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
    camera->yaw -= 40 * deltaTime;
  if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
    camera->yaw += 40 * deltaTime;

  if (camera->pitch > 89.0f)
    camera->pitch = 89.0f;
  if (camera->pitch < -89.0f)
    camera->pitch = -89.0f;

  camera->setPos(cameraPos);
  camera->setDirection(cameraFront);

  camera->update();
}
