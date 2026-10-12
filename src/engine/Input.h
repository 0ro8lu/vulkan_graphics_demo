#pragma once

#include <GLFW/glfw3.h>

struct GameEvent
{
  bool moveForward = false;
  bool moveBackward = false;
  bool moveLeft = false;
  bool moveRight = false;
  bool lookUp = false;
  bool lookDown = false;
  bool lookLeft = false;
  bool lookRight = false;

  static void pollGameEvents(GLFWwindow* window, GameEvent& gameEvent)
  {

    gameEvent.moveForward = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    gameEvent.moveBackward = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    gameEvent.moveLeft = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
    gameEvent.moveRight = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
    gameEvent.lookUp = glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS;
    gameEvent.lookDown = glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS;
    gameEvent.lookLeft = glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS;
    gameEvent.lookRight = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
  }
};
