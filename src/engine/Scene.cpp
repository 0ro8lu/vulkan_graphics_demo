#include "Scene.h"

Scene::Scene(VulkanContext* vkContext)
  : m_vkContext(vkContext)
{
  // --------------------- Init Scene ---------------------
  m_directionalLight = LightManager::createDirectionalLight(
    glm::vec3(-5.0f, 5.0f, -3.0f), glm::vec3(1), true);

  m_pointLights.emplace_back(LightManager::createPointLight(
    glm::vec3(0, 0.5, -10), glm::vec3(5, 0.4, 0.1), true));
  m_pointLights.emplace_back(LightManager::createPointLight(
    glm::vec3(0, -2, -20), glm::vec3(5, 1, 1), false));
  m_pointLights.emplace_back(LightManager::createPointLight(
    glm::vec3(0, -2, -30), glm::vec3(0.5), false));
  m_pointLights.emplace_back(LightManager::createPointLight(
    glm::vec3(0, -2, -30), glm::vec3(5, 2, 3), false));
  m_pointLights.emplace_back(LightManager::createPointLight(
    glm::vec3(0, -2, -40), glm::vec3(10, 0, 0), false));

  m_spotLights.emplace_back(LightManager::createSpotLight(glm::vec3(3, -3, 3),
                                                          glm::vec3(-1, 1, -1),
                                                          glm::vec3(10),
                                                          8.5f,
                                                          9.5f,
                                                          true));
  m_spotLights.emplace_back(LightManager::createSpotLight(glm::vec3(-3, -3, 3),
                                                          glm::vec3(1, 1, -1),
                                                          glm::vec3(0, 10, 0),
                                                          8.5f,
                                                          9.5f,
                                                          true));

  // create light cubes for the lights
  std::string modelPath = MODEL_PATH;
  for (auto& pointLight : m_pointLights) {
    Model cube = Model(modelPath + "cube.glb",
                       vkContext,
                       pointLight.getPosition(),
                       glm::vec3(0.0),
                       0,
                       glm::vec3(0.1f));
    m_lightCubes.push_back(std::move(cube));
  }

  m_camera = std::make_unique<Camera3D>(glm::vec3(0.0, 0.0, 3.0),
                                        glm::vec3(0.0, 0.0, -1.0));

  // create new skybox
  std::string texturePath = TEXTURE_PATH;
  std::array<std::string, 6> files = {
    texturePath + "skybox/right.jpg", texturePath + "skybox/left.jpg",
    texturePath + "skybox/top.jpg",   texturePath + "skybox/bottom.jpg",
    texturePath + "skybox/front.jpg", texturePath + "skybox/back.jpg"
  };
  m_skybox = std::make_unique<Skybox>(vkContext, files);

  // finish initializing models and loading them, setup camera etc.
  Model plane = Model(modelPath + "for_demo/plane.glb",
                      vkContext,
                      glm::vec3(0, 1, 0),
                      glm::vec3(0),
                      0,
                      glm::vec3(50));
  m_models.push_back(std::move(plane));
  Model cube = Model(modelPath + "cube.glb", vkContext, glm::vec3(0, 0, 0));
  m_models.push_back(std::move(cube));
  // Model voyager = Model(modelPath + "voyager.gltf", vkContext, glm::vec3(0,
  // -2, 0)); models.push_back(std::move(voyager));
  Model desk = Model(modelPath + "for_demo/prova_optimized.glb",
                     vkContext,
                     glm::vec3(0.0, 1.0f, -3.0f),
                     glm::vec3(0.0, 1.0, 0.0),
                     180.0f,
                     glm::vec3(1.0f));
  m_models.push_back(std::move(desk));
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
  m_models.clear();
  vkDestroyDescriptorSetLayout(
    m_vkContext->logicalDevice, Model::s_textureLayout, nullptr);
}

void
Scene::update(float deltaTime, const GameEvent& events) noexcept
{
  m_models[1].rotate(1.0, glm::vec3(1.0, 0.5, 0.3));

  const float cameraSpeed = deltaTime * 2.5f;

  glm::vec3 cameraPos = m_camera->getCameraPos();
  glm::vec3 cameraFront = m_camera->getCameraFront();
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
    m_camera->m_pitch -= 40 * deltaTime;
  if (events.lookDown)
    m_camera->m_pitch += 40 * deltaTime;
  if (events.lookLeft)
    m_camera->m_yaw -= 40 * deltaTime;
  if (events.lookRight)
    m_camera->m_yaw += 40 * deltaTime;

  m_camera->m_pitch = glm::clamp(m_camera->m_pitch, -89.0f, 89.0f);

  m_camera->setPos(cameraPos);
  m_camera->setDirection(cameraFront);
  m_camera->update();
}
