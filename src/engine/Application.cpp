#include "engine/Application.h"
#include "engine/Input.h"
#include "engine/Renderer.h"
#include "engine/Scene.h"
#include "engine/Window.h"

#include <chrono>
#include <format>

Application::Application(uint32_t width,
                         uint32_t height,
                         std::string_view appTitle)
  : m_width(width)
  , m_height(height)
  , m_title(appTitle)
{
  m_window = std::make_unique<Window>(Window::Config{
    .width = m_width,
    .height = m_height,
    .title = m_title,
  });

  m_renderer = std::make_unique<Renderer>(*m_window);
  m_scene = std::make_unique<Scene>(m_renderer->device(),
                                    m_renderer->allocator(),
                                    m_renderer->samplerCache(),
                                    m_renderer->immediateCommands());
  m_renderer->buildRenderGraph();

  const auto initialSize = m_window->framebufferSize();
  m_scene->m_camera->resizeCamera(initialSize.width, initialSize.height);

  m_window->setResizeCallback([this](uint32_t w, uint32_t h) {
    if (w == 0 || h == 0) {
      return;
    }
    m_scene->m_camera->resizeCamera(w, h);
    m_renderer->onResize(w, h);
  });
}

Application::~Application()
{
  if (m_renderer) {
    m_renderer->waitIdle();
  }

  m_scene.reset();
  m_renderer.reset();
  m_window.reset();
}

void
Application::run()
{
  auto lastFrameTime = std::chrono::steady_clock::now();
  auto lastFpsUpdateTime = lastFrameTime;
  int frameCount = 0;
  constexpr double fpsUpdateInterval = 1.0;

  GameEvent gameEvents = {};

  while (!m_window->shouldClose()) {
    m_window->pollEvents();

    if (m_window->shouldClose()) {
      break;
    }

    if (m_window->isMinimized()) {
      m_window->waitEventsWhileMinimized();
      const auto resumeTime = std::chrono::steady_clock::now();
      lastFrameTime = resumeTime;
      lastFpsUpdateTime = resumeTime;
      frameCount = 0;
      continue;
    }

    const auto frameStart = std::chrono::steady_clock::now();
    const std::chrono::duration<float> rawDelta = frameStart - lastFrameTime;
    lastFrameTime = frameStart;

    // Clamp delta time to 100ms max to prevent simulation tunneling/spikes
    const float deltaTime = std::min(rawDelta.count(), 0.1f);

    if (m_window->isKeyPressed(KeyCode::Escape)) {
      m_window->setShouldClose(true);
    }

    GameEvent::pollGameEvents(*m_window, gameEvents);
    m_scene->update(deltaTime, gameEvents);
    m_renderer->draw(m_scene->getRenderData());

    // FPS Counter via std::format
    frameCount++;
    const auto now = std::chrono::steady_clock::now();
    const double elapsedSinceFpsUpdate =
      std::chrono::duration<double>(now - lastFpsUpdateTime).count();

    if (elapsedSinceFpsUpdate >= fpsUpdateInterval) {
      const double currentFps = frameCount / elapsedSinceFpsUpdate;
      const std::string newTitle =
        std::format("{} | FPS: {:.0f}", m_title, currentFps);
      m_window->setTitle(newTitle);

      frameCount = 0;
      lastFpsUpdateTime = now;
    }
  }

  if (m_renderer) {
    m_renderer->waitIdle();
  }
}
