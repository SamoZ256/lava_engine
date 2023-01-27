#include "editor.hpp"

#include "lvutils/math/math.hpp"

#include "lvutils/scripting/script_manager.hpp"

//#include "lvutils/physics/rigid_body.hpp"

Editor* g_editor = nullptr;

Editor::Editor(LvndWindow* aWindow
#ifdef LV_BACKEND_VULKAN
, lv::PipelineLayout& aDeferredLayout
#endif
) : window(aWindow)
#ifdef LV_BACKEND_VULKAN
, deferredLayout(aDeferredLayout)
#endif
{
#ifdef LV_BACKEND_VULKAN
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1024 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1024 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1024 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1024 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1024 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1024 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1024 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1024 }
    };

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1024;
    poolInfo.poolSizeCount = std::size(poolSizes);
    poolInfo.pPoolSizes = poolSizes;

    VK_CHECK_RESULT(vkCreateDescriptorPool(lv::g_device->device(), &poolInfo, nullptr, &imguiPool))
#endif

    // 2: initialize imgui library

    //this initializes the core structures of imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    io = &ImGui::GetIO(); (void)io;
    io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    //io->DisplaySize = {1920, 1080};
    //io->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    //Font
    io->FontDefault = io->Fonts->AddFontFromFileTTF("assets/fonts/open_sans/OpenSans-Regular.ttf", 18.0f);

    //Style
    ImGui::StyleColorsDark();

    //Colors
    setupTheme();

#ifdef LV_BACKEND_VULKAN
    ImGui_ImplLvnd_InitForVulkan(window, true);
#elif defined(LV_BACKEND_METAL)
    ImGui_ImplLvnd_InitForOther(window, true);
#endif

#ifdef LV_BACKEND_VULKAN
    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = lv::g_instance->instance;
    init_info.PhysicalDevice = lv::g_device->physicalDevice;
    init_info.Device = lv::g_device->device();
    init_info.Queue = lv::g_device->graphicsQueue();
    init_info.QueueFamily = lv::g_device->findQueueFamilies(lv::g_device->physicalDevice).graphicsFamily;
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = lv::g_swapChain->imageCount();
    init_info.ImageCount = lv::g_swapChain->imageCount();
    //init_info.Allocator = g_allocator.allocator;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info, lv::g_swapChain->renderPass.renderPass);
#elif defined(LV_BACKEND_METAL)
    ImGui_ImplMetal_Init(lv::g_device->device);
#endif

#ifdef LV_BACKEND_VULKAN
    //execute a gpu command to upload imgui font textures
    VkCommandBuffer commandBuffer = lv::g_device->beginSingleTimeCommands();
    ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
    lv::g_device->endSingleTimeCommands(commandBuffer);
    
    //clear font textures from cpu data
    ImGui_ImplVulkan_DestroyFontUploadObjects();
#elif defined LV_BACKEND_METAL
    ImGui_ImplMetal_CreateFontsTexture(lv::g_device->device);
#endif

    //Textures and descriptor sets
    playButtonTex.load("assets/textures/play_button.png");
    playButtonTex.init();
    stopButtonTex.load("assets/textures/stop_button.png");
    stopButtonTex.init();
    folderTex.load("assets/textures/folder.png");
    folderTex.init();
    fileTex.load("assets/textures/file.png");
    fileTex.init();

#ifdef LV_BACKEND_VULKAN
    playButtonSet = createDescriptorSet(playButtonTex.imageView.imageViews[0], playButtonTex.sampler.sampler);
    stopButtonSet = createDescriptorSet(stopButtonTex.imageView.imageViews[0], stopButtonTex.sampler.sampler);
    folderSet = createDescriptorSet(folderTex.imageView.imageViews[0], folderTex.sampler.sampler);
    fileSet = createDescriptorSet(fileTex.imageView.imageViews[0], fileTex.sampler.sampler);
#elif defined(LV_BACKEND_METAL)
    playButtonSet = playButtonTex.image.images[0];
    stopButtonSet = stopButtonTex.image.images[0];
    folderSet = folderTex.image.images[0];
    fileSet = fileTex.image.images[0];
#endif

    lvndGetWindowSize(window, &viewportWidth, &viewportHeight);

    currentBrowseDir = g_game->assetPath;

    g_editor = this;
}

void Editor::resize() {
#ifdef LV_BACKEND_VULKAN
  	ImGui_ImplVulkan_SetMinImageCount(lv::g_swapChain->imageCount());
#endif
}

void Editor::newFrame() {
#ifdef LV_BACKEND_VULKAN
	ImGui_ImplVulkan_NewFrame();
#elif defined LV_BACKEND_METAL
    ImGui_ImplMetal_NewFrame(lv::g_swapChain->activeRenderPasses[fmin(lv::g_swapChain->crntFrame, lv::g_swapChain->activeRenderPasses.size() - 1)]);
#endif
	ImGui_ImplLvnd_NewFrame();

	ImGui::NewFrame();
	//Scr::g_scriptManager.loadScriptNames();
}

void Editor::render() {
	ImGui::Render();
#ifdef LV_BACKEND_VULKAN
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), lv::g_swapChain->getActiveCommandBuffer());
#elif defined LV_BACKEND_METAL
    ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), lv::g_swapChain->getActiveCommandBuffer(), lv::g_swapChain->activeRenderEncoder);
#endif
    //lv::g_swapChain->activeFramebuffer->encoder->popDebugGroup();
}

void Editor::createViewportSet(
#ifdef LV_BACKEND_VULKAN
        std::vector<VkImageView>& viewportImageViews, VkSampler& viewportSampler
#elif defined LV_BACKEND_METAL
        std::vector<MTL::Texture*>& viewportImages
#endif
) {
#ifdef LV_BACKEND_VULKAN
    viewportSets.resize(lv::g_swapChain->maxFramesInFlight);
  	for (uint8_t i = 0; i < lv::g_swapChain->maxFramesInFlight; i++)
		viewportSets[i] = createDescriptorSet(viewportImageViews[i], viewportSampler);
#elif defined LV_BACKEND_METAL
    viewportSets = viewportImages;
#endif
}

#ifdef LV_BACKEND_VULKAN
void Editor::destroyViewportSet() {
  	vkFreeDescriptorSets(lv::g_device->device(), imguiPool, static_cast<uint32_t>(viewportSets.size()), viewportSets.data());
}
#endif

void Editor::destroy() {
#ifdef LV_BACKEND_VULKAN
  	vkDestroyDescriptorPool(lv::g_device->device(), imguiPool, nullptr);
	ImGui_ImplVulkan_Shutdown();
#elif defined LV_BACKEND_METAL
    ImGui_ImplMetal_Shutdown();
#endif
    ImGui_ImplLvnd_Shutdown();
    ImGui::DestroyContext();
}

void Editor::drawEntity(entt::entity entityI) {
    lv::Entity entity(entityI, g_game->scene().registry);

    ImGuiTreeNodeFlags flags = (selectedEntityID == entityI ? ImGuiTreeNodeFlags_Selected : 0);
    bool opened = treeNode((uint16_t)entityI, entity.getComponent<lv::TagComponent>().name.c_str());
    //bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entityI, flags, "%s", entity.getComponent<TagComponent>().name.c_str());
    if (ImGui::IsItemClicked()) {
        selectedEntityID = entityI;
    }
    
    //std::cout << "Now in " << (unsigned int)entityI << std::endl;
    if (ImGui::BeginDragDropTarget()) {
        //std::cout << (unsigned int)entityI << " began drag drop payload" << std::endl;
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(NODE_DRAG_DROP_PAYLOAD_ID)) {
            //std::cout << (unsigned int)entityI << " accepted drag drop payload" << std::endl << std::endl;
            entt::entity dragEntityID = *(entt::entity*)(payload->Data);
            if (entityI != dragEntityID) {
                lv::Entity draggedEntity(dragEntityID, g_game->scene().registry);
                lv::NodeComponent& dragNodeComponent = draggedEntity.getComponent<lv::NodeComponent>();
                //std::cout << (int)entityID << std::endl;
                //std::cout << g_game->scene().registry.get<lv::TagComponent>(entityID).name << std::endl;

                //std::cout << "[END] Accept entity: " << (unsigned int)entityI << " : Drag entity: " << (unsigned int)dragEntityID << std::endl;
                
                //Check if the dragged entity is not parent or parent of parents and so on of the new parent
                bool shouldChangeParent = true;
                entt::entity checkEntity = entityI;
                while (checkEntity != lv::Entity::nullEntity) {
                    if (dragEntityID == checkEntity) {
                        shouldChangeParent = false;
                        break;
                    }
                    lv::NodeComponent& checkNodeComponent = g_game->scene().registry->get<lv::NodeComponent>(checkEntity);
                    checkEntity = checkNodeComponent.parent;
                }

                if (shouldChangeParent) {
                    //std::cout << "Changing parent " << rand() << std::endl;

                    //Remove the dragged entity from the childs of the old parent
                    if (dragNodeComponent.parent != lv::Entity::nullEntity) {
                        std::vector<entt::entity>& childs = g_game->scene().registry->get<lv::NodeComponent>(dragNodeComponent.parent).childs;
                        for (uint32_t i = 0; i < childs.size(); i++) {
                            if (dragEntityID == childs[i]) {
                                childs.erase(childs.begin() + i);
                                break;
                            }
                        }
                    }

                    entity.getComponent<lv::NodeComponent>().childs.push_back(dragEntityID);
                    dragNodeComponent.parent = entityI;
                }
                //std::cout << "Reordered" << std::endl;
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (ImGui::BeginDragDropSource()) {
        //std::cout << "[BEGIN] Drag entity: " << (unsigned int)entityI << std::endl;
        ImGui::SetDragDropPayload(NODE_DRAG_DROP_PAYLOAD_ID, &entityI, sizeof(entt::entity));
        ImGui::EndDragDropSource();
    }

    if (opened) {
        /*
        if (entity.hasComponent<lv::TransformComponent>())
            ImGui::Text("%s", TRANSFORM_COMPONENT_NAME);
        if (entity.hasComponent<lv::ModelComponent>())
            ImGui::Text("%s", MODEL_COMPONENT_NAME);
        if (entity.hasComponent<lv::MaterialComponent>())
            ImGui::Text("%s", MATERIAL_COMPONENT_NAME);
        */
        /*
        if (entity.hasComponent<PointLightComponent>())
            ImGui::Text("%s", components[3].c_str());
        */
        lv::NodeComponent& nodeComponent = entity.getComponent<lv::NodeComponent>();
        for (auto& entityJ : nodeComponent.childs) {
            drawEntity(entityJ);
        }

        ImGui::TreePop();
    }
}

//ImGui functions
void Editor::beginDockspace() {
	int windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

    uint16_t width, height;
    lvndGetWindowSize(window, &width, &height);

	ImVec2 windowSize{(float)width, (float)height};
	ImVec2 windowPos{0.0f, 0.0f};

	ImGui::SetNextWindowPos(windowPos);
	ImGui::SetNextWindowSize(windowSize);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	ImGui::Begin("Dockspace", new bool(true), windowFlags);
	ImGui::PopStyleVar(2);

	// Dockspace
	ImGui::DockSpace(ImGui::GetID("Dockspace"), ImVec2(0, 0));
}

void Editor::endDockspace() {
  	ImGui::End();
}

void Editor::createViewport(const char* title, ViewportType viewportType) {
    lv::Entity selectedEntity((entt::entity)selectedEntityID, g_game->scene().registry);

    uint16_t width, height;
    lvndGetWindowSize(window, &width, &height);

    ImGui::Begin(title, new bool(true), ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (ImGui::IsWindowFocused()) {
        if (activeViewport != viewportType) {
            activeViewport = viewportType;

            if (viewportType == VIEWPORT_TYPE_SCENE) {
                g_game->scene().camera = &g_game->scene().editorCamera;
            } else if (viewportType == VIEWPORT_TYPE_GAME) {
                g_game->scene().primaryCamera = &g_game->scene().registry->get<lv::CameraComponent>(g_game->scene().primaryCameraEntity);
                g_game->scene().camera = g_game->scene().primaryCamera;
            }
        }
    }
    if (activeViewport == viewportType) {
        ImVec2 minBorder = ImGui::GetWindowContentRegionMin();
        //std::cout << "Min: " << minBorder.x << ", " << minBorder.y << std::endl;
        ImVec2 viewportSize = ImGui::GetWindowContentRegionMax();
        //std::cout << "Max: " << viewportSize.x << ", " << viewportSize.y << std::endl;
        viewportSize.x -= minBorder.x;
        viewportSize.y -= minBorder.y;
        ImVec2 viewportPos = ImGui::GetWindowPos();
        //std::cout << "Pos: " << viewportPos.x << ", " << viewportPos.y << std::endl;
        viewportPos.x += minBorder.x;
        viewportPos.y += minBorder.y;
        viewportX = viewportPos.x;
        viewportY = height - viewportPos.y - viewportSize.y;
        if ((viewportSize.x != viewportWidth || viewportSize.y != viewportHeight) && lvndGetMouseButtonState(window, LVND_MOUSE_BUTTON_LEFT) == LVND_STATE_RELEASED) {
            viewportWidth = viewportSize.x;
            viewportHeight = viewportSize.y;
            //std::cout << (int)viewportWidth << ", " << (int)viewportHeight << " : " << camera.aspectRatio << std::endl;
            //std::cout << "Resized" << std::endl;
        }
        ImGui::Image((ImTextureID)viewportSets[lv::g_swapChain->crntFrame], viewportSize);

        //Drag drop
        if (ImGui::BeginDragDropTarget()) {
            const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ASSET_DRAG_DROP_PAYLOAD_ID);
            if (payload) {
                std::string itemPath((const char*)payload->Data);
                std::cout << itemPath << std::endl;

                std::string extension = std::filesystem::path(itemPath).extension();
                std::cout << extension << std::endl;

                /*
                if (extension == ".json") {
                    App::g_application.addScene();
                    App::g_application.scenes[App::g_application.scenes.size() - 1].filename = itemPath;
                    App::g_application.changeScene(App::g_application.scenes.size() - 1);
                }
                */
                if (extension == ".obj" || extension == ".gltf" || extension == ".fbx" || extension == ".blend") {
                    g_game->scene().newEntityWithModel(itemPath.c_str());
                }
            }

            ImGui::EndDragDropTarget();
        }

        //Gizmos
        if (viewportActive) {
            //Getting gizmos type
            if (lvndGetKeyState(window, LVND_KEY_W) == LVND_STATE_PRESSED)
                gizmosType = ImGuizmo::OPERATION::TRANSLATE;
            else if (lvndGetKeyState(window, LVND_KEY_E) == LVND_STATE_PRESSED)
                gizmosType = ImGuizmo::OPERATION::ROTATE;
            else if (lvndGetKeyState(window, LVND_KEY_R) == LVND_STATE_PRESSED)
                gizmosType = ImGuizmo::OPERATION::SCALE;
        }

        //Drawing gizmos
        if (g_game->scene().state == SCENE_STATE_EDITOR) {
            if (selectedEntity.isValid()) {
                if (selectedEntity.hasComponent<lv::TransformComponent>()) {
                    ImGuizmo::SetOrthographic(false);
                    ImGuizmo::SetDrawlist();
                    ImGuizmo::SetRect(viewportPos.x, viewportPos.y, viewportSize.x, viewportSize.y);

                    auto& transformComponent = selectedEntity.getComponent<lv::TransformComponent>();

                    ImGuizmo::Manipulate(glm::value_ptr(g_game->scene().camera->view), glm::value_ptr(g_game->scene().camera->projection), gizmosType, ImGuizmo::LOCAL, glm::value_ptr(transformComponent.model.model));

                    if (ImGui::IsWindowFocused() && ImGuizmo::IsUsing()) {
                        glm::mat4 localModel = transformComponent.model.model;
                        entt::entity parent = selectedEntity.getComponent<lv::NodeComponent>().parent;
                        if (parent != lv::Entity::nullEntity) {
                            if (g_game->scene().registry->all_of<lv::TransformComponent>(parent))
                                localModel = glm::inverse(g_game->scene().registry->get<lv::TransformComponent>(parent).model.model) * localModel;
                        }
                        decomposeModel(localModel, transformComponent.position, transformComponent.rotation, transformComponent.scale);
                    }
                }
            }
        }

        //Play / pause button
        createToolBar();

        viewportActive = (ImGui::IsWindowFocused() && !ImGuizmo::IsUsing());
    }

    ImGui::End();
}

void Editor::createSceneViewport() {
    createViewport("Scene viewport", VIEWPORT_TYPE_SCENE);
}

void Editor::createGameViewport() {
    createViewport("Game viewport", VIEWPORT_TYPE_GAME);
}

void Editor::createToolBar() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

    //ImGui::Begin("##toolbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    float size = 32.0f;//ImGui::GetWindowHeight() - 4.0f;
    ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x * 0.5f - (size * 0.5f));
    ImGui::SetCursorPosY(size + 12.0f);
    auto& buttonSet = g_game->scene().state == SCENE_STATE_EDITOR ? playButtonSet : stopButtonSet;
    if (ImGui::ImageButton((ImTextureID)buttonSet, ImVec2(size, size)/*, ImVec2(0, 0), ImVec2(1, 1), 0*/)) {
        SceneState newState = (g_game->scene().state == SCENE_STATE_EDITOR ? SCENE_STATE_RUNTIME : SCENE_STATE_EDITOR);
        //App::g_application.scene().running = !App::g_application.scene().running;
        g_game->scene().changeState(newState);
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void Editor::createSceneHierarchy() {
    ImGui::Begin("Scene hierarchy");

    //Scene hierarchy
    auto view = g_game->scene().registry->view<lv::TagComponent>();
    for (auto& entityI : view) {
        if (g_game->scene().registry->get<lv::NodeComponent>(entityI).parent == lv::Entity::nullEntity) {
            //std::cout << (unsigned int)g_game->scene().registry.get<lv::NodeComponent>(entityI).parent << " : " << (unsigned int)Entity::nullEntity << std::endl;
            drawEntity(entityI);
        }
    }
    ImGui::End();
}

void Editor::createPropertiesPanel() {
    lv::Entity selectedEntity(selectedEntityID, g_game->scene().registry);

    ImGui::Begin("Entity");

    if (selectedEntity.isValid()) {
        lv::TagComponent& tagComponent = selectedEntity.getComponent<lv::TagComponent>();
        lv::NodeComponent& nodeComponent = selectedEntity.getComponent<lv::NodeComponent>();
        std::string buffer = std::string(tagComponent.name);
        if (ImGui::InputText("Tag", &buffer))
            tagComponent.name = buffer;

        //std::cout << entity.name << " : "<< buffer << std::endl;
        //Transform
        if (selectedEntity.hasComponent<lv::TransformComponent>()) {
            bool opened = treeNode(0, TRANSFORM_COMPONENT_NAME);
            if (opened) {
                auto& transformComponent = selectedEntity.getComponent<lv::TransformComponent>();

                ImGui::DragFloat3("Position", &transformComponent.position[0], 0.1f);
                ImGui::SliderFloat3("Rotation", &transformComponent.rotation[0], 0.0f, 360.0f);
                ImGui::DragFloat3("Size", &transformComponent.scale[0], 0.01f, 0.0f);

                if (ImGui::Button("Remove component")) {
                    selectedEntity.removeComponent<lv::TransformComponent>();
                }
                
                ImGui::TreePop();
            }
        }
        //Mesh
        if (selectedEntity.hasComponent<lv::MeshComponent>()) {
            bool opened = treeNode(1, MESH_COMPONENT_NAME);
            if (opened) {
                auto& meshComponent = selectedEntity.getComponent<lv::MeshComponent>();

                for (uint8_t i = 0; i < LV_MESH_TEXTURE_COUNT; i++) {
                    ImGui::Button("O");
                    if (ImGui::BeginDragDropTarget()) {
                        const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ASSET_DRAG_DROP_PAYLOAD_ID);
                        if (payload) {
                            std::string itemPath((const char*)payload->Data);
                            std::cout << itemPath << std::endl;

                            std::string extension = std::filesystem::path(itemPath).extension();
                            std::cout << extension << std::endl;

                            /*
                            if (extension == ".json") {
                                App::g_application.addScene();
                                App::g_application.scenes[App::g_application.scenes.size() - 1].filename = itemPath;
                                App::g_application.changeScene(App::g_application.scenes.size() - 1);
                            }
                            */
                            if (extension == ".png" || extension == ".jpg" || extension == ".jpeg") {
                                meshComponent.setTexture(lv::MeshComponent::loadTextureFromFile(itemPath.c_str()), i);
#ifdef LV_BACKEND_VULKAN
                                meshComponent.destroyDescriptorSet();
                                meshComponent.initDescriptorSet();
#endif
                            }
                        }

                        ImGui::EndDragDropTarget();
                    }
                }

                if (ImGui::Button("Remove component")) {
                    if (g_game->scene().state == SCENE_STATE_EDITOR) 
                        meshComponent.destroy();
                    selectedEntity.removeComponent<lv::MeshComponent>();
                }

                ImGui::TreePop();
            }
        }
        //Material
        if (selectedEntity.hasComponent<lv::MaterialComponent>()) {
            bool opened = treeNode(2, MATERIAL_COMPONENT_NAME);
            if (opened) {
                auto& materialComponent = selectedEntity.getComponent<lv::MaterialComponent>();

                ImGui::ColorEdit4("Color", &materialComponent.material.albedo[0]);
                ImGui::SliderFloat("Roughness", &materialComponent.material.roughness, 0.0f, 1.0f);
                ImGui::SliderFloat("Metallic", &materialComponent.material.metallic, 0.0f, 1.0f);

                if (ImGui::Button("Remove component")) {
                    if (g_game->scene().state == SCENE_STATE_EDITOR) 
                        materialComponent.destroy();
                    selectedEntity.removeComponent<lv::MaterialComponent>();
                }

                ImGui::TreePop();
            }
        }
        //Script
        if (selectedEntity.hasComponent<lv::ScriptComponent>()) {
            bool opened = treeNode(3, SCRIPT_COMPONENT_NAME);
            if (opened) {
                auto& scriptComponent = selectedEntity.getComponent<lv::ScriptComponent>();

                if (scriptComponent.script == nullptr) {
                    if (ImGui::Button("Load")) {
                        std::string filename = getFileDialogResult("ScriptFileDialog", "C++ files", "cpp");
                        if (filename != "") {
                            scriptComponent.loadScript(filename.c_str(), selectedEntityID, &g_game->scene().runtimeRegistry);
                        }
                    }
                }

                if (ImGui::Button("Remove component")) {
                    if (g_game->scene().state == SCENE_STATE_EDITOR) 
                        scriptComponent.destroy();
                    selectedEntity.removeComponent<lv::ScriptComponent>();
                }
                
                ImGui::TreePop();
            }
        }
        //Camera
        if (selectedEntity.hasComponent<lv::CameraComponent>()) {
            bool opened = treeNode(4, CAMERA_COMPONENT_NAME);
            if (opened) {
                lv::CameraComponent& cameraComponent = selectedEntity.getComponent<lv::CameraComponent>();
                
                ImGui::DragFloat3("Position", &cameraComponent.position[0], 0.1f);
                ImGui::SliderFloat3("Rotation", &cameraComponent.rotation[0], 0.0f, 360.0f);

                ImGui::SliderFloat("Near plane", &cameraComponent.nearPlane, 0.001f, 0.5f);
                ImGui::SliderFloat("Far plane", &cameraComponent.farPlane, 10.0f, 10000.0f);
                ImGui::SliderFloat("Field of view", &cameraComponent.fov, 0.001f, 178.0f);

                if (!cameraComponent.active) {
                    if (ImGui::Button("Set primary")) {
                        g_game->scene().setPrimaryCamera(selectedEntityID, &cameraComponent);
                    }
                }

                if (ImGui::Button("Remove component")) {
                    if (cameraComponent.active) {
                        //TODO: create popup window to warn the user
                    } else {
                        selectedEntity.removeComponent<lv::CameraComponent>();
                    }
                }
                
                ImGui::TreePop();
            }
        }
        //Sphere collider
        if (selectedEntity.hasComponent<lv::SphereColliderComponent>()) {
            bool opened = treeNode(5, SPHERE_COLLIDER_COMPONENT_NAME);
            if (opened) {
                lv::SphereColliderComponent& sphereColliderComponent = selectedEntity.getComponent<lv::SphereColliderComponent>();

                ImGui::DragFloat("Radius", &sphereColliderComponent.radius, 0.001f, 0.0f);

                /*
                if (ImGui::Button("Adjust radius to mesh")) {
                    float radius = 0.0f;
                    glm::vec3 scale(1.0f);
                    for (auto& child : nodeComponent) {
                        lv::Entity childEntity(child, g_game->scene().registry);
                        if (childEntity.hasComponent<lv::TransformComponent>())
                            scale = childEntity.getComponent<lv::TransformComponent>().scale;
                    }

                    sphereColliderComponent.radius = radius * std::max(std::max(scale.x, scale.y), scale.z);
                }
                */

                if (ImGui::Button("Remove component")) {
                    if (g_game->scene().state == SCENE_STATE_RUNTIME)
                        sphereColliderComponent.destroy();
                    selectedEntity.removeComponent<lv::SphereColliderComponent>();
                }
                
                ImGui::TreePop();
            }
        }
        //Box collider
        if (selectedEntity.hasComponent<lv::BoxColliderComponent>()) {
            bool opened = treeNode(6, BOX_COLLIDER_COMPONENT_NAME);
            if (opened) {
                lv::BoxColliderComponent& boxColliderComponent = selectedEntity.getComponent<lv::BoxColliderComponent>();

                ImGui::DragFloat3("Scale", (float*)&boxColliderComponent.scale, 0.001f, 0.0f);

                /*
                if (selectedEntity.hasComponent<lv::MeshComponent>()) {
                    if (ImGui::Button("Adjust scale to mesh")) {
                        lv::MeshComponent& meshComponent = selectedEntity.getComponent<lv::MeshComponent>();
                        glm::vec3 scale(1.0f);
                        if (selectedEntity.hasComponent<lv::TransformComponent>())
                            scale = selectedEntity.getComponent<lv::TransformComponent>().scale;
                        boxColliderComponent.scale = glm::vec3(meshComponent.maxX - meshComponent.minX,
                                                               meshComponent.maxY - meshComponent.minY,
                                                               meshComponent.maxZ - meshComponent.minZ) * scale;
                    }
                }
                */

                if (ImGui::Button("Remove component")) {
                    if (g_game->scene().state == SCENE_STATE_RUNTIME)
                        boxColliderComponent.destroy();
                    selectedEntity.removeComponent<lv::BoxColliderComponent>();
                }
                
                ImGui::TreePop();
            }
        }
        //Rigid body
        if (selectedEntity.hasComponent<lv::RigidBodyComponent>()) {
            bool opened = treeNode(7, RIGID_BODY_COMPONENT_NAME);
            if (opened) {
                lv::RigidBodyComponent& rigidBodyComponent = selectedEntity.getComponent<lv::RigidBodyComponent>();

                ImGui::DragFloat3("Origin", (float*)&rigidBodyComponent.origin, 0.001f, 0.0f);

                ImGui::SliderFloat("Mass", &rigidBodyComponent.mass, 0.0f, 1000.0f);
                if (rigidBodyComponent.rigidBody != nullptr)
                    rigidBodyComponent.setMass();
                //TODO: set other properties

                ImGui::Checkbox("Sync transform", &rigidBodyComponent.syncTransform);

                /*
                if (selectedEntity.hasComponent<lv::MeshComponent>()) {
                    if (ImGui::Button("Adjust origin to mesh")) {
                        lv::MeshComponent& meshComponent = selectedEntity.getComponent<lv::MeshComponent>();
                        rigidBodyComponent.origin = glm::vec3((meshComponent.minX + meshComponent.maxX) * 0.5f,
                                                              (meshComponent.minY + meshComponent.maxY) * 0.5f,
                                                              (meshComponent.minZ + meshComponent.maxZ) * 0.5f);
                    }
                }
                */

                if (ImGui::Button("Remove component")) {
                    //TODO: destroy the rigid body
                    selectedEntity.removeComponent<lv::RigidBodyComponent>();
                }
                
                ImGui::TreePop();
            }
        }

        if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Button("Add Component"))
                ImGui::OpenPopup("Select Component");
            
            if (ImGui::BeginPopup("Select Component")) {
                if (ImGui::MenuItem(TRANSFORM_COMPONENT_NAME)) {
                    selectedEntity.addComponent<lv::TransformComponent>();
                }
                if (ImGui::MenuItem(MESH_COMPONENT_NAME)) {
                    if (!selectedEntity.hasComponent<lv::MeshComponent>()) {
                        selectedEntity.addComponent<lv::MeshComponent>(
#ifdef LV_BACKEND_VULKAN
                            deferredLayout
#endif
                        );
                    }
                }
                if (ImGui::MenuItem(MATERIAL_COMPONENT_NAME)) {
                    if (!selectedEntity.hasComponent<lv::MaterialComponent>()) {
                        selectedEntity.addComponent<lv::MaterialComponent>(
#ifdef LV_BACKEND_VULKAN
                            deferredLayout
#endif
                        );
                    }
                }
                if (ImGui::MenuItem(SCRIPT_COMPONENT_NAME)) {
                    if (!selectedEntity.hasComponent<lv::ScriptComponent>()) {
                        selectedEntity.addComponent<lv::ScriptComponent>();
                    }
                }
                if (ImGui::MenuItem(CAMERA_COMPONENT_NAME)) {
                    if (!selectedEntity.hasComponent<lv::CameraComponent>()) {
                        selectedEntity.addComponent<lv::CameraComponent>();
                    }
                }
                if (ImGui::MenuItem(SPHERE_COLLIDER_COMPONENT_NAME)) {
                    if (!selectedEntity.hasComponent<lv::SphereColliderComponent>()) {
                        selectedEntity.addComponent<lv::SphereColliderComponent>();
                    }
                }
                if (ImGui::MenuItem(BOX_COLLIDER_COMPONENT_NAME)) {
                    if (!selectedEntity.hasComponent<lv::BoxColliderComponent>()) {
                        selectedEntity.addComponent<lv::BoxColliderComponent>();
                    }
                }
                if (ImGui::MenuItem(RIGID_BODY_COMPONENT_NAME)) {
                    if (!selectedEntity.hasComponent<lv::RigidBodyComponent>()) {
                        selectedEntity.addComponent<lv::RigidBodyComponent>(g_game->scene().physicsWorld);
                    }
                }
                ImGui::EndPopup();
            }
        }

        //Option for destroying
        if (ImGui::Button("Destroy")) {
            if (selectedEntity.hasComponent<lv::CameraComponent>() && selectedEntity.getComponent<lv::CameraComponent>().active) {
                //TODO: create popup window to warn the user
            } else {
#ifdef LV_BACKEND_VULKAN
                lv::g_device->waitIdle();
#endif
                if (selectedEntity.hasComponent<lv::MeshComponent>()) selectedEntity.getComponent<lv::MeshComponent>().destroy();
                if (selectedEntity.hasComponent<lv::MaterialComponent>()) selectedEntity.getComponent<lv::MaterialComponent>().destroy();
                selectedEntity.destroy();

                selectedEntityID = lv::Entity::nullEntity;
            }
        }
    }
    ImGui::End();
}

void Editor::createAssetsPanel() {
    ImGui::Begin("Assets panel");

    ImGui::Text("%s", currentBrowseDir.c_str());

    //Back button
    if (currentBrowseDir != g_game->assetPath) {
        ImGui::SameLine();
        if (ImGui::Button("<")) {
            currentBrowseDir = currentBrowseDir.substr(0, currentBrowseDir.find_last_of("/"));
        }
    }

    //Getting the column count
    float cellSize = 64.0f + 8.0f;
    uint8_t columnCount = (uint8_t)(ImGui::GetContentRegionAvail().x / cellSize);
    if (columnCount < 1) columnCount = 1;
    ImGui::Columns(columnCount, 0, false);
    
    //Refresh timer
    //bool refresh = false;

    for (auto& dirEntry : std::filesystem::directory_iterator(currentBrowseDir)) {
        std::string path = dirEntry.path().string();
        std::string filename = dirEntry.path().filename().string();

        bool isDir = dirEntry.is_directory();
        auto& descriptorSet = isDir ? folderSet : fileSet;

        ImGui::PushID(path.c_str());
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

        //Clickable icon
        ImGui::ImageButton((ImTextureID)descriptorSet, ImVec2(64, 64));

        ImGui::PopStyleColor();
        ImGui::PopID();

        if (!isDir) {
            if (ImGui::BeginDragDropSource()) {
                const char* itemPath = path.c_str();
                ImGui::SetDragDropPayload(ASSET_DRAG_DROP_PAYLOAD_ID, itemPath, (strlen(itemPath) + 1) * sizeof(const char*), ImGuiCond_Once);
                ImGui::EndDragDropSource();
            }
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            if (isDir) {
                currentBrowseDir = path;
                //refresh = true;
            }
        }
        ImGui::TextWrapped("%s", filename.c_str());

        ImGui::NextColumn();
    }
    ImGui::Columns(1);

    ImGui::End();
}

//Static functions
void Editor::setupTheme() {
    auto& colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_WindowBg] = ImVec4{ 0.05f, 0.053f, 0.06f, 1.0f };

    // Headers
    colors[ImGuiCol_Header] = ImVec4{ 0.1f, 0.102f, 0.1f, 1.0f };
    colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.15f, 0.152f, 0.16f, 1.0f };
    colors[ImGuiCol_HeaderActive] = ImVec4{ 0.07f, 0.075f, 0.076f, 1.0f };
    
    // Buttons
    colors[ImGuiCol_Button] = ImVec4{ 0.1f, 0.102f, 0.1f, 1.0f };
    colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.15f, 0.152f, 0.16f, 1.0f };
    colors[ImGuiCol_ButtonActive] = ImVec4{ 0.07f, 0.075f, 0.076f, 1.0f };

    // Frame BG
    colors[ImGuiCol_FrameBg] = ImVec4{ 0.1f, 0.102f, 0.1f, 1.0f };
    colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.15f, 0.152f, 0.16f, 1.0f };
    colors[ImGuiCol_FrameBgActive] = ImVec4{ 0.07f, 0.075f, 0.076f, 1.0f };

    // Tabs
    colors[ImGuiCol_Tab] = ImVec4{ 0.07f, 0.075f, 0.076f, 1.0f };
    colors[ImGuiCol_TabHovered] = ImVec4{ 0.19f, 0.191f, 0.1905f, 1.0f };
    colors[ImGuiCol_TabActive] = ImVec4{ 0.14f, 0.141f, 0.1405f, 1.0f };
    colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.07f, 0.075f, 0.076f, 1.0f };
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.1f, 0.102f, 0.1f, 1.0f };

    // Title
    colors[ImGuiCol_TitleBg] = ImVec4{ 0.07f, 0.075f, 0.076f, 1.0f };
    colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.07f, 0.075f, 0.076f, 1.0f };
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.07f, 0.075f, 0.076f, 1.0f };
}

#ifdef LV_BACKEND_VULKAN
VkDescriptorSet Editor::createDescriptorSet(VkImageView& imageView, VkSampler& sampler) {
    return ImGui_ImplVulkan_AddTexture(sampler, imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
#endif

bool Editor::treeNode(uint16_t id, const char* text, ImGuiTreeNodeFlags flags) {
    return ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)id, ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth | flags, "%s", text);
}

std::string Editor::getFileDialogResult(const char* fileDialogName, const char* filterItemName, const char* filterItems) {
    /*
    if (ImGuiFileDialog::Instance()->Display(fileDialogName)) {
        if (ImGuiFileDialog::Instance()->IsOk())
            return ImGuiFileDialog::Instance()->GetFilePathName();

        ImGuiFileDialog::Instance()->Close();
    }
    */
    NFD::UniquePath outPath;

    nfdfilteritem_t nfdFilterItems[1] = {{filterItemName, filterItems}};

    nfdresult_t result = NFD::OpenDialog(outPath, nfdFilterItems, 1);

    if (result == NFD_OKAY) {
        return std::string(outPath.get());
    }

    return "";
}
