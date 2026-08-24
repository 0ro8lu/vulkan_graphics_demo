#include "engine/Application.h"

int
main()
{
#ifdef VULKAN_STEALTH
  setenv("VK_ICD_FILENAMES", VULKAN_ICD_FILEPATH, 1);
  setenv("VK_LAYER_PATH", VULKAN_LAYER_PATH, 1);
#endif

  Application app = Application(800, 600, "Vulkan Engine");
  app.run();

  return 0;
}
