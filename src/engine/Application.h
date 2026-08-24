#pragma once

#include "engine/Input.h"
#include "engine/Renderer.h"
#include "engine/Scene.h"

#include "GLFW/glfw3.h"

#include <chrono>
#include <memory>
#include <string>

class Application
{
public:
  Application(uint32_t width, uint32_t height, std::string appTitle);
  ~Application();

  void run();

  Application(Application&&) = delete;
  Application& operator=(Application&&) = delete;
  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

private:
  uint32_t m_width, m_height;
  std::string m_title;
  GLFWwindow* m_window;

  std::chrono::microseconds calculateFrameDuration(double targetFps);
  GameEvent pollGameEvents(GLFWwindow* window);

  std::unique_ptr<Renderer> m_renderer;
  std::unique_ptr<Scene> m_scene;
};
