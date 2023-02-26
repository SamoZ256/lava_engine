#include "scene.hpp"

#include <thread>

#include "lvutils/entity/tag.hpp"
#include "lvutils/entity/mesh.hpp"
#include "lvutils/entity/transform.hpp"
#include "lvutils/entity/material.hpp"
#include "lvutils/entity/entity.hpp"
#include "lvutils/entity/mesh_loader.hpp"
#include "lvutils/scripting/script_manager.hpp"
#include "lvutils/camera/camera.hpp"
#include "lvutils/disk_io/disk_io.hpp"

#include "../editor/editor.hpp"

std::string Scene::aoTypeStr[AMBIENT_OCCLUSION_TYPE_COUNT] = {
    "None", "SSAO", "HBAO"
};

lv::ColliderComponent* getEntityColliderComponent(lv::Entity& entity) {
    lv::ColliderComponent* colliderComponent = nullptr;
    if (entity.hasComponent<lv::SphereColliderComponent>()) colliderComponent = &entity.getComponent<lv::SphereColliderComponent>();
    if (entity.hasComponent<lv::BoxColliderComponent>()) colliderComponent = &entity.getComponent<lv::BoxColliderComponent>();

    return colliderComponent;
}

unsigned int Scene::meshDataFilenameIndex = 0;
std::string Scene::meshDataDir = "";

void Scene::destroy() {
    auto view = editorRegistry.view<lv::MeshComponent>();
    for (auto &entity : view) {
        auto &modelComponent = editorRegistry.get<lv::MeshComponent>(entity);

        modelComponent.destroy();
    }

    auto view2 = editorRegistry.view<lv::MaterialComponent>();
    for (auto &entity : view2) {
        auto &materialComponent = editorRegistry.get<lv::MaterialComponent>(entity);

        materialComponent.destroy();
    }
}

entt::entity Scene::addEntity() {
    entt::entity entity = registry->create();
    registry->emplace<lv::TagComponent>(entity);
    registry->emplace<lv::NodeComponent>(entity);

    return entity;
}

void Scene::render(lv::GraphicsPipeline& graphicsPipeline) {
    auto view = registry->view<lv::MeshComponent, lv::TransformComponent, lv::MaterialComponent>();
    for (auto &entity : view) {
        auto &meshComponent = registry->get<lv::MeshComponent>(entity);
        auto &transformComponent = registry->get<lv::TransformComponent>(entity);
        auto &materialComponent = registry->get<lv::MaterialComponent>(entity);

        materialComponent.uploadUniforms();
        graphicsPipeline.uploadPushConstants(&transformComponent.model, 0);
        meshComponent.render();
    }
}

void Scene::renderShadows(lv::GraphicsPipeline& graphicsPipeline) {
    auto view = registry->view<lv::MeshComponent, lv::TransformComponent>();
    for (auto &entity : view) {
        auto &meshComponent = registry->get<lv::MeshComponent>(entity);
        auto &transformComponent = registry->get<lv::TransformComponent>(entity);

        graphicsPipeline.uploadPushConstants(&transformComponent.model.model, 0);
        meshComponent.renderShadows();
    }
}

void Scene::calculateModels() {
    auto view = registry->view<lv::TransformComponent>();
    for (auto& entity : view) {
        if (registry->get<lv::NodeComponent>(entity).parent == lv::Entity::nullEntity)
            calculateEntityModel(entity, glm::mat4(1.0f));
    }
}

void Scene::calculateEntityModel(entt::entity entity, glm::mat4 parentModel) {
    glm::mat4 crntModel = parentModel;
    if (registry->all_of<lv::TransformComponent>(entity)) {
        lv::TransformComponent& transformComponent = registry->get<lv::TransformComponent>(entity);
        transformComponent.calcModel(parentModel);
        crntModel = transformComponent.model.model;
    }

    for (auto& childEntity : registry->get<lv::NodeComponent>(entity).childs) {
        if (registry->all_of<lv::TransformComponent>(childEntity)) {
            calculateEntityModel(childEntity, crntModel);
        }
    }
}

void Scene::loadFromFile() {
    nh::json JSON;
    std::string text = lv::readFile(filename.c_str());
    JSON = nh::json::parse(text);

    name = JSON["name"];

    //std::cout << filename << std::endl;
    //std::cout << name << std::endl;
    for (auto& element : JSON["entities"].items()) {
        entt::entity entity = addEntity();
        auto& savedEntity = JSON["entities"][std::to_string((unsigned int)entity)];
        loadEntity(entity, savedEntity);
    }

    loaded = true;
}

void Scene::loadEntity(entt::entity entity, nh::json& savedEntity) {
    editorRegistry.get<lv::TagComponent>(entity).name = savedEntity["name"];
    //std::cout << entityName << std::endl;
    for (auto& element : savedEntity.items()) {
        std::string componentName = element.key();
        //Transform component
        if (componentName == TRANSFORM_COMPONENT_NAME) {
            auto& transformComponent = editorRegistry.emplace<lv::TransformComponent>(entity);
            auto& component = savedEntity[TRANSFORM_COMPONENT_NAME];
            
            transformComponent.position.x = component["position"]["x"];
            transformComponent.position.y = component["position"]["y"];
            transformComponent.position.z = component["position"]["z"];

            transformComponent.rotation.x = component["rotation"]["x"];
            transformComponent.rotation.y = component["rotation"]["y"];
            transformComponent.rotation.z = component["rotation"]["z"];

            transformComponent.scale.x = component["scale"]["x"];
            transformComponent.scale.y = component["scale"]["y"];
            transformComponent.scale.z = component["scale"]["z"];
        }
        //Mesh component
        if (componentName == MESH_COMPONENT_NAME) {
            lv::MeshComponent& meshComponent = editorRegistry.emplace<lv::MeshComponent>(entity, deferredLayout);
            auto& component = savedEntity[MESH_COMPONENT_NAME];

            std::string vertDataFilename = component["vertDataFilename"];
            std::string indDataFilename = component["indDataFilename"];
            meshComponent.loadFromFile(0, vertDataFilename.c_str(), indDataFilename.c_str());

            for (uint16_t texIndex = 0; texIndex < LV_MESH_TEXTURE_COUNT; texIndex++) {
                std::string texIndexStr = std::to_string(texIndex);
                if (component["textures"].contains(texIndexStr)) {
                    std::string filename = component["textures"][texIndexStr]["filename"];
                    //std::cout << filename << std::endl;
                    lv::Texture* texture = lv::MeshComponent::loadTextureFromFile(0, filename.c_str(), (texIndex == 0 ? LV_FORMAT_R8G8B8A8_UNORM_SRGB : LV_FORMAT_R8G8B8A8_UNORM));
                    meshComponent.setTexture(texture, texIndex);
                }
            }
            meshComponent.initDescriptorSet();
        }
        //Material component
        if (componentName == MATERIAL_COMPONENT_NAME) {
            auto& materialComponent = editorRegistry.emplace<lv::MaterialComponent>(entity, deferredLayout);
            auto& component = savedEntity[MATERIAL_COMPONENT_NAME];
            
            materialComponent.material.albedo.x = component["albedo"]["r"];
            materialComponent.material.albedo.y = component["albedo"]["g"];
            materialComponent.material.albedo.z = component["albedo"]["b"];

            materialComponent.material.roughness = component["roughness"];
            materialComponent.material.metallic = component["metallic"];
        }
        //Script component
        if (componentName == SCRIPT_COMPONENT_NAME) {
            lv::ScriptComponent& scriptComponent = editorRegistry.emplace<lv::ScriptComponent>(entity);
            auto& component = savedEntity[SCRIPT_COMPONENT_NAME];

            std::string scriptFilename = component["filename"];
            scriptComponent.loadScript(scriptFilename.c_str(), entity, &runtimeRegistry);
        }
        //Camera component
        if (componentName == CAMERA_COMPONENT_NAME) {
            lv::CameraComponent& cameraComponent = editorRegistry.emplace<lv::CameraComponent>(entity);
            auto& component = savedEntity[CAMERA_COMPONENT_NAME];
            
            cameraComponent.position.x = component["position"]["x"];
            cameraComponent.position.y = component["position"]["y"];
            cameraComponent.position.z = component["position"]["z"];

            cameraComponent.direction.x = component["direction"]["x"];
            cameraComponent.direction.y = component["direction"]["y"];
            cameraComponent.direction.z = component["direction"]["z"];

            cameraComponent.active = component["active"];

            if (cameraComponent.active) {
                primaryCameraEntity = entity;
            }
        }
        //Sphere collider
        if (componentName == SPHERE_COLLIDER_COMPONENT_NAME) {
            lv::SphereColliderComponent& sphereColliderComponent = editorRegistry.emplace<lv::SphereColliderComponent>(entity);
            auto& component = savedEntity[SPHERE_COLLIDER_COMPONENT_NAME];

            sphereColliderComponent.radius = component["radius"];
        }
        //Box collider
        if (componentName == BOX_COLLIDER_COMPONENT_NAME) {
            lv::BoxColliderComponent& boxColliderComponent = editorRegistry.emplace<lv::BoxColliderComponent>(entity);
            auto& component = savedEntity[BOX_COLLIDER_COMPONENT_NAME];

            boxColliderComponent.scale.x = component["scale"]["x"];
            boxColliderComponent.scale.y = component["scale"]["y"];
            boxColliderComponent.scale.z = component["scale"]["z"];
        }
        //Rigid body
        if (componentName == RIGID_BODY_COMPONENT_NAME) {
            lv::RigidBodyComponent& rigidBodyComponent = editorRegistry.emplace<lv::RigidBodyComponent>(entity, physicsWorld);
            auto& component = savedEntity[RIGID_BODY_COMPONENT_NAME];

            rigidBodyComponent.origin.x = component["origin"]["x"];
            rigidBodyComponent.origin.y = component["origin"]["y"];
            rigidBodyComponent.origin.z = component["origin"]["z"];

            rigidBodyComponent.mass = component["mass"];
            rigidBodyComponent.restitution = component["restitution"];
            rigidBodyComponent.friction = component["friction"];
        }

        //Loading child entities
        if (componentName == "childs") {
            for (auto& childElement : savedEntity["childs"].items()) {
                std::string child = childElement.key();
                entt::entity childEntity = addEntity();
                editorRegistry.get<lv::NodeComponent>(entity).childs.push_back(childEntity);
                editorRegistry.get<lv::NodeComponent>(childEntity).parent = entity;
                auto& childSavedEntity = savedEntity["childs"][child];
                loadEntity(childEntity, childSavedEntity);
            }
        }
    }
}

void Scene::saveToFile() {
    nh::json JSON;

    JSON["name"] = name;

    auto view = editorRegistry.view<lv::TagComponent>();
    for (auto& entity : view) {
        if (editorRegistry.get<lv::NodeComponent>(entity).parent == lv::Entity::nullEntity) {
            auto& savedEntity = JSON["entities"][std::to_string((unsigned int)entity)];
            saveEntity(entity, savedEntity);
        }
    }

    std::ofstream out(filename.c_str());
    //json::printer pp9 = json::printer::pretty_printer();
    out << JSON.dump(4) << std::endl;
}

void Scene::saveEntity(entt::entity entity, nh::json& savedEntity) {
    savedEntity["name"] = editorRegistry.get<lv::TagComponent>(entity).name;
    //Transform component
    if (editorRegistry.all_of<lv::TransformComponent>(entity)) {
        auto& transformComponent = editorRegistry.get<lv::TransformComponent>(entity);
        auto& component = savedEntity[TRANSFORM_COMPONENT_NAME];

        component["position"]["x"] = transformComponent.position.x;
        component["position"]["y"] = transformComponent.position.y;
        component["position"]["z"] = transformComponent.position.z;

        component["rotation"]["x"] = transformComponent.rotation.x;
        component["rotation"]["y"] = transformComponent.rotation.y;
        component["rotation"]["z"] = transformComponent.rotation.z;

        component["scale"]["x"] = transformComponent.scale.x;
        component["scale"]["y"] = transformComponent.scale.y;
        component["scale"]["z"] = transformComponent.scale.z;
    }
    //Model component
    /*
    if (editorRegistry.all_of<lv::ModelComponent>(entity)) {
        auto& modelComponent = editorRegistry.get<lv::ModelComponent>(entity);
        auto& component = JSON["entities"][uuid][MODEL_COMPONENT_NAME];

        component["modelType"] = modelComponent.modelType;
        component["loaded"] = modelComponent.loaded;
        component["filename"] = modelComponent.filename;
        component["flipUVs"] = modelComponent.flipUVs;
    }
    */
    //Mesh component
    if (editorRegistry.all_of<lv::MeshComponent>(entity)) {
        lv::MeshComponent& meshComponent = editorRegistry.get<lv::MeshComponent>(entity);
        auto& component = savedEntity[MESH_COMPONENT_NAME];

        for (uint16_t i = 0; i < LV_MESH_TEXTURE_COUNT; i++) {
            lv::Texture* texture = meshComponent.textures[i];
            if (texture->filename != "")
                component["textures"][std::to_string(i)]["filename"] = texture->filename;
        }

        std::string vertDataFilename = Scene::meshDataDir + "/mesh" + std::to_string(meshDataFilenameIndex) + "_vert";
        component["vertDataFilename"] = vertDataFilename;
        //std::ofstream vertDataOut(vertDataFilename.c_str(), std::ios::out | std::ios::binary);
        //vertDataOut.write((char*)meshComponent.vertices.data(), meshComponent.vertices.size() * sizeof(lv::MainVertex));
        lv::dumpRawBinary(vertDataFilename.c_str(), meshComponent.vertices.data(), meshComponent.vertices.size() * sizeof(lv::MainVertex));
        //vertDataOut << meshComponent.vertices.data() << std::endl;
        //std::cout << vertDataFilename << std::endl;

        std::string indDataFilename = Scene::meshDataDir + "/mesh" + std::to_string(meshDataFilenameIndex) + "_ind";
        component["indDataFilename"] = indDataFilename;
        //std::ofstream indDataOut(indDataFilename.c_str(), std::ios::out | std::ios::binary);
        //indDataOut.write((char*)meshComponent.indices.data(), meshComponent.indices.size() * sizeof(unsigned int));
        lv::dumpRawBinary(indDataFilename.c_str(), meshComponent.indices.data(), meshComponent.indices.size() * sizeof(unsigned int));
        //indDataOut << meshComponent.indices.data() << std::endl;

        meshDataFilenameIndex++;
    }
    //Material component
    if (editorRegistry.all_of<lv::MaterialComponent>(entity)) {
        lv::MaterialComponent& materialComponent = editorRegistry.get<lv::MaterialComponent>(entity);
        auto& component = savedEntity[MATERIAL_COMPONENT_NAME];

        component["albedo"]["r"] = materialComponent.material.albedo.r;
        component["albedo"]["g"] = materialComponent.material.albedo.g;
        component["albedo"]["b"] = materialComponent.material.albedo.b;
        
        component["roughness"] = materialComponent.material.roughness;
        component["metallic"] = materialComponent.material.metallic;
    }
    //Script component
    if (editorRegistry.all_of<lv::ScriptComponent>(entity)) {
        lv::ScriptComponent& scriptComponent = editorRegistry.get<lv::ScriptComponent>(entity);

        if (scriptComponent.isValid()) {
            auto& component = savedEntity[SCRIPT_COMPONENT_NAME];
            component["filename"] = scriptComponent.script->filename;
        }
    }
    //Camera component
    if (editorRegistry.all_of<lv::CameraComponent>(entity)) {
        lv::CameraComponent& cameraComponent = editorRegistry.get<lv::CameraComponent>(entity);
        auto& component = savedEntity[CAMERA_COMPONENT_NAME];
        
        component["position"]["x"] = cameraComponent.position.x;
        component["position"]["y"] = cameraComponent.position.y;
        component["position"]["z"] = cameraComponent.position.z;

        component["direction"]["x"] = cameraComponent.direction.x;
        component["direction"]["y"] = cameraComponent.direction.y;
        component["direction"]["z"] = cameraComponent.direction.z;

        component["active"] = cameraComponent.active;
    }
    //Sphere collider
    if (editorRegistry.all_of<lv::SphereColliderComponent>(entity)) {
        lv::SphereColliderComponent& sphereColliderComponent = editorRegistry.get<lv::SphereColliderComponent>(entity);
        auto& component = savedEntity[SPHERE_COLLIDER_COMPONENT_NAME];

        component["radius"] = sphereColliderComponent.radius;
    }
    //Box collider
    if (editorRegistry.all_of<lv::BoxColliderComponent>(entity)) {
        lv::BoxColliderComponent& boxColliderComponent = editorRegistry.get<lv::BoxColliderComponent>(entity);
        auto& component = savedEntity[BOX_COLLIDER_COMPONENT_NAME];

        component["scale"]["x"] = boxColliderComponent.scale.x;
        component["scale"]["y"] = boxColliderComponent.scale.y;
        component["scale"]["z"] = boxColliderComponent.scale.z;
    }
    //Rigid body
    if (editorRegistry.all_of<lv::RigidBodyComponent>(entity)) {
        lv::RigidBodyComponent& rigidBodyComponent = editorRegistry.get<lv::RigidBodyComponent>(entity);
        auto& component = savedEntity[RIGID_BODY_COMPONENT_NAME];

        component["origin"]["x"] = rigidBodyComponent.origin.x;
        component["origin"]["y"] = rigidBodyComponent.origin.y;
        component["origin"]["z"] = rigidBodyComponent.origin.z;

        component["mass"] = rigidBodyComponent.mass;
        component["restitution"] = rigidBodyComponent.restitution;
        component["friction"] = rigidBodyComponent.friction;
    }

    //Saving child entities
    auto& childs = editorRegistry.get<lv::NodeComponent>(entity).childs;
    for (unsigned int i = 0; i < childs.size(); i++) {
        entt::entity childEntity = childs[i];
        auto& childSavedEntity = savedEntity["childs"][std::to_string(i)];
        saveEntity(childEntity, childSavedEntity);
    }
}

void Scene::setPrimaryCamera(entt::entity entity, lv::CameraComponent* newCamera) {
    primaryCamera->active = false;
    newCamera->active = true;
    primaryCamera = newCamera;
    primaryCameraEntity = entity;
}

void Scene::updateCameraTransforms() {
    auto view = registry->view<lv::CameraComponent, lv::TransformComponent>();
    for (auto& entity : view) {
        lv::CameraComponent& cameraComponent = registry->get<lv::CameraComponent>(entity);
        lv::TransformComponent& transformComponent = registry->get<lv::TransformComponent>(entity);

        cameraComponent.position = transformComponent.position;
        cameraComponent.rotation = transformComponent.rotation;
    }
}

void Scene::changeState(SceneState newState) {
    if (state != newState) {
        state = newState;

        if (state == SCENE_STATE_EDITOR) {
            physicsWorld.destroy();

            //TODO: destroy components of entities created during the runtime

            auto view = runtimeRegistry.view<lv::TagComponent>();
            for (auto& entity : view) {
                if (runtimeRegistry.all_of<lv::ColliderComponent>(entity)) {
                    runtimeRegistry.get<lv::ColliderComponent>(entity).destroy();

                    if (runtimeRegistry.all_of<lv::RigidBodyComponent>(entity)) {
                        runtimeRegistry.get<lv::RigidBodyComponent>(entity).destroy();
                    }
                }

                runtimeRegistry.destroy(entity);
            }

            if (g_editor->activeViewport == VIEWPORT_TYPE_GAME) {
                primaryCamera = &editorRegistry.get<lv::CameraComponent>(primaryCameraEntity);
                camera = primaryCamera;
            }

            registry = &editorRegistry;
        } else if (state == SCENE_STATE_RUNTIME) {
            physicsWorld.init();

            auto view = editorRegistry.view<lv::TagComponent>();
            for (auto& entity : view) {
                lv::Entity editorEntity(entity, &editorRegistry);
                lv::Entity runtimeEntity(runtimeRegistry.create(entity), &runtimeRegistry);
                
                //Copying components
                runtimeEntity.addComponent<lv::TagComponent>(editorEntity.getComponent<lv::TagComponent>());
                runtimeEntity.addComponent<lv::NodeComponent>(editorEntity.getComponent<lv::NodeComponent>());
                
                if (editorEntity.hasComponent<lv::TransformComponent>())
                    runtimeEntity.addComponent<lv::TransformComponent>(editorEntity.getComponent<lv::TransformComponent>());

                if (editorEntity.hasComponent<lv::MeshComponent>())
                    runtimeEntity.addComponent<lv::MeshComponent>(editorEntity.getComponent<lv::MeshComponent>());

                if (editorEntity.hasComponent<lv::MaterialComponent>())
                    runtimeEntity.addComponent<lv::MaterialComponent>(editorEntity.getComponent<lv::MaterialComponent>());

                if (editorEntity.hasComponent<lv::ScriptComponent>()) {
                    lv::ScriptComponent& scriptComponent = editorEntity.getComponent<lv::ScriptComponent>();
                    if (scriptComponent.isValid()) {
                        time_t lastModified = 0;
                        struct stat result;
                        if (stat(scriptComponent.script->filename.c_str(), &result) == 0) {
                            lastModified = result.st_mtime;
                        }
                        if (scriptComponent.script->lastModified != lastModified) {
                            scriptComponent.script->refresh();
                            scriptComponent.script->lastModified = lastModified;
                        }
                    }
                    lv::ScriptComponent& runtimeScriptComponent = runtimeEntity.addComponent<lv::ScriptComponent>(editorEntity.getComponent<lv::ScriptComponent>());
                    if (runtimeScriptComponent.isValid())
                        runtimeScriptComponent.entity = runtimeScriptComponent.script->entityConstructor(runtimeEntity.ID, &runtimeRegistry);
                        runtimeScriptComponent.entity->window = scriptComponent.entity->window;
                }

                if (editorEntity.hasComponent<lv::CameraComponent>())
                    runtimeEntity.addComponent<lv::CameraComponent>(editorEntity.getComponent<lv::CameraComponent>());

                if (editorEntity.hasComponent<lv::RigidBodyComponent>()) {
                    runtimeEntity.addComponent<lv::RigidBodyComponent>(editorEntity.getComponent<lv::RigidBodyComponent>());
                }

                lv::ColliderComponent* colliderComponent = nullptr;
                if (editorEntity.hasComponent<lv::SphereColliderComponent>()) {
                    colliderComponent = &runtimeEntity.addComponent<lv::SphereColliderComponent>(editorEntity.getComponent<lv::SphereColliderComponent>());
                }
                if (editorEntity.hasComponent<lv::BoxColliderComponent>()) {
                    colliderComponent = &runtimeEntity.addComponent<lv::BoxColliderComponent>(editorEntity.getComponent<lv::BoxColliderComponent>());
                }

                if (runtimeEntity.hasComponent<lv::TransformComponent>()) {
                    if (colliderComponent != nullptr) {
                        colliderComponent->init();
                        lv::TransformComponent& transformComponent = runtimeEntity.getComponent<lv::TransformComponent>();
                        if (runtimeEntity.hasComponent<lv::RigidBodyComponent>())
                            runtimeEntity.getComponent<lv::RigidBodyComponent>().init(*colliderComponent, transformComponent.position, transformComponent.rotation);
                    }
                }
            }

            if (g_editor->activeViewport == VIEWPORT_TYPE_GAME) {
                primaryCamera = &runtimeRegistry.get<lv::CameraComponent>(primaryCameraEntity);
                camera = primaryCamera;
            }

            registry = &runtimeRegistry;

            start();
        }
    }
}

void Scene::start() {
    if (state == SCENE_STATE_RUNTIME) {
        auto view = runtimeRegistry.view<lv::ScriptComponent>();
        for (auto& entity : view) {
            lv::ScriptComponent& scriptComponent = runtimeRegistry.get<lv::ScriptComponent>(entity);

            if (scriptComponent.isValid()) {
                scriptComponent.entity->start();
            }
        }
    }
}

void Scene::update(float dt) {
    if (state == SCENE_STATE_RUNTIME) {
        auto view = runtimeRegistry.view<lv::ScriptComponent>();
        for (auto& entity : view) {
            lv::ScriptComponent& scriptComponent = runtimeRegistry.get<lv::ScriptComponent>(entity);

            if (scriptComponent.isValid())
                scriptComponent.entity->update(dt);
        }

        auto view2 = runtimeRegistry.view<lv::RigidBodyComponent, lv::TransformComponent>();
        for (auto& entity : view2) {
            lv::RigidBodyComponent& rigidBodyComponent = runtimeRegistry.get<lv::RigidBodyComponent>(entity);
            //lv::Entity entityClass(entity, registry)
            //lv::ColliderComponent* colliderComponent = getEntityColliderComponent(entityClass);
            if (runtimeRegistry.any_of<lv::SphereColliderComponent, lv::BoxColliderComponent>(entity)) {
                lv::TransformComponent& transformComponent = runtimeRegistry.get<lv::TransformComponent>(entity);

                btTransform transform;
                rigidBodyComponent.setMass();
                rigidBodyComponent.rigidBody->setRestitution(rigidBodyComponent.restitution);
                rigidBodyComponent.rigidBody->setFriction(rigidBodyComponent.friction);
                rigidBodyComponent.motionState->getWorldTransform(transform);
                //btTransform localTransform = rigidBodyComponent.rigidBody->getCenterOfMassTransform();
                glm::vec3 globalPos = transformComponent.position;// + rigidBodyComponent.origin;
                //btVector3 vec = localTransform.getOrigin();
                //std::cout << vec.x() << ", " << vec.y() << ", " << vec.z() << std::endl;
                //localTransform.setOrigin(btVector3(localPos.x, localPos.y, localPos.z));
                //rigidBodyComponent.rigidBody->setCenterOfMassTransform(localTransform);
                transform.setOrigin(btVector3(globalPos.x, globalPos.y, globalPos.z));
                //rigidBodyComponent.setOrigin(transformComponent.position);
                rigidBodyComponent.setRotation(transformComponent.rotation);
                if (runtimeRegistry.all_of<lv::BoxColliderComponent>(entity)) {
                    lv::BoxColliderComponent& boxColliderComponent = runtimeRegistry.get<lv::BoxColliderComponent>(entity);
                    boxColliderComponent.shape->setLocalScaling(btVector3(boxColliderComponent.scale.x, boxColliderComponent.scale.y, boxColliderComponent.scale.z));
                }
                //rigidBodyComponent.motionState->setWorldTransform(transform);
            }
        }

        //TODO: add option to change how many steps should be made
        //std::thread* thread = new std::thread([&](){
            physicsWorld.world->stepSimulation(dt, 10);
        //});

        for (auto& entity : view2) {
            lv::RigidBodyComponent& rigidBodyComponent = runtimeRegistry.get<lv::RigidBodyComponent>(entity);
            if (runtimeRegistry.any_of<lv::SphereColliderComponent, lv::BoxColliderComponent>(entity)) {
                lv::TransformComponent& transformComponent = runtimeRegistry.get<lv::TransformComponent>(entity);

                btTransform transform;
                rigidBodyComponent.motionState->getWorldTransform(transform);
                btVector3& position = transform.getOrigin();
                //rigidBodyComponent.setOrigin(glm::vec3(0.0f));
                transformComponent.position = glm::vec3(position.x(), position.y(), position.z());// - rigidBodyComponent.origin;
                //rigidBodyComponent.setRotation(glm::vec3(0.0f));
                rigidBodyComponent.getRotation(transformComponent.rotation);
            }
        }
    }
}

//New entity functions
void Scene::newEntityWithModel(const char* filename) {
    lv::Entity entity(addEntity(), registry);
    entity.getComponent<lv::TagComponent>().name = std::filesystem::path(filename).stem().string();

    entity.addComponent<lv::TransformComponent>();
    
    lv::MeshLoader meshLoader(g_game->deferredLayout);
    meshLoader.loadFromFile(filename);
    for (unsigned int i = 0; i < meshLoader.meshes.size(); i++) {
        lv::Entity entity2(addEntity(), registry);
        entity2.addComponent<lv::TransformComponent>();
        entity2.addComponent<lv::MeshComponent>(meshLoader.meshes[i]);
        entity2.addComponent<lv::MaterialComponent>(g_game->deferredLayout);

        entity.getComponent<lv::NodeComponent>().childs.push_back(entity2.ID);
        entity2.getComponent<lv::NodeComponent>().parent = entity.ID;
    }
}
