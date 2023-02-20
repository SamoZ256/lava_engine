#ifndef EDITOR_H
#define EDITOR_H

#include <imgui_impl_lvnd.h>
#ifdef LV_BACKEND_VULKAN
#include <imgui_impl_vulkan.h>
#elif defined LV_BACKEND_METAL
#include <imgui_impl_metal.h>
#endif
//#include <ImGuiFileDialog.h>
#include <imgui_stdlib.h>

#include <ImGuizmo/ImGuizmo.h>

#include "nfd.hpp"

#include "lvutils/entity/tag.hpp"
#include "lvutils/entity/transform.hpp"
#include "lvutils/entity/mesh.hpp"
#include "lvutils/entity/material.hpp"
#include "lvutils/entity/entity.hpp"
#include "lvutils/camera/camera.hpp"

#include "../scene/game.hpp"

#define NODE_DRAG_DROP_PAYLOAD_ID "NODE_DRAG_DROP_PAYLOAD_ID"
#define ASSET_DRAG_DROP_PAYLOAD_ID "ASSET_DRAG_DROP_PAYLOAD_ID"

enum ViewportType {
    VIEWPORT_TYPE_SCENE,
    VIEWPORT_TYPE_GAME
};

class Editor {
public:
#ifdef LV_BACKEND_VULKAN
    VkDescriptorPool imguiPool;
#endif
    ImGuiIO* io;

    LvndWindow* window;

    //Viewport
    uint16_t viewportWidth;
    uint16_t viewportHeight;
    uint16_t viewportX = 0;
    uint16_t viewportY = 0;

    bool viewportActive = false;
    bool cameraActive = false;
    ViewportType activeViewport = VIEWPORT_TYPE_SCENE;

    std::string currentBrowseDir;

    //Entity
    entt::entity selectedEntityID = lv::Entity::nullEntity;

    //Gizmos
    ImGuizmo::OPERATION gizmosType = ImGuizmo::OPERATION::TRANSLATE;

    //Descriptor sets
#ifdef LV_BACKEND_VULKAN
    std::vector<VkDescriptorSet> viewportSets;

    lv::PipelineLayout& deferredLayout;
#elif defined LV_BACKEND_METAL
    std::vector<MTL::Texture*> viewportSets;
#endif

    //NFD
    NFD::Guard nfdGuard;

    //Textures and descriptor sets
    lv::Texture playButtonTex;
    lv::Texture stopButtonTex;
    lv::Texture folderTex;
    lv::Texture fileTex;
    lv::Texture translateButtonTex;
    lv::Texture rotateButtonTex;
    lv::Texture scaleButtonTex;

#ifdef LV_BACKEND_VULKAN
    VkDescriptorSet playButtonSet;
    VkDescriptorSet stopButtonSet;
    VkDescriptorSet folderSet;
    VkDescriptorSet fileSet;
    VkDescriptorSet translateButtonSet;
    VkDescriptorSet rotateButtonSet;
    VkDescriptorSet scaleButtonSet;
#elif defined LV_BACKEND_METAL
    MTL::Texture* playButtonSet;
    MTL::Texture* stopButtonSet;
    MTL::Texture* folderSet;
    MTL::Texture* fileSet;
    MTL::Texture* translateButtonSet;
    MTL::Texture* rotateButtonSet;
    MTL::Texture* scaleButtonSet;
#endif

#ifdef LV_BACKEND_VULKAN
    Editor(LvndWindow* aWindow, lv::PipelineLayout& aDeferredLayout) : window(aWindow), deferredLayout(aDeferredLayout) {}
#elif defined LV_BACKEND_METAL
    Editor(LvndWindow* aWindow) : window(aWindow) {}
#endif

    void init();

    void resize();

    void newFrame();

    void render();

    void createViewportSet(
#ifdef LV_BACKEND_VULKAN
        std::vector<VkImageView>& viewportImageViews, VkSampler& viewportSampler
#elif defined LV_BACKEND_METAL
        std::vector<MTL::Texture*>& viewportImages
#endif
    );

#ifdef LV_BACKEND_VULKAN
    void destroyViewportSet();
#endif

    void destroy();

    void drawEntity(entt::entity entityI);

    void drawGizmoSelection(ImTextureID imageSet, float x, float size, ImGuizmo::OPERATION aGizmosType);

    //ImGui functions
    void beginDockspace();

    void endDockspace();

    void createViewport(const char* title, ViewportType viewportType);

    void createSceneViewport();

    void createGameViewport();

    void createToolBar();

    void createSceneHierarchy();

    void createPropertiesPanel();

    void createAssetsPanel();

    void createGraphicsPanel();

    //Static functions
    static void setupTheme();

#ifdef LV_BACKEND_VULKAN
    static VkDescriptorSet createDescriptorSet(VkImageView& imageView, VkSampler& sampler);
#endif

    static bool treeNode(uint16_t id, const char* text, ImGuiTreeNodeFlags flags = 0);

    static std::string getFileDialogResult(const char* fileDialogName, const char* filterItemName, const char* filterItems);
};

extern Editor* g_editor;

#endif
