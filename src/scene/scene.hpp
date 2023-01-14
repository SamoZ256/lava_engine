#ifndef SCENE_H
#define SCENE_H

#include <entt/entt.hpp>

#include <json/json.h>

#include "backend.hpp"

#include "lvcore/lvcore.hpp"
#include "lvcore/filesystem/filesystem.hpp"

#include "lvutils/libraries/glm.hpp"
#include "lvutils/entity/node.hpp"
#include "lvutils/camera/arcball_camera.hpp"

#include "lvutils/physics/rigid_body.hpp"

namespace nh = nlohmann;

enum SceneState {
    SCENE_STATE_EDITOR,
    SCENE_STATE_RUNTIME
};

class Scene {
public:
    //Camera
    lv::ArcballCamera editorCamera;

    lv::CameraComponent* camera = &editorCamera;
    lv::CameraComponent* primaryCamera = nullptr;
    entt::entity primaryCameraEntity = lv::Entity::nullEntity;

    //Registry
    entt::registry editorRegistry;
    entt::registry runtimeRegistry;

    entt::registry* registry = &editorRegistry;

    //Physics
    lv::PhysicsWorld physicsWorld;

    std::string name;
    bool loaded = false;
    std::string filename;
    SceneState state = SCENE_STATE_EDITOR;

    static unsigned int meshDataFilenameIndex;

    static std::string meshDataDir;

#ifdef LV_BACKEND_VULKAN
    lv::PipelineLayout& deferredLayout;

    Scene(lv::PipelineLayout& aDeferredLayout) : deferredLayout(aDeferredLayout) {}
#endif

    void destroy();

    entt::entity addEntity();

    void render(lv::GraphicsPipeline& graphicsPipeline);

    void renderShadows(lv::GraphicsPipeline& graphicsPipeline);

    void calculateModels();

    void calculateEntityModel(entt::entity entity, glm::mat4 parentModel);

    void loadFromFile();

    void loadEntity(entt::entity entity, nh::json& savedEntity);

    void saveToFile();

    void saveEntity(entt::entity entity, nh::json& savedEntity);

    void setPrimaryCamera(entt::entity entity, lv::CameraComponent* newCamera);

    void updateCameraTransforms();

    void changeState(SceneState newState);

    void start();

    void update(float dt);

    //New entity functions
    void newEntityWithModel(const char* filename);
};

#endif
