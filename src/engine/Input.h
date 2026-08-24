#ifndef INPUT_H
#define INPUT_H

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
};

#endif
