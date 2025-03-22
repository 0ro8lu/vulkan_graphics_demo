#include "GLFW/glfw3.h"

#include "engine/Renderer.h"

#include "engine/VulkanInitializer.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

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

  Scene scene(vkContext);

  int width, height;
  glfwGetFramebufferSize(window, &width, &height);

  scene.camera->resizeCamera(width, height);

  Renderer renderer(vkContext, vkInitializer.vkSwapchain, scene);

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

    moveCamera(window, deltaTime.count(), scene.camera);
    scene.update();

    renderer.draw(scene);

    auto frame_end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
      frame_end - frameStart);

    auto sleep_duration = frame_duration - elapsed;

    if (sleep_duration > std::chrono::microseconds::zero()) {
      std::this_thread::sleep_for(sleep_duration);
    }
    // glfwSetWindowShouldClose(window, true);
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
