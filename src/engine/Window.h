#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include "engine/KeyCode.h"
#include "engine/WindowUserData.h"

struct GLFWwindow;

struct WindowExtent
{
  uint32_t width{ 0 };
  uint32_t height{ 0 };
};

class Window final
{
public:
  struct Config
  {
    uint32_t width{ 1200 };
    uint32_t height{ 800 };
    std::string title{ "Vulkan Engine" };
  };

  explicit Window(const Config& config);
  ~Window();

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;
  Window(Window&&) = delete;
  Window& operator=(Window&&) = delete;

  [[nodiscard]] bool shouldClose() const noexcept;
  void setShouldClose(bool close) const noexcept;
  void pollEvents() const noexcept;
  void waitEventsWhileMinimized() const noexcept;

  [[nodiscard]] WindowExtent framebufferSize() const noexcept;
  [[nodiscard]] bool isMinimized() const noexcept;
  [[nodiscard]] float aspectRatio() const noexcept;

  void setTitle(std::string_view title);

  [[nodiscard]] bool isKeyPressed(KeyCode key) const noexcept;

  using ResizeCallback = std::function<void(uint32_t width, uint32_t height)>;
  void setResizeCallback(ResizeCallback callback)
  {
    m_onResize = std::move(callback);
  }

  [[nodiscard]] GLFWwindow* rawHandle() const noexcept { return m_handle; }

  void onFramebufferResize(int width, int height);

private:
  static void framebufferResizeCallback(GLFWwindow* window,
                                        int width,
                                        int height);

  GLFWwindow* m_handle{ nullptr };
  uint32_t m_width{ 0 };
  uint32_t m_height{ 0 };
  ResizeCallback m_onResize;
  WindowUserData m_userData;
};
