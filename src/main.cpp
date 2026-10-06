#include "engine/Application.h"

int
main()
{
  Application app = Application(800, 600, "Vulkan Engine");
  app.run();

  return 0;
}
