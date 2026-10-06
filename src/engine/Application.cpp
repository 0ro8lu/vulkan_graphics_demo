#include "engine/Application.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <thread>

Application::Application(uint32_t width, uint32_t height, std::string appTitle)
{
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  m_window =
    glfwCreateWindow(width, height, appTitle.c_str(), nullptr, nullptr);

  m_renderer = std::make_unique<Renderer>(m_window);
  m_scene = std::make_unique<Scene>(m_renderer->m_vkContext);
  m_renderer->buildRenderGraph();

  int framebufferWidth, framebufferHeight;
  glfwGetFramebufferSize(m_window, &framebufferWidth, &framebufferHeight);

  m_scene->camera->resizeCamera(framebufferWidth, framebufferHeight);
}

void
Application::run()
{
  const float targetFps = 60.0f;
  auto frame_duration = calculateFrameDuration(targetFps);
  auto lastFrameTime = std::chrono::steady_clock::now();

  int frameCount = 0;
  auto lastFpsUpdateTime = std::chrono::steady_clock::now();
  const double fpsUpdateInterval = 1.0; // Update FPS display every 1.0 second

  while (!glfwWindowShouldClose(m_window)) {
    auto frameStart = std::chrono::steady_clock::now();
    std::chrono::duration<float> deltaTime = frameStart - lastFrameTime;
    lastFrameTime = frameStart;

    glfwPollEvents();

    if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      glfwSetWindowShouldClose(m_window, true);
    }

    auto events = pollGameEvents(m_window);
    m_scene->update(deltaTime.count(), events);
    m_renderer->update(*m_scene);

    m_renderer->draw(*m_scene, m_window);

    // FPS Logic
    frameCount++;
    auto now = std::chrono::steady_clock::now();
    auto elapsedSinceLastUpdate =
      std::chrono::duration<double>(now - lastFpsUpdateTime).count();

    if (elapsedSinceLastUpdate >= fpsUpdateInterval) {
      double fps = frameCount / elapsedSinceLastUpdate;

      std::string newTitle =
        "My Application | FPS: " + std::to_string(static_cast<int>(fps));
      glfwSetWindowTitle(m_window, newTitle.c_str());

      // Reset for the next interval
      frameCount = 0;
      lastFpsUpdateTime = now;
    }

    auto frame_end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
      frame_end - frameStart);

    auto sleep_duration = frame_duration - elapsed;

    if (sleep_duration > std::chrono::microseconds::zero()) {
      std::this_thread::sleep_for(sleep_duration);
    }
  }
  vkDeviceWaitIdle(m_renderer->m_vkContext->logicalDevice);
}

std::chrono::microseconds
Application::calculateFrameDuration(double targetFps)
{
  float microseconds_per_frame = 1.0f / targetFps;

  return std::chrono::duration_cast<std::chrono::microseconds>(
    std::chrono::duration<double>(microseconds_per_frame));
}

GameEvent
Application::pollGameEvents(GLFWwindow* window)
{
  GameEvent event;
  event.moveForward = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
  event.moveBackward = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
  event.moveLeft = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
  event.moveRight = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
  event.lookUp = glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS;
  event.lookDown = glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS;
  event.lookLeft = glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS;
  event.lookRight = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
  return event;
}

Application::~Application()
{
  if (m_window)
    glfwDestroyWindow(m_window);

  glfwTerminate();
}
