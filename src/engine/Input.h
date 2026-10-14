#pragma once

#include "engine/KeyCode.h"
#include "engine/Window.h"

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

  static void pollGameEvents(const Window& window, GameEvent& gameEvent)
  {
    gameEvent.moveForward = window.isKeyPressed(KeyCode::W);
    gameEvent.moveBackward = window.isKeyPressed(KeyCode::S);
    gameEvent.moveLeft = window.isKeyPressed(KeyCode::A);
    gameEvent.moveRight = window.isKeyPressed(KeyCode::D);
    gameEvent.lookUp = window.isKeyPressed(KeyCode::Up);
    gameEvent.lookDown = window.isKeyPressed(KeyCode::Down);
    gameEvent.lookLeft = window.isKeyPressed(KeyCode::Left);
    gameEvent.lookRight = window.isKeyPressed(KeyCode::Right);
  }
};
