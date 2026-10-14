#include <GLFW/glfw3.h>

#include "engine/Window.h"
#include "engine/Error.h"

void
Window::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
  auto* userData =
    static_cast<WindowUserData*>(glfwGetWindowUserPointer(window));
  if (userData && userData->window) {
    userData->window->onFramebufferResize(width, height);
  }
}

Window::Window(const Config& config)
{
  if (!glfwInit()) {
    ReportFatalError("Failed to initialize GLFW!");
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  m_handle = glfwCreateWindow(static_cast<int>(config.width),
                              static_cast<int>(config.height),
                              config.title.c_str(),
                              nullptr,
                              nullptr);

  if (!m_handle) {
    glfwTerminate();
    ReportFatalError("Failed to create GLFW window!");
  }

  int fbWidth = 0;
  int fbHeight = 0;
  glfwGetFramebufferSize(m_handle, &fbWidth, &fbHeight);
  m_width = static_cast<uint32_t>(fbWidth);
  m_height = static_cast<uint32_t>(fbHeight);

  m_userData.window = this;
  glfwSetWindowUserPointer(m_handle, &m_userData);
  glfwSetFramebufferSizeCallback(m_handle, framebufferResizeCallback);
}

Window::~Window()
{
  if (m_handle) {
    glfwSetWindowUserPointer(m_handle, nullptr);
    glfwDestroyWindow(m_handle);
    m_handle = nullptr;
  }
  glfwTerminate();
}

bool
Window::shouldClose() const noexcept
{
  return m_handle ? glfwWindowShouldClose(m_handle) : true;
}

void
Window::setShouldClose(bool close) const noexcept
{
  if (m_handle) {
    glfwSetWindowShouldClose(m_handle, close ? GLFW_TRUE : GLFW_FALSE);
  }
}

void
Window::pollEvents() const noexcept
{
  glfwPollEvents();
}

void
Window::waitEventsWhileMinimized() const noexcept
{
  if (!m_handle) {
    return;
  }

  while (isMinimized()) {
    if (shouldClose()) {
      return;
    }
    glfwWaitEvents();
  }
}

WindowExtent
Window::framebufferSize() const noexcept
{
  if (!m_handle) {
    return { 0, 0 };
  }
  int fbWidth = 0;
  int fbHeight = 0;
  glfwGetFramebufferSize(m_handle, &fbWidth, &fbHeight);
  return { static_cast<uint32_t>(fbWidth), static_cast<uint32_t>(fbHeight) };
}

bool
Window::isMinimized() const noexcept
{
  if (!m_handle) {
    return true;
  }
  if (glfwGetWindowAttrib(m_handle, GLFW_ICONIFIED) == GLFW_TRUE) {
    return true;
  }
  int fbWidth = 0;
  int fbHeight = 0;
  glfwGetFramebufferSize(m_handle, &fbWidth, &fbHeight);
  return fbWidth == 0 || fbHeight == 0;
}

float
Window::aspectRatio() const noexcept
{
  if (!m_handle) {
    return 1.0f;
  }
  int fbWidth = 0;
  int fbHeight = 0;
  glfwGetFramebufferSize(m_handle, &fbWidth, &fbHeight);
  if (fbHeight == 0) {
    return 1.0f;
  }
  return static_cast<float>(fbWidth) / static_cast<float>(fbHeight);
}

void
Window::setTitle(std::string_view title)
{
  if (m_handle) {
    glfwSetWindowTitle(m_handle, std::string(title).c_str());
  }
}

bool
Window::isKeyPressed(KeyCode key) const noexcept
{
  if (!m_handle) {
    return false;
  }
  return glfwGetKey(m_handle, static_cast<int>(key)) == GLFW_PRESS;
}

void
Window::onFramebufferResize(int width, int height)
{
  m_width = static_cast<uint32_t>(width);
  m_height = static_cast<uint32_t>(height);
  if (m_onResize) {
    m_onResize(m_width, m_height);
  }
}
