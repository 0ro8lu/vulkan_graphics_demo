#include "Scene.h"

Scene::Scene(VulkanContext* vkContext)
  : vkContext(vkContext)
{
  // --------------------- Init Scene ---------------------
  directionalLight = LightManager::createDirectionalLight(
    glm::vec3(-5.0f, 5.0f, -3.0f), glm::vec3(1), true);

  pointLights.emplace_back(LightManager::createPointLight(
    glm::vec3(0, 0.5, -10), glm::vec3(5, 0.4, 0.1), true));
  pointLights.emplace_back(LightManager::createPointLight(
    glm::vec3(0, -2, -20), glm::vec3(5, 1, 1), false));
  pointLights.emplace_back(LightManager::createPointLight(
    glm::vec3(0, -2, -30), glm::vec3(0.5), false));
  pointLights.emplace_back(LightManager::createPointLight(
    glm::vec3(0, -2, -30), glm::vec3(5, 2, 3), false));
  pointLights.emplace_back(LightManager::createPointLight(
    glm::vec3(0, -2, -40), glm::vec3(10, 0, 0), false));

  spotLights.emplace_back(LightManager::createSpotLight(glm::vec3(3, -3, 3),
                                                        glm::vec3(-1, 1, -1),
                                                        glm::vec3(10),
                                                        8.5f,
                                                        9.5f,
                                                        true));
  spotLights.emplace_back(LightManager::createSpotLight(glm::vec3(-3, -3, 3),
                                                        glm::vec3(1, 1, -1),
                                                        glm::vec3(0, 10, 0),
                                                        8.5f,
                                                        9.5f,
                                                        true));

  // create light cubes for the lights
  std::string modelPath = MODEL_PATH;
  for (auto& pointLight : pointLights) {
    Model cube = Model(modelPath + "cube.glb",
                       vkContext,
                       pointLight.getPosition(),
                       glm::vec3(0.0),
                       0,
                       glm::vec3(0.1f));
    lightCubes.push_back(std::move(cube));
  }

  camera = std::make_unique<Camera3D>(glm::vec3(0.0, 0.0, 3.0),
                                      glm::vec3(0.0, 0.0, -1.0));

  // create new skybox
  std::string texturePath = TEXTURE_PATH;
  std::array<std::string, 6> files = {
    texturePath + "skybox/right.jpg", texturePath + "skybox/left.jpg",
    texturePath + "skybox/top.jpg",   texturePath + "skybox/bottom.jpg",
    texturePath + "skybox/front.jpg", texturePath + "skybox/back.jpg"
  };
  skybox = std::make_unique<Skybox>(vkContext, files);

  // finish initializing models and loading them, setup camera etc.
  Model plane = Model(modelPath + "for_demo/plane.glb",
                      vkContext,
                      glm::vec3(0, 1, 0),
                      glm::vec3(0),
                      0,
                      glm::vec3(50));
  models.push_back(std::move(plane));
  Model cube = Model(modelPath + "cube.glb", vkContext, glm::vec3(0, 0, 0));
  models.push_back(std::move(cube));
  // Model voyager = Model(modelPath + "voyager.gltf", vkContext, glm::vec3(0,
  // -2, 0)); models.push_back(std::move(voyager));
  Model desk = Model(modelPath + "for_demo/prova_optimized.glb",
                     vkContext,
                     glm::vec3(0.0, 1.0f, -3.0f),
                     glm::vec3(0.0, 1.0, 0.0),
                     180.0f,
                     glm::vec3(1.0f));
  models.push_back(std::move(desk));
  // Model rare = Model(modelPath + "for_demo/rare_logo/rare.glb",
  //                         vkContext,
  //                         glm::vec3(-1.85, -0.7, -19.5f),
  //                         glm::vec3(1.0, 0.0, 0.0),
  //                         -90.0f,
  //                         glm::vec3(0.1f));
  //  models.push_back(std::move(rare));
}

Scene::~Scene()
{
  // destroy models
  models.clear();
  vkDestroyDescriptorSetLayout(
    vkContext->logicalDevice, Model::textureLayout, nullptr);
}

void
Scene::update(float deltaTime, GameEvent events)
{
  models[1].rotate(1.0, glm::vec3(1.0, 0.5, 0.3));

  const float cameraSpeed = deltaTime * 2.5f;

  glm::vec3 cameraPos = camera->getCameraPos();
  glm::vec3 cameraFront = camera->getCameraFront();
  glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

  if (events.moveForward)
    cameraPos += cameraSpeed * cameraFront;
  if (events.moveBackward)
    cameraPos -= cameraSpeed * cameraFront;
  if (events.moveLeft)
    cameraPos -=
      glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
  if (events.moveRight)
    cameraPos +=
      glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;

  if (events.lookUp)
    camera->pitch -= 40 * deltaTime;
  if (events.lookDown)
    camera->pitch += 40 * deltaTime;
  if (events.lookLeft)
    camera->yaw -= 40 * deltaTime;
  if (events.lookRight)
    camera->yaw += 40 * deltaTime;

  camera->pitch = glm::clamp(camera->pitch, -89.0f, 89.0f);

  camera->setPos(cameraPos);
  camera->setDirection(cameraFront);
  camera->update();
}
