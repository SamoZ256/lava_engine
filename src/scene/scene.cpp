#include "scene.hpp"

#include "lvutils/entity/tag.hpp"
#include "lvutils/entity/mesh.hpp"
#include "lvutils/entity/transform.hpp"
#include "lvutils/entity/material.hpp"
#include "lvutils/entity/entity.hpp"
#include "lvutils/scripting/script_manager.hpp"
#include "lvutils/camera/camera.hpp"

#include "../editor/editor.hpp"

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
#ifdef LV_BACKEND_VULKAN
        graphicsPipeline.uploadPushConstants(&transformComponent.model, 0);
#elif defined LV_BACKEND_METAL
        graphicsPipeline.uploadPushConstants(&transformComponent.model, 0, sizeof(lv::PushConstantModel), LV_SHADER_STAGE_VERTEX_BIT);
#endif
        meshComponent.render();
    }
}

void Scene::renderShadows(lv::GraphicsPipeline& graphicsPipeline) {
    auto view = registry->view<lv::MeshComponent, lv::TransformComponent>();
    for (auto &entity : view) {
        auto &meshComponent = registry->get<lv::MeshComponent>(entity);
        auto &transformComponent = registry->get<lv::TransformComponent>(entity);

#ifdef LV_BACKEND_VULKAN
        graphicsPipeline.uploadPushConstants(&transformComponent.model.model, 0);
#elif defined LV_BACKEND_METAL
        graphicsPipeline.uploadPushConstants(&transformComponent.model.model, 1, sizeof(glm::mat4), LV_SHADER_STAGE_VERTEX_BIT);
#endif
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
    //std::cout << "Test 1 passed" << std::endl;
    editorRegistry.get<lv::TagComponent>(entity).name = savedEntity["name"];
    //std::cout << "Test 1.1 passed" << std::endl;
    //std::cout << entityName << std::endl;
    for (auto& element : savedEntity.items()) {
        std::string componentName = element.key();
        //std::cout << "Test 2 passed" << std::endl;
        //Transform component
        if (componentName == TRANSFORM_COMPONENT_NAME) {
            //std::cout << "Test 3 passed" << std::endl;
            auto& transformComponent = editorRegistry.emplace<lv::TransformComponent>(entity);
            auto& component = savedEntity[TRANSFORM_COMPONENT_NAME];
            //std::cout << "Test 4 passed" << std::endl;
            
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
        //Model component
        /*
        if (componentName == MESH_COMPONENT_NAME) {
            auto& modelComponent = registry->emplace<lv::ModelComponent>(entity
#ifdef LV_BACKEND_VULKAN
            , deferredLayout
#endif
            );
            auto& component = JSON["entities"][uuid][MODEL_COMPONENT_NAME];

            modelComponent.modelType = component["modelType"];
            modelComponent.loaded = component["loaded"];
            modelComponent.flipUVs = component["flipUVs"];
            //std::cout << "MODEL_TYPE: " << modelComponent.modelType << std::endl;
            if (modelComponent.modelType == lv::MODEL_TYPE_LOADED) {
                //std::cout << "Normal model" << std::endl;
                if (modelComponent.loaded) {
                    std::string filename = component["filename"];
                    modelComponent.load(filename.c_str());
                    //std::cout << "Filename: " << modelComponent.filename << " : " << filename << std::endl;
                }
            } else if (modelComponent.modelType == lv::MODEL_TYPE_PLANE) {
                modelComponent.createPlane();
            }
        }
        */
        //Mesh component
        if (componentName == MESH_COMPONENT_NAME) {
            lv::MeshComponent& meshComponent = editorRegistry.emplace<lv::MeshComponent>(entity
#ifdef LV_BACKEND_VULKAN
                , deferredLayout
#endif
            );
            auto& component = savedEntity[MESH_COMPONENT_NAME];

            /*
            std::string vertStr = lv::readFile(vertDataFilename.c_str());
            const char* vertChar = vertStr.c_str();

            std::vector<lv::MainVertex> vertices((lv::MainVertex*)vertChar, (lv::MainVertex*)vertChar + strlen(vertChar) * sizeof(char));
            */
            std::string vertDataFilename = component["vertDataFilename"];
            std::ifstream vertFile(vertDataFilename.c_str(), std::ios::in | std::ios::binary);
            size_t vertSize = std::filesystem::file_size(vertDataFilename);
            char* vertData = (char*)malloc(vertSize);
            vertFile.read(vertData, vertSize);
            std::vector<lv::MainVertex> vertices((lv::MainVertex*)vertData, (lv::MainVertex*)vertData + vertSize / sizeof(lv::MainVertex));//((std::istreambuf_iterator<char>(vertFile)), std::istreambuf_iterator<char>());

            std::string indDataFilename = component["indDataFilename"];
            //std::string indStr = lv::readFile(indDataFilename.c_str());
            //const char* indChar = indStr.c_str();
            std::ifstream indFile(indDataFilename.c_str(), std::ios::in | std::ios::binary);
            size_t indSize = std::filesystem::file_size(indDataFilename);
            char* indData = (char*)malloc(indSize);
            indFile.read(indData, indSize);

            std::vector<unsigned int> indices((unsigned int*)indData, (unsigned int*)indData + indSize / sizeof(unsigned int));//((unsigned int*)indChar, (unsigned int*)indChar + strlen(indChar) * sizeof(char));

            /*
            if ((int)entity == 1) {
                std::cout << vertices.size() << " : " << indices.size() << std::endl;
            }
            */

            meshComponent.init(vertices, indices);

            for (uint16_t texIndex = 0; texIndex < 3; texIndex++) {
                std::string texIndexStr = std::to_string(texIndex);
                lv::Texture* texture = nullptr;
                if (component["textures"].contains(texIndexStr)) {
                    std::string filename = component["textures"][texIndexStr]["filename"];
                    //std::cout << filename << std::endl;
                    texture = lv::MeshComponent::loadTextureFromFile(filename.c_str());
                }
                meshComponent.addTexture(texture, texIndex);
            }
#ifdef LV_BACKEND_VULKAN
            meshComponent.descriptorSet->init();
#endif
        }
        //Material component
        if (componentName == MATERIAL_COMPONENT_NAME) {
            auto& materialComponent = editorRegistry.emplace<lv::MaterialComponent>(entity
#ifdef LV_BACKEND_VULKAN
                , deferredLayout
#endif
            );
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

        for (uint16_t i = 0; i < meshComponent.texturesToSave.size(); i++) {
            component["textures"][std::to_string(meshComponent.texturesToSave[i].second)]["filename"] = meshComponent.texturesToSave[i].first->filename;
        }

        //component["vertices"] = meshComponent.vertices;
        /*
        for (unsigned int i = 0; i < meshComponent.vertices.size(); i++) {
            component["vertices"][i][0][0] = meshComponent.vertices[i].position.x;
            component["vertices"][i][0][1] = meshComponent.vertices[i].position.y;
            component["vertices"][i][0][2] = meshComponent.vertices[i].position.z;

            component["vertices"][i][1][0] = meshComponent.vertices[i].texCoord.x;
            component["vertices"][i][1][1] = meshComponent.vertices[i].texCoord.y;

            component["vertices"][i][2][0] = meshComponent.vertices[i].normal.x;
            component["vertices"][i][2][1] = meshComponent.vertices[i].normal.y;
            component["vertices"][i][2][2] = meshComponent.vertices[i].normal.z;

            component["vertices"][i][3][0] = meshComponent.vertices[i].tangent.x;
            component["vertices"][i][3][1] = meshComponent.vertices[i].tangent.y;
            component["vertices"][i][3][2] = meshComponent.vertices[i].tangent.z;
        }
        component["indices"] = meshComponent.indices;
        */
        std::string vertDataFilename = Scene::meshDataDir + "/mesh" + std::to_string(meshDataFilenameIndex) + "_vert";
        component["vertDataFilename"] = vertDataFilename;
        std::ofstream vertDataOut(vertDataFilename.c_str(), std::ios::out | std::ios::binary);
        vertDataOut.write((char*)meshComponent.vertices.data(), meshComponent.vertices.size() * sizeof(lv::MainVertex));
        //vertDataOut << meshComponent.vertices.data() << std::endl;
        //std::cout << vertDataFilename << std::endl;

        std::string indDataFilename = Scene::meshDataDir + "/mesh" + std::to_string(meshDataFilenameIndex) + "_ind";
        component["indDataFilename"] = indDataFilename;
        std::ofstream indDataOut(indDataFilename.c_str(), std::ios::out | std::ios::binary);
        indDataOut.write((char*)meshComponent.indices.data(), meshComponent.indices.size() * sizeof(unsigned int));
        //indDataOut << meshComponent.indices.data() << std::endl;

        /*
        if ((int)entity == 1) {
            std::cout << meshComponent.vertices.size() << " : " << meshComponent.indices.size() << std::endl;
        }
        */

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
            //TODO: destroy components of entities created during the runtime

            auto view = runtimeRegistry.view<lv::TagComponent>();
            for (auto& entity : view) {
                runtimeRegistry.destroy(entity);
            }

            if (g_editor->activeViewport == VIEWPORT_TYPE_GAME) {
                primaryCamera = &editorRegistry.get<lv::CameraComponent>(primaryCameraEntity);
                camera = primaryCamera;
            }

            registry = &editorRegistry;
        } else if (state == SCENE_STATE_RUNTIME) {
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
                    runtimeEntity.addComponent<lv::ScriptComponent>(editorEntity.getComponent<lv::ScriptComponent>());
                }

                if (editorEntity.hasComponent<lv::CameraComponent>())
                    runtimeEntity.addComponent<lv::CameraComponent>(editorEntity.getComponent<lv::CameraComponent>());
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
    }
}
