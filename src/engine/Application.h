#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

struct GLFWwindow;
class Renderer;
class Scene;

class Application final
{
public:
  Application(uint32_t width, uint32_t height, std::string_view appTitle);
  ~Application();

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;
  Application(Application&&) noexcept = default;
  Application& operator=(Application&&) noexcept = default;

  void run();

private:
  uint32_t m_width{ 800 };
  uint32_t m_height{ 600 };
  std::string m_title;
  GLFWwindow* m_window{ nullptr };

  std::unique_ptr<Renderer> m_renderer;
  std::unique_ptr<Scene> m_scene;
};
