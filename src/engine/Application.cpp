#include "engine/Application.h"
#include "engine/Input.h"
#include "engine/Renderer.h"
#include "engine/Scene.h"
#include "engine/VulkanCheck.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <format>
#include <iostream>

namespace {

GameEvent
pollGameEvents(GLFWwindow* window)
{
  return GameEvent{
    .moveForward = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS,
    .moveBackward = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS,
    .moveLeft = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS,
    .moveRight = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS,
    .lookUp = glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS,
    .lookDown = glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS,
    .lookLeft = glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS,
    .lookRight = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS,
  };
}

} // namespace

Application::Application(uint32_t width,
                         uint32_t height,
                         std::string_view appTitle)
  : m_width(width)
  , m_height(height)
  , m_title(appTitle)
{
  if (!glfwInit()) {
    std::cerr << "\n========================================\n"
              << "[FATAL ERROR]: Failed to initialize GLFW!\n"
              << "========================================\n"
              << std::flush;
    ENGINE_DEBUG_BREAK();
    std::abort();
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  m_window = glfwCreateWindow(static_cast<int>(m_width),
                              static_cast<int>(m_height),
                              m_title.c_str(),
                              nullptr,
                              nullptr);

  if (!m_window) {
    std::cerr << "\n========================================\n"
              << "[FATAL ERROR]: Failed to create GLFW window!\n"
              << "========================================\n"
              << std::flush;
    glfwTerminate();
    ENGINE_DEBUG_BREAK();
    std::abort();
  }

  m_renderer = std::make_unique<Renderer>(m_window);
  m_scene = std::make_unique<Scene>(m_renderer->m_vkContext);
  m_renderer->buildRenderGraph();

  int framebufferWidth, framebufferHeight;
  glfwGetFramebufferSize(m_window, &framebufferWidth, &framebufferHeight);

  m_scene->camera->resizeCamera(framebufferWidth, framebufferHeight);
}

Application::~Application()
{
  if (m_renderer) {
    m_renderer->waitIdle();
  }

  m_scene.reset();
  m_renderer.reset();

  if (m_window) {
    glfwDestroyWindow(m_window);
    m_window = nullptr;
  }

  glfwTerminate();
}

void
Application::run()
{
  auto lastFrameTime = std::chrono::steady_clock::now();
  auto lastFpsUpdateTime = lastFrameTime;
  int frameCount = 0;
  constexpr double fpsUpdateInterval = 1.0;

  while (!glfwWindowShouldClose(m_window)) {
    const auto frameStart = std::chrono::steady_clock::now();
    const std::chrono::duration<float> rawDelta = frameStart - lastFrameTime;
    lastFrameTime = frameStart;

    // Clamp delta time to 100ms max to prevent simulation tunneling/spikes
    const float deltaTime = std::min(rawDelta.count(), 0.1f);

    glfwPollEvents();

    if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      glfwSetWindowShouldClose(m_window, GLFW_TRUE);
    }

    const GameEvent events = pollGameEvents(m_window);
    m_scene->update(deltaTime, events);
    m_renderer->update(*m_scene);
    m_renderer->draw(*m_scene, m_window);

    // FPS Counter via std::format
    frameCount++;
    const auto now = std::chrono::steady_clock::now();
    const double elapsedSinceFpsUpdate =
      std::chrono::duration<double>(now - lastFpsUpdateTime).count();

    if (elapsedSinceFpsUpdate >= fpsUpdateInterval) {
      const double currentFps = frameCount / elapsedSinceFpsUpdate;
      const std::string newTitle =
        std::format("{} | FPS: {:.0f}", m_title, currentFps);
      glfwSetWindowTitle(m_window, newTitle.c_str());

      frameCount = 0;
      lastFpsUpdateTime = now;
    }
  }

  if (m_renderer) {
    m_renderer->waitIdle();
  }
}
