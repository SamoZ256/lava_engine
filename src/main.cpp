//#include "PerlinNoise/PerlinNoise.hpp"
#include <random>
#include <array>
#include <iostream>

#include "backend.hpp"

#define LVND_DEBUG
#ifdef LV_BACKEND_VULKAN
#define LVND_BACKEND_VULKAN
#elif defined LV_BACKEND_METAL
#define LVND_BACKEND_METAL
#endif
#include "lvnd/lvnd.h"

#include "lvutils/camera/arcball_camera.hpp"

#include "lvutils/entity/mesh_loader.hpp"
#include "lvutils/entity/transform.hpp"
#include "lvutils/entity/material.hpp"

#include "lvutils/light/direct_light.hpp"

#include "lvutils/skylight/skylight.hpp"

#include "editor/editor.hpp"

//Callbacks
void windowResizeCallback(LvndWindow* window, uint16_t width, uint16_t height);

void scrollCallback(LvndWindow* window, double xoffset, double yoffset);

//Functions

//Shadows
std::vector<glm::vec4> getFrustumCorners(const glm::mat4& proj, const glm::mat4& view);

void getLightMatrix(glm::vec3 lightDir, float nearPlane, float farPlane, bool firstCascade);

void getLightMatrices(glm::vec3 lightDir);

//Menu bar actions
void saveGame();

void newEntityCreateEmpty();

void newEntityLoadModel();

void newEntityPlane();

//Macros

//Shadows
#define SHADOW_MAP_SIZE 2048
#define CASCADE_COUNT 4
#define SHADOW_FAR_PLANE 100.0f
#define SHADOW_FRAME_COUNT 4

//SSAO
#define SSAO_NOISE_SIZE 8

//Window
#define SRC_WIDTH 1920
#define SRC_HEIGHT 1080
#define SRC_TITLE "Lava Engine"

#define SCROLL_SENSITIVITY 0.05f

//Uniform buffer objects
struct UBOMainVP {
    glm::mat4 invViewProj;
    glm::vec3 cameraPos;
};

struct PCShadowVP {
    glm::mat4 viewProj;
    int layerIndex;
};

struct PCSsaoVP {
    glm::mat4 projection;
	glm::mat4 view;
    glm::mat4 invViewProj;
};

//#define SHADER_COUNT 10

/*
enum ShaderType {
    SHADER_TYPE_EQUI_TO_CUBE,
    SHADER_TYPE_IRRADIANCE,
    SHADER_TYPE_PREFILTERED,
    SHADER_TYPE_DEFERRED,
    SHADER_TYPE_SHADOW,
    SHADER_TYPE_SSAO,
    SHADER_TYPE_SSAO_BLUR,
    SHADER_TYPE_SKYLIGHT,
  	SHADER_TYPE_MAIN,
    SHADER_TYPE_HDR
};
*/

//Render passes

//Deferred render pass
struct DeferredRenderPass {
#ifdef LV_BACKEND_VULKAN
    lv::RenderPass renderPass;
#endif
    lv::Framebuffer framebuffer;

    /*
    lv::Image positionDepthAttachment;
    lv::ImageView positionDepthAttachmentView;
    lv::Sampler positionDepthAttachmentSampler;
    */

    lv::Image normalRoughnessAttachment;
#ifdef LV_BACKEND_VULKAN
    lv::ImageView normalRoughnessAttachmentView;
#endif
    lv::Sampler normalRoughnessAttachmentSampler;

    lv::Image albedoMetallicAttachment;
#ifdef LV_BACKEND_VULKAN
    lv::ImageView albedoMetallicAttachmentView;
#endif
    lv::Sampler albedoMetallicAttachmentSampler;

    lv::Image depthAttachment;
#ifdef LV_BACKEND_VULKAN
    lv::ImageView depthAttachmentView;
#endif
    lv::Sampler depthAttachmentSampler;

    //lv::DescriptorSet descriptorSet = lv::DescriptorSet(SHADER_TYPE_MAIN, 0);
};

#define SETUP_MAIN_0_DESCRIPTORS \
mainDescriptorSet0.addBufferBinding(directLight.lightUniformBuffer.descriptorInfo(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER); \
mainDescriptorSet0.addBufferBinding(mainVPUniformBuffer.descriptorInfo(), 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER); \
mainDescriptorSet0.addBufferBinding(mainShadowUniformBuffer.descriptorInfo(), 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER); \
mainDescriptorSet0.addImageBinding(shadowRenderPass.depthAttachmentSampler.descriptorInfo(shadowRenderPass.depthAttachmentView), 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); \

#define SETUP_MAIN_1_DESCRIPTORS \
mainDescriptorSet1.addImageBinding(skylight.irradianceMapSampler.descriptorInfo(skylight.irradianceMapImageView), 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); \
mainDescriptorSet1.addImageBinding(skylight.prefilteredMapSampler.descriptorInfo(skylight.prefilteredMapImageView), 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); \
mainDescriptorSet1.addImageBinding(brdfLutTexture.sampler.descriptorInfo(brdfLutTexture.imageView), 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

#define SETUP_MAIN_2_DESCRIPTORS \
mainDescriptorSet2.addImageBinding(deferredRenderPass.depthAttachmentSampler.descriptorInfo(deferredRenderPass.depthAttachmentView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL), 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); \
mainDescriptorSet2.addImageBinding(deferredRenderPass.normalRoughnessAttachmentSampler.descriptorInfo(deferredRenderPass.normalRoughnessAttachmentView), 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); \
mainDescriptorSet2.addImageBinding(deferredRenderPass.albedoMetallicAttachmentSampler.descriptorInfo(deferredRenderPass.albedoMetallicAttachmentView), 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); \
mainDescriptorSet2.addImageBinding(ssaoBlurRenderPass.colorAttachmentSampler.descriptorInfo(ssaoBlurRenderPass.colorAttachmentView), 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

//Shadow render pass
struct ShadowRenderPass {
#ifdef LV_BACKEND_VULKAN
    lv::RenderPass renderPass;
#endif
    lv::Framebuffer framebuffer;//s[CASCADE_COUNT];
    //lv::ImageView depthAttachmentViews[CASCADE_COUNT];

    lv::Image depthAttachment;
#ifdef LV_BACKEND_VULKAN
    lv::ImageView depthAttachmentView;
    //lv::ImageView depthAttachmentViews[CASCADE_COUNT];
#elif defined(LV_BACKEND_METAL)
    //lv::Image depthAttachmentViews[CASCADE_COUNT];
#endif
    lv::Sampler depthAttachmentSampler;
};

//SSAO render pass
struct SSAORenderPass {
#ifdef LV_BACKEND_VULKAN
    lv::RenderPass renderPass;
#endif
    lv::Framebuffer framebuffer;

    lv::Image colorAttachment;
#ifdef LV_BACKEND_VULKAN
    lv::ImageView colorAttachmentView;
#endif
    lv::Sampler colorAttachmentSampler;
};

#define SETUP_SSAO_DESCRIPTORS \
ssaoDescriptorSet.addImageBinding(deferredRenderPass.depthAttachmentSampler.descriptorInfo(deferredRenderPass.depthAttachmentView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL), 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); \
ssaoDescriptorSet.addImageBinding(deferredRenderPass.normalRoughnessAttachmentSampler.descriptorInfo(deferredRenderPass.normalRoughnessAttachmentView), 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); \
ssaoDescriptorSet.addImageBinding(ssaoNoiseTex.sampler.descriptorInfo(ssaoNoiseTex.imageView), 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

//SSAO blur render pass
struct SSAOBlurRenderPass {
#ifdef LV_BACKEND_VULKAN
    lv::RenderPass renderPass;
#endif
    lv::Framebuffer framebuffer;

    lv::Image colorAttachment;
#ifdef LV_BACKEND_VULKAN
    lv::ImageView colorAttachmentView;
#endif
    lv::Sampler colorAttachmentSampler;
};

#define SETUP_SSAO_BLUR_DESCRIPTORS \
ssaoBlurDescriptorSet.addImageBinding(ssaoRenderPass.colorAttachmentSampler.descriptorInfo(ssaoRenderPass.colorAttachmentView), 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);/* \
ssaoBlurDescriptorSet.addImageBinding(deferredRenderPass.positionDepthAttachmentSampler.descriptorInfo(), 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);*/

//Main render pass
struct MainRenderPass {
#ifdef LV_BACKEND_VULKAN
    lv::RenderPass renderPass;
#endif
    lv::Framebuffer framebuffer;

    lv::Image colorAttachment;
#ifdef LV_BACKEND_VULKAN
    lv::ImageView colorAttachmentView;
#endif
    lv::Sampler colorAttachmentSampler;

    //lv::DescriptorSet descriptorSet = lv::DescriptorSet(SHADER_TYPE_HDR, 0);
};

#define SETUP_HDR_DESCRIPTORS \
hdrDescriptorSet.addImageBinding(mainRenderPass.colorAttachmentSampler.descriptorInfo(mainRenderPass.colorAttachmentView), 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);/* \
hdrDescriptorSet.addImageBinding(deferredRenderPass.depthAttachmentSampler.descriptorInfo(), 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER); \
hdrDescriptorSet.addImageBinding(deferredRenderPass.normalRoughnessAttachmentSampler.descriptorInfo(), 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);*/

/*
std::vector<Vertex> vertices {
    {{-0.5f,  0.5f,  0.5f}},
    {{-0.5f, -0.5f,  0.5f}},
    {{ 0.5f, -0.5f,  0.5f}},
    {{ 0.5f,  0.5f,  0.5f}}
};

std::vector<uint32_t> indices {
    0, 1, 2,
    0, 2, 3
};
*/

//Shadows
const float cascadeLevels[CASCADE_COUNT] = {
    SHADOW_FAR_PLANE * 0.08f, SHADOW_FAR_PLANE * 0.22f, SHADOW_FAR_PLANE * 0.5f, SHADOW_FAR_PLANE  
};

std::vector<glm::mat4> shadowVPs;

uint8_t shadowFrameCounter = 0;
const uint8_t shadowRefreshFrames[CASCADE_COUNT] = {
    1, 2, 4, 4
};

const uint8_t shadowStartingFrames[CASCADE_COUNT] = {
    0, 0, 0, 2
};

//Time
float lastTime = 0.0f;

int main() {
    //Menu bar
    LvndMenuBar* menuBar = lvndCreateMenuBar();

    //File menu
    LvndMenu* fileMenu = lvndCreateMenu("File");
    lvndMenuBarAddMenu(menuBar, fileMenu);
    LvndMenuItem* saveMenuItem = lvndCreateMenuItem("Save", saveGame, "s");
    lvndMenuAddMenuItem(fileMenu, saveMenuItem);

    //Entity menu
    LvndMenu* entityMenu = lvndCreateMenu("Entity");
    lvndMenuBarAddMenu(menuBar, entityMenu);

    LvndMenu* newEntityMenu = lvndCreateMenu("New Entity");
    lvndMenuAddMenu(entityMenu, newEntityMenu);
    LvndMenuItem* newEntityCreateEmptyMenuItem = lvndCreateMenuItem("Create empty", newEntityCreateEmpty, "");
    lvndMenuAddMenuItem(newEntityMenu, newEntityCreateEmptyMenuItem);
    lvndMenuAddSeparator(newEntityMenu);
    LvndMenuItem* newEntityLoadModelMenuItem = lvndCreateMenuItem("Load model", newEntityLoadModel, "");
    lvndMenuAddMenuItem(newEntityMenu, newEntityLoadModelMenuItem);
    lvndMenuAddSeparator(newEntityMenu);
    LvndMenuItem* newEntityPlaneMenuItem = lvndCreateMenuItem("Plane", newEntityPlane, "");
    lvndMenuAddMenuItem(newEntityMenu, newEntityPlaneMenuItem);

    /*
    lvndMenuAddSeparator(entityMenu);
    LvndMenuItem* fooMenuItem = lvndCreateMenuItem("Foo", nullptr, "");
    lvndMenuAddMenuItem(entityMenu, fooMenuItem);
    */

    lvndSetGlobalMenuBar(menuBar);

    lvndInit();

    //srand((unsigned)time(0));
    LvndWindow* window = lvndCreateWindow(SRC_WIDTH, SRC_HEIGHT, SRC_TITLE);

    //Callbacks
    lvndSetWindowResizeCallback(window, windowResizeCallback);
    lvndSetScrollCallback(window, scrollCallback);

#ifdef LV_BACKEND_VULKAN
    lv::InstanceCreateInfo instanceCreateInfo;
	instanceCreateInfo.applicationName = "Lava Engine";
	instanceCreateInfo.enableValidationLayers = true;
	lv::Instance instance(instanceCreateInfo);

	lv::DeviceCreateInfo deviceCreateInfo;
	deviceCreateInfo.window = window;
	lv::Device device(deviceCreateInfo);

	lv::AllocatorCreateInfo allocatorCreateInfo;
	lv::Allocator allocator(allocatorCreateInfo);

	lv::SwapChainCreateInfo swapChainCreateInfo;
	swapChainCreateInfo.window = window;
	swapChainCreateInfo.vsyncEnabled = false;
	lv::SwapChain swapChain(swapChainCreateInfo);

	lv::DescriptorPoolCreateInfo descriptorPoolCreateInfo;
	//descriptorManagerCreateInfo.pipelineLayoutCount = SHADER_COUNT;
	descriptorPoolCreateInfo.poolSizes[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER] = 64;
	descriptorPoolCreateInfo.poolSizes[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER] = 1024;
	lv::DescriptorPool descriptorPool(descriptorPoolCreateInfo);
#elif defined LV_BACKEND_METAL
    lv::Device device;

	lv::SwapChain swapChain(window);
#endif
    
    glm::i8vec3 neautralColor(127, 127, 127);
    lv::MeshComponent::neautralTexture.width = 1;
    lv::MeshComponent::neautralTexture.height = 1;
    lv::MeshComponent::neautralTexture.textureData = &neautralColor;
    lv::MeshComponent::neautralTexture.init();

    //lv::g_descriptorManager.shaderLayouts.resize(SHADER_COUNT);

#ifdef LV_BACKEND_VULKAN
    //Equirectangular to cubemap shader
    lv::PipelineLayout equiToCubeLayout;
    equiToCubeLayout.descriptorSetLayouts.resize(1);

	equiToCubeLayout.descriptorSetLayouts[0].addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LV_SHADER_STAGE_VERTEX_BIT | LV_SHADER_STAGE_FRAGMENT_BIT); //View projection
	equiToCubeLayout.descriptorSetLayouts[0].addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Environment map

    //Irradiance shader
    lv::PipelineLayout irradianceLayout;
    irradianceLayout.descriptorSetLayouts.resize(1);

	irradianceLayout.descriptorSetLayouts[0].addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LV_SHADER_STAGE_VERTEX_BIT | LV_SHADER_STAGE_FRAGMENT_BIT); //View projection
	irradianceLayout.descriptorSetLayouts[0].addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Environment map

    //Prefiltered shader
    lv::PipelineLayout prefilterLayout;
    prefilterLayout.descriptorSetLayouts.resize(1);

	prefilterLayout.descriptorSetLayouts[0].addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LV_SHADER_STAGE_VERTEX_BIT | LV_SHADER_STAGE_FRAGMENT_BIT); //View projection
	prefilterLayout.descriptorSetLayouts[0].addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Environment map

    //Deferred shader
    lv::PipelineLayout deferredLayout;
    deferredLayout.descriptorSetLayouts.resize(3);

	deferredLayout.descriptorSetLayouts[0].addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LV_SHADER_STAGE_VERTEX_BIT | LV_SHADER_STAGE_FRAGMENT_BIT); //View projection

	deferredLayout.descriptorSetLayouts[1].addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LV_SHADER_STAGE_FRAGMENT_BIT); //Material

	//descriptorManager.getDescriptorSetLayout(SHADER_TYPE_DEFERRED, 2).addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT); //Available textures
	deferredLayout.descriptorSetLayouts[2].addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Albedo texture
	deferredLayout.descriptorSetLayouts[2].addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Roughness texture
	deferredLayout.descriptorSetLayouts[2].addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Metallic texture
	//deferredLayout.descriptorSetLayouts[2].addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Normal texture

    //Shadow shader
    lv::PipelineLayout shadowLayout;
    shadowLayout.descriptorSetLayouts.resize(1);

	shadowLayout.descriptorSetLayouts[0].addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LV_SHADER_STAGE_VERTEX_BIT); //View projection

    //SSAO shader
    lv::PipelineLayout ssaoLayout;
    ssaoLayout.descriptorSetLayouts.resize(1);

	ssaoLayout.descriptorSetLayouts[0].addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Position depth
	ssaoLayout.descriptorSetLayouts[0].addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Normal roughness
    ssaoLayout.descriptorSetLayouts[0].addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //SSAO noise

    //SSAO blur shader
    lv::PipelineLayout ssaoBlurLayout;
    ssaoBlurLayout.descriptorSetLayouts.resize(1);

	ssaoBlurLayout.descriptorSetLayouts[0].addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //SSAO attachment
	//lv::GET_DESCRIPTOR_SET_LAYOUT(SHADER_TYPE_SSAO_BLUR, 0).addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); //Position depth

    //Skylight shader
    lv::PipelineLayout skylightLayout;
    skylightLayout.descriptorSetLayouts.resize(1);

	skylightLayout.descriptorSetLayouts[0].addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Environment map

    //Main shader
    lv::PipelineLayout mainLayout;
    mainLayout.descriptorSetLayouts.resize(3);

	mainLayout.descriptorSetLayouts[0].addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LV_SHADER_STAGE_FRAGMENT_BIT); //Light
    mainLayout.descriptorSetLayouts[0].addBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LV_SHADER_STAGE_FRAGMENT_BIT); //Inverse view projection
	mainLayout.descriptorSetLayouts[0].addBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LV_SHADER_STAGE_FRAGMENT_BIT); //Shadow
    mainLayout.descriptorSetLayouts[0].addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Shadow map

    mainLayout.descriptorSetLayouts[1].addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Irradiance map
    mainLayout.descriptorSetLayouts[1].addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Prefiltered map
    mainLayout.descriptorSetLayouts[1].addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //BRDF Lut map

    mainLayout.descriptorSetLayouts[2].addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Depth
    mainLayout.descriptorSetLayouts[2].addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Normal roughness
    mainLayout.descriptorSetLayouts[2].addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Albedo metallic
    mainLayout.descriptorSetLayouts[2].addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //SSAO

    //Point light shader
    lv::PipelineLayout pointLightLayout;
    pointLightLayout.descriptorSetLayouts.resize(1);

	pointLightLayout.descriptorSetLayouts[0].addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LV_SHADER_STAGE_VERTEX_BIT); //View projection
    pointLightLayout.descriptorSetLayouts[0].addBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LV_SHADER_STAGE_FRAGMENT_BIT); //Inverse view projection
    pointLightLayout.descriptorSetLayouts[0].addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Depth
    pointLightLayout.descriptorSetLayouts[0].addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Normal roughness
    pointLightLayout.descriptorSetLayouts[0].addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Albedo metallic

    //HDR shader
    lv::PipelineLayout hdrLayout;
    hdrLayout.descriptorSetLayouts.resize(1);

	hdrLayout.descriptorSetLayouts[0].addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Input image
	//lv::GET_DESCRIPTOR_SET_LAYOUT(SHADER_TYPE_HDR, 0).addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); //Depth
	//lv::GET_DESCRIPTOR_SET_LAYOUT(SHADER_TYPE_HDR, 0).addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); //Normal roughness

    //descriptorManager.init();
#endif

    //Render passes

    //Deferred render pass
#pragma region DEFERRED_RENDER_PASS
    DeferredRenderPass deferredRenderPass{};
    /*
    deferredRenderPass.positionDepthAttachment.usage |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    deferredRenderPass.positionDepthAttachment.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    deferredRenderPass.positionDepthAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    deferredRenderPass.positionDepthAttachment.init(window.width, window.height);
    deferredRenderPass.positionDepthAttachmentView.init(&deferredRenderPass.positionDepthAttachment);
    deferredRenderPass.positionDepthAttachmentSampler.init(&deferredRenderPass.positionDepthAttachmentView);
    */

    deferredRenderPass.normalRoughnessAttachment.usage |= LV_IMAGE_USAGE_SAMPLED_BIT | LV_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    deferredRenderPass.normalRoughnessAttachment.format = LV_FORMAT_RGBA16_SNORM;
#ifdef LV_BACKEND_VULKAN
    deferredRenderPass.normalRoughnessAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	deferredRenderPass.normalRoughnessAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
#endif
    deferredRenderPass.normalRoughnessAttachment.init(SRC_WIDTH, SRC_HEIGHT);
#ifdef LV_BACKEND_VULKAN
    deferredRenderPass.normalRoughnessAttachmentView.init(&deferredRenderPass.normalRoughnessAttachment);
#endif
    deferredRenderPass.normalRoughnessAttachmentSampler.init();

    deferredRenderPass.albedoMetallicAttachment.usage |= LV_IMAGE_USAGE_SAMPLED_BIT | LV_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    deferredRenderPass.albedoMetallicAttachment.format = LV_FORMAT_RGBA16_UNORM;
#ifdef LV_BACKEND_VULKAN
    deferredRenderPass.albedoMetallicAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	deferredRenderPass.albedoMetallicAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
#endif
    deferredRenderPass.albedoMetallicAttachment.init(SRC_WIDTH, SRC_HEIGHT);
#ifdef LV_BACKEND_VULKAN
    deferredRenderPass.albedoMetallicAttachmentView.init(&deferredRenderPass.albedoMetallicAttachment);
#endif
    deferredRenderPass.albedoMetallicAttachmentSampler.init();

    deferredRenderPass.depthAttachment.usage |= LV_IMAGE_USAGE_SAMPLED_BIT | LV_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    deferredRenderPass.depthAttachment.format = swapChain.depthFormat;
    deferredRenderPass.depthAttachment.loadOp = LV_ATTACHMENT_LOAD_OP_CLEAR;
#ifdef LV_BACKEND_VULKAN
    deferredRenderPass.depthAttachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	deferredRenderPass.depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
#endif
    deferredRenderPass.depthAttachment.init(SRC_WIDTH, SRC_HEIGHT);
#ifdef LV_BACKEND_VULKAN
    deferredRenderPass.depthAttachmentView.init(&deferredRenderPass.depthAttachment);
#endif
    deferredRenderPass.depthAttachmentSampler.init();

    //deferredRenderPass.framebuffer.addColorAttachment({&deferredRenderPass.positionDepthAttachment, &deferredRenderPass.positionDepthAttachmentView, 0});
    deferredRenderPass.framebuffer.addColorAttachment({&deferredRenderPass.normalRoughnessAttachment,
#ifdef LV_BACKEND_VULKAN
    &deferredRenderPass.normalRoughnessAttachmentView,
#endif
    0});
    deferredRenderPass.framebuffer.addColorAttachment({&deferredRenderPass.albedoMetallicAttachment,
#ifdef LV_BACKEND_VULKAN
    &deferredRenderPass.albedoMetallicAttachmentView,
#endif
    1});
    deferredRenderPass.framebuffer.setDepthAttachment({&deferredRenderPass.depthAttachment,
#ifdef LV_BACKEND_VULKAN
    &deferredRenderPass.depthAttachmentView,
#endif
    2});

#ifdef LV_BACKEND_VULKAN
    deferredRenderPass.renderPass.init(deferredRenderPass.framebuffer.getAttachmentDescriptions());
#endif

    deferredRenderPass.framebuffer.init(
#ifdef LV_BACKEND_VULKAN
        &deferredRenderPass.renderPass, SRC_WIDTH, SRC_HEIGHT
#endif
    );

#ifdef LV_BACKEND_VULKAN
	deferredRenderPass.normalRoughnessAttachment.transitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	deferredRenderPass.albedoMetallicAttachment.transitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	deferredRenderPass.depthAttachment.transitionLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
#endif

    deferredRenderPass.depthAttachment.loadOp = LV_ATTACHMENT_LOAD_OP_LOAD;
#pragma endregion DEFERRED_RENDER_PASS

    //Shadow render pass
#pragma region SHADOW_RENDER_PASS
    ShadowRenderPass shadowRenderPass{};
    shadowRenderPass.depthAttachment.usage |= LV_IMAGE_USAGE_SAMPLED_BIT | LV_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    shadowRenderPass.depthAttachment.format = swapChain.depthFormat;
    shadowRenderPass.depthAttachment.loadOp = LV_ATTACHMENT_LOAD_OP_CLEAR;
    shadowRenderPass.depthAttachment.layerCount = CASCADE_COUNT;
#ifdef LV_BACKEND_VULKAN
    shadowRenderPass.depthAttachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	shadowRenderPass.depthAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    shadowRenderPass.depthAttachmentView.viewType = LV_IMAGE_VIEW_TYPE_2D_ARRAY;
#elif defined LV_BACKEND_METAL
	shadowRenderPass.depthAttachment.viewType = LV_IMAGE_VIEW_TYPE_2D_ARRAY;
    shadowRenderPass.depthAttachment.storageMode = MTL::StorageModePrivate;
#endif
    //shadowRenderPass.depthAttachment.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
    shadowRenderPass.depthAttachment.init(SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
#ifdef LV_BACKEND_VULKAN
    shadowRenderPass.depthAttachmentView.init(&shadowRenderPass.depthAttachment);
#endif
    shadowRenderPass.depthAttachmentSampler.init();

    /*
    for (uint8_t i = 0; i < CASCADE_COUNT; i++) {
#ifdef LV_BACKEND_VULKAN
        shadowRenderPass.depthAttachmentViews[i].baseLayer = i;
        shadowRenderPass.depthAttachmentViews[i].layerCount = 1;
        shadowRenderPass.depthAttachmentViews[i].init(&shadowRenderPass.depthAttachment);
#elif defined(LV_BACKEND_METAL)
        shadowRenderPass.depthAttachmentViews[i] = shadowRenderPass.depthAttachment;
        //shadowRenderPass.depthAttachmentViews[i].baseLayer = i;
        shadowRenderPass.depthAttachmentViews[i].layerCount = 1;
        shadowRenderPass.depthAttachmentViews[i].images[0] = shadowRenderPass.depthAttachment.images[0]->newTextureView(shadowRenderPass.depthAttachment.format, LV_IMAGE_VIEW_TYPE_2D, NS::Range(0, 1), NS::Range(i, 1));
#endif

#ifdef LV_BACKEND_VULKAN
        shadowRenderPass.framebuffers[i].setDepthAttachment({&shadowRenderPass.depthAttachment, &shadowRenderPass.depthAttachmentViews[i], 0});
#elif defined(LV_BACKEND_METAL)
        shadowRenderPass.framebuffers[i].setDepthAttachment({&shadowRenderPass.depthAttachmentViews[i], 0});
#endif
    }
    */

    shadowRenderPass.framebuffer.setDepthAttachment({&shadowRenderPass.depthAttachment,
#ifdef LV_BACKEND_VULKAN
    &shadowRenderPass.depthAttachmentView,
#endif
    0});

#ifdef LV_BACKEND_VULKAN
    shadowRenderPass.renderPass.init(shadowRenderPass.framebuffer/*s[0]*/.getAttachmentDescriptions());
#endif

    //for (uint8_t i = 0; i < CASCADE_COUNT; i++) {
        shadowRenderPass.framebuffer/*s[i]*/.init(
#ifdef LV_BACKEND_VULKAN
            &shadowRenderPass.renderPass, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE
#endif
        );
    //}

#ifdef LV_BACKEND_VULKAN
	shadowRenderPass.depthAttachment.transitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
#endif

    /*
    for (uint8_t i = 0; i < CASCADE_COUNT; i++) {
        //std::cout << (int)i << std::endl;
        shadowRenderPass.depthAttachmentViews[i].baseLayer = i;
        shadowRenderPass.depthAttachmentViews[i].layerCount = 1;
        shadowRenderPass.depthAttachmentViews[i].init(&shadowRenderPass.depthAttachment);
        shadowRenderPass.framebuffers[i].setDepthAttachment({&shadowRenderPass.depthAttachment, &shadowRenderPass.depthAttachmentViews[i], 0});

        if (i == 0) shadowRenderPass.renderPass.init(shadowRenderPass.framebuffers[0].getAttachmentDescriptions());

        shadowRenderPass.framebuffers[i].init(&shadowRenderPass.renderPass, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
    }
    */
    //shadowRenderPass.framebuffer.clearValues[0].depthStencil = {0.0f, 0};
#pragma endregion SHADOW_RENDER_PASS

    //SSAO render pass
#pragma region SSAO_RENDER_PASS
    SSAORenderPass ssaoRenderPass{};
    ssaoRenderPass.colorAttachment.usage |= LV_IMAGE_USAGE_SAMPLED_BIT | LV_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ssaoRenderPass.colorAttachment.format = LV_FORMAT_R8_UNORM;
#ifdef LV_BACKEND_VULKAN
    ssaoRenderPass.colorAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ssaoRenderPass.colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
#endif
    ssaoRenderPass.colorAttachment.init(SRC_WIDTH, SRC_HEIGHT);
#ifdef LV_BACKEND_VULKAN
    ssaoRenderPass.colorAttachmentView.init(&ssaoRenderPass.colorAttachment);
#endif
    ssaoRenderPass.colorAttachmentSampler.init();

    ssaoRenderPass.framebuffer.addColorAttachment({&ssaoRenderPass.colorAttachment,
#ifdef LV_BACKEND_VULKAN
    &ssaoRenderPass.colorAttachmentView,
#endif
    0});

    ssaoRenderPass.framebuffer.setDepthAttachment({&deferredRenderPass.depthAttachment,
#ifdef LV_BACKEND_VULKAN
    &deferredRenderPass.depthAttachmentView,
#endif
    1});

#ifdef LV_BACKEND_VULKAN
    ssaoRenderPass.renderPass.init(ssaoRenderPass.framebuffer.getAttachmentDescriptions());
#endif

    ssaoRenderPass.framebuffer.init(
#ifdef LV_BACKEND_VULKAN
        &ssaoRenderPass.renderPass, SRC_WIDTH, SRC_HEIGHT
#endif
    );

#ifdef LV_BACKEND_VULKAN
	ssaoRenderPass.colorAttachment.transitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
#endif
#pragma endregion SSAO_RENDER_PASS

    //SSAO blur render pass
#pragma region SSAO_BLUR_RENDER_PASS
    SSAOBlurRenderPass ssaoBlurRenderPass{};
    ssaoBlurRenderPass.colorAttachment.usage |= LV_IMAGE_USAGE_SAMPLED_BIT | LV_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ssaoBlurRenderPass.colorAttachment.format = LV_FORMAT_R8_UNORM;
#ifdef LV_BACKEND_VULKAN
    ssaoBlurRenderPass.colorAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ssaoBlurRenderPass.colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
#endif
    ssaoBlurRenderPass.colorAttachment.init(SRC_WIDTH, SRC_HEIGHT);
#ifdef LV_BACKEND_VULKAN
    ssaoBlurRenderPass.colorAttachmentView.init(&ssaoBlurRenderPass.colorAttachment);
#endif
    ssaoBlurRenderPass.colorAttachmentSampler.init();

    ssaoBlurRenderPass.framebuffer.addColorAttachment({&ssaoBlurRenderPass.colorAttachment,
#ifdef LV_BACKEND_VULKAN
    &ssaoBlurRenderPass.colorAttachmentView,
#endif
    0});

    ssaoBlurRenderPass.framebuffer.setDepthAttachment({&deferredRenderPass.depthAttachment,
#ifdef LV_BACKEND_VULKAN
    &deferredRenderPass.depthAttachmentView,
#endif
    1});

#ifdef LV_BACKEND_VULKAN
    ssaoBlurRenderPass.renderPass.init(ssaoBlurRenderPass.framebuffer.getAttachmentDescriptions());
#endif

    ssaoBlurRenderPass.framebuffer.init(
#ifdef LV_BACKEND_VULKAN
        &ssaoBlurRenderPass.renderPass, SRC_WIDTH, SRC_HEIGHT
#endif
    );

#ifdef LV_BACKEND_VULKAN
	ssaoBlurRenderPass.colorAttachment.transitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
#endif
#pragma endregion SSAO_BLUR_RENDER_PASS

    //Main render pass
#pragma region MAIN_RENDER_PASS
    MainRenderPass mainRenderPass{};
    mainRenderPass.colorAttachment.usage |= LV_IMAGE_USAGE_SAMPLED_BIT | LV_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    mainRenderPass.colorAttachment.format = LV_FORMAT_RGBA32_SFLOAT;
#ifdef LV_BACKEND_VULKAN
    mainRenderPass.colorAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    mainRenderPass.colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
#endif
    mainRenderPass.colorAttachment.init(SRC_WIDTH, SRC_HEIGHT);
#ifdef LV_BACKEND_VULKAN
    mainRenderPass.colorAttachmentView.init(&mainRenderPass.colorAttachment);
#endif
    mainRenderPass.colorAttachmentSampler.init();

    //mainRenderPass.depthAttachment.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    //mainRenderPass.depthAttachment.init(mainRenderPass.framebuffer.width, mainRenderPass.framebuffer.height, lv::g_swapChain.depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    mainRenderPass.framebuffer.addColorAttachment({&mainRenderPass.colorAttachment,
#ifdef LV_BACKEND_VULKAN
    &mainRenderPass.colorAttachmentView,
#endif
    0});

    mainRenderPass.framebuffer.setDepthAttachment({&deferredRenderPass.depthAttachment,
#ifdef LV_BACKEND_VULKAN
    &deferredRenderPass.depthAttachmentView,
#endif
    1});
    //mainRenderPass.framebuffer.setDepthAttachment({&deferredRenderPass.depthAttachment, &deferredRenderPass.depthAttachmentView, 1});
    //mainRenderPass.framebuffer.setDepthAttachment(&mainRenderPass.depthAttachment, 1);

#ifdef LV_BACKEND_VULKAN
    mainRenderPass.renderPass.init(mainRenderPass.framebuffer.getAttachmentDescriptions());
#endif

    mainRenderPass.framebuffer.init(
#ifdef LV_BACKEND_VULKAN
        &mainRenderPass.renderPass, SRC_WIDTH, SRC_HEIGHT
#endif
    );

#ifdef LV_BACKEND_VULKAN
	mainRenderPass.colorAttachment.transitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
#endif
#pragma endregion MAIN_RENDER_PASS

    //HDR render pass
#pragma region HDR_RENDER_PASS
    MainRenderPass hdrRenderPass{};
    hdrRenderPass.colorAttachment.usage |= LV_IMAGE_USAGE_SAMPLED_BIT | LV_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    hdrRenderPass.colorAttachment.format = LV_FORMAT_RGBA16_UNORM;
#ifdef LV_BACKEND_VULKAN
    hdrRenderPass.colorAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    hdrRenderPass.colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
#endif
    hdrRenderPass.colorAttachment.init(SRC_WIDTH, SRC_HEIGHT);
#ifdef LV_BACKEND_VULKAN
    hdrRenderPass.colorAttachmentView.init(&hdrRenderPass.colorAttachment);
#endif
    hdrRenderPass.colorAttachmentSampler.init();

    hdrRenderPass.framebuffer.addColorAttachment({&hdrRenderPass.colorAttachment,
#ifdef LV_BACKEND_VULKAN
    &hdrRenderPass.colorAttachmentView,
#endif
    0});

#ifdef LV_BACKEND_VULKAN
    hdrRenderPass.renderPass.init(hdrRenderPass.framebuffer.getAttachmentDescriptions());
#endif

    hdrRenderPass.framebuffer.init(
#ifdef LV_BACKEND_VULKAN
        &hdrRenderPass.renderPass, SRC_WIDTH, SRC_HEIGHT
#endif
    );

#ifdef LV_BACKEND_VULKAN
	hdrRenderPass.colorAttachment.transitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
#endif
#pragma endregion HDR_RENDER_PASS

    //Shaders

    // *************** Equirectangular to cubemap shader ***************
#pragma region EQUIRECTANGULAR_TO_CUBE_SHADER
#ifdef LV_BACKEND_VULKAN
    equiToCubeLayout.pushConstantRanges.resize(1);
	equiToCubeLayout.pushConstantRanges[0].stageFlags = LV_SHADER_STAGE_VERTEX_BIT | LV_SHADER_STAGE_FRAGMENT_BIT;
	equiToCubeLayout.pushConstantRanges[0].offset = 0;
	equiToCubeLayout.pushConstantRanges[0].size = sizeof(lv::UBOCubemapVP);

    equiToCubeLayout.init();
#endif

    //Vertex
    lv::ShaderModuleCreateInfo vertCubemapCreateInfo{};
#ifdef LV_BACKEND_VULKAN
    vertCubemapCreateInfo.shaderType = LV_SHADER_STAGE_VERTEX_BIT;
    vertCubemapCreateInfo.filename = "assets/shaders/vulkan/compiled/vertex/cubemap.spv";
#elif defined LV_BACKEND_METAL
    vertCubemapCreateInfo.filename = "assets/shaders/metal/compiled/vertex/cubemap.metallib";
#endif

    lv::ShaderModule vertCubemapModule(vertCubemapCreateInfo);

    //Fragment
    lv::ShaderModuleCreateInfo fragEquiToCubeCreateInfo{};
#ifdef LV_BACKEND_VULKAN
    fragEquiToCubeCreateInfo.shaderType = LV_SHADER_STAGE_FRAGMENT_BIT;
    fragEquiToCubeCreateInfo.filename = "assets/shaders/vulkan/compiled/fragment/equi_to_cube.spv";
#elif defined LV_BACKEND_METAL
    fragEquiToCubeCreateInfo.filename = "assets/shaders/metal/compiled/fragment/equi_to_cube.metallib";
#endif

    lv::ShaderModule fragEquiToCubeModule(fragEquiToCubeCreateInfo);
    
    //Shader
	lv::GraphicsPipelineCreateInfo equiToCubeShaderCreateInfo{};
	
	equiToCubeShaderCreateInfo.vertexShaderModule = &vertCubemapModule;
	equiToCubeShaderCreateInfo.fragmentShaderModule = &fragEquiToCubeModule;
#ifdef LV_BACKEND_VULKAN
	equiToCubeShaderCreateInfo.pipelineLayout = &equiToCubeLayout;
#endif
	//equiToCubeShaderCreateInfo.renderPass = &mainRenderPass.renderPass;
#pragma endregion EQUIRECTANGULAR_TO_CUBE_SHADER

    // *************** Irradiance shader ***************
#pragma region IRRADIANCE_SHADER
    /*
    lv::GET_SHADER_LAYOUT(SHADER_TYPE_IRRADIANCE).pushConstantRanges.resize(1);
	lv::GET_SHADER_LAYOUT(SHADER_TYPE_IRRADIANCE).pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	lv::GET_SHADER_LAYOUT(SHADER_TYPE_IRRADIANCE).pushConstantRanges[0].offset = 0;
	lv::GET_SHADER_LAYOUT(SHADER_TYPE_IRRADIANCE).pushConstantRanges[0].size = sizeof(lv::UBOEquiVP);
    */

#ifdef LV_BACKEND_VULKAN
    irradianceLayout.init();
#endif

    //Fragment
    lv::ShaderModuleCreateInfo fragIrradianceCreateInfo{};
#ifdef LV_BACKEND_VULKAN
    fragIrradianceCreateInfo.shaderType = LV_SHADER_STAGE_FRAGMENT_BIT;
    fragIrradianceCreateInfo.filename = "assets/shaders/vulkan/compiled/fragment/irradiance.spv";
#elif defined LV_BACKEND_METAL
    fragIrradianceCreateInfo.filename = "assets/shaders/metal/compiled/fragment/irradiance.metallib";
#endif

    lv::ShaderModule fragIrradianceModule(fragIrradianceCreateInfo);
    
    //Shader
	lv::GraphicsPipelineCreateInfo irradianceShaderCreateInfo{};
	
	irradianceShaderCreateInfo.vertexShaderModule = &vertCubemapModule;
	irradianceShaderCreateInfo.fragmentShaderModule = &fragIrradianceModule;
#ifdef LV_BACKEND_VULKAN
	irradianceShaderCreateInfo.pipelineLayout = &irradianceLayout;
#endif
	//irradianceCreateInfo.renderPass = &mainRenderPass.renderPass;

#ifdef LV_BACKEND_VULKAN
    irradianceShaderCreateInfo.config.depthTest = LV_FALSE;
#endif
#pragma endregion IRRADIANCE_SHADER

    // *************** Irradiance shader ***************
#pragma region PREFILTERED_SHADER
#ifdef LV_BACKEND_VULKAN
    prefilterLayout.pushConstantRanges.resize(1);
	prefilterLayout.pushConstantRanges[0].stageFlags = LV_SHADER_STAGE_VERTEX_BIT | LV_SHADER_STAGE_FRAGMENT_BIT;
	prefilterLayout.pushConstantRanges[0].offset = 0;
	prefilterLayout.pushConstantRanges[0].size = sizeof(float);

    prefilterLayout.init();
#endif

    //Fragment
    lv::ShaderModuleCreateInfo fragPrefilteredCreateInfo{};
#ifdef LV_BACKEND_VULKAN
    fragPrefilteredCreateInfo.shaderType = LV_SHADER_STAGE_FRAGMENT_BIT;
    fragPrefilteredCreateInfo.filename = "assets/shaders/vulkan/compiled/fragment/prefiltered.spv";
#elif defined LV_BACKEND_METAL
    fragPrefilteredCreateInfo.filename = "assets/shaders/metal/compiled/fragment/prefiltered.metallib";
#endif

    lv::ShaderModule fragPrefilteredModule(fragPrefilteredCreateInfo);
    
    //Shader
	lv::GraphicsPipelineCreateInfo prefilteredShaderCreateInfo{};
	
	prefilteredShaderCreateInfo.vertexShaderModule = &vertCubemapModule;
	prefilteredShaderCreateInfo.fragmentShaderModule = &fragPrefilteredModule;
#ifdef LV_BACKEND_VULKAN
	prefilteredShaderCreateInfo.pipelineLayout = &prefilterLayout;
#endif
	//prefilteredShaderCreateInfo.renderPass = &mainRenderPass.renderPass
#pragma endregion PREFILTERED_SHADER

    // *************** Deferred shader ***************
#pragma region DEFERRED_SHADER
#ifdef LV_BACKEND_VULKAN
    deferredLayout.pushConstantRanges.resize(1);
	deferredLayout.pushConstantRanges[0].stageFlags = LV_SHADER_STAGE_VERTEX_BIT;
	deferredLayout.pushConstantRanges[0].offset = 0;
	deferredLayout.pushConstantRanges[0].size = sizeof(lv::PushConstantModel);

    deferredLayout.init();
#endif

    //Vertex
    lv::ShaderModuleCreateInfo vertDeferredCreateInfo{};
#ifdef LV_BACKEND_VULKAN
    vertDeferredCreateInfo.shaderType = LV_SHADER_STAGE_VERTEX_BIT;
    vertDeferredCreateInfo.filename = "assets/shaders/vulkan/compiled/vertex/deferred.spv";
#elif defined LV_BACKEND_METAL
    vertDeferredCreateInfo.filename = "assets/shaders/metal/compiled/vertex/deferred.metallib";
#endif

    lv::ShaderModule vertDeferredModule(vertDeferredCreateInfo);

    //Fragment
    lv::ShaderModuleCreateInfo fragDeferredCreateInfo{};
#ifdef LV_BACKEND_VULKAN
    fragDeferredCreateInfo.shaderType = LV_SHADER_STAGE_FRAGMENT_BIT;
    fragDeferredCreateInfo.filename = "assets/shaders/vulkan/compiled/fragment/deferred.spv";
#elif defined LV_BACKEND_METAL
    fragDeferredCreateInfo.filename = "assets/shaders/metal/compiled/fragment/deferred.metallib";
#endif

    lv::ShaderModule fragDeferredModule(fragDeferredCreateInfo);
    
    //Shader
	lv::GraphicsPipelineCreateInfo deferredGraphicsPipelineCreateInfo{};
	deferredGraphicsPipelineCreateInfo.vertexShaderModule = &vertDeferredModule;
	deferredGraphicsPipelineCreateInfo.fragmentShaderModule = &fragDeferredModule;
#ifdef LV_BACKEND_VULKAN
	deferredGraphicsPipelineCreateInfo.pipelineLayout = &deferredLayout;
	deferredGraphicsPipelineCreateInfo.renderPass = &deferredRenderPass.renderPass;
#elif defined LV_BACKEND_METAL
    deferredGraphicsPipelineCreateInfo.framebuffer = &deferredRenderPass.framebuffer;
#endif

    deferredGraphicsPipelineCreateInfo.vertexDescriptor = lv::MainVertex::getVertexDescriptor();

    deferredGraphicsPipelineCreateInfo.config.cullMode = LV_CULL_MODE_BACK_BIT;
    deferredGraphicsPipelineCreateInfo.config.depthTest = LV_TRUE;

	lv::GraphicsPipeline deferredGraphicsPipeline(deferredGraphicsPipelineCreateInfo);
#pragma endregion DEFERRED_SHADER

    // *************** Shadow shader ***************
#pragma region SHADOW_SHADER
#ifdef LV_BACKEND_VULKAN
    shadowLayout.pushConstantRanges.resize(1);
	shadowLayout.pushConstantRanges[0].stageFlags = LV_SHADER_STAGE_VERTEX_BIT;
	shadowLayout.pushConstantRanges[0].offset = 0;
	shadowLayout.pushConstantRanges[0].size = sizeof(glm::mat4);

    shadowLayout.init();
#endif

    //Vertex
    lv::ShaderModuleCreateInfo vertShadowCreateInfo{};
#ifdef LV_BACKEND_VULKAN
    vertShadowCreateInfo.shaderType = LV_SHADER_STAGE_VERTEX_BIT;
    vertShadowCreateInfo.filename = "assets/shaders/vulkan/compiled/vertex/shadow.spv";
#elif defined LV_BACKEND_METAL
    vertShadowCreateInfo.filename = "assets/shaders/metal/compiled/vertex/shadow.metallib";
#endif

    lv::ShaderModule vertShadowModule(vertShadowCreateInfo);

    //Fragment
    lv::ShaderModuleCreateInfo fragShadowCreateInfo{};
#ifdef LV_BACKEND_VULKAN
    fragShadowCreateInfo.shaderType = LV_SHADER_STAGE_FRAGMENT_BIT;
    fragShadowCreateInfo.filename = "assets/shaders/vulkan/compiled/fragment/shadow.spv";
#elif defined LV_BACKEND_METAL
    fragShadowCreateInfo.filename = "assets/shaders/metal/compiled/fragment/shadow.metallib";
#endif

    lv::ShaderModule fragShadowModule(fragShadowCreateInfo);
    
    //Shader
	lv::GraphicsPipelineCreateInfo shadowGraphicsPipelineCreateInfo{};
	
	shadowGraphicsPipelineCreateInfo.vertexShaderModule = &vertShadowModule;
	shadowGraphicsPipelineCreateInfo.fragmentShaderModule = &fragShadowModule;
#ifdef LV_BACKEND_VULKAN
	shadowGraphicsPipelineCreateInfo.pipelineLayout = &shadowLayout;
	shadowGraphicsPipelineCreateInfo.renderPass = &shadowRenderPass.renderPass;
#elif defined LV_BACKEND_METAL
    shadowGraphicsPipelineCreateInfo.framebuffer = &shadowRenderPass.framebuffer/*s[0]*/;
#endif

    shadowGraphicsPipelineCreateInfo.vertexDescriptor = lv::MainVertex::getVertexDescriptorShadows();

    shadowGraphicsPipelineCreateInfo.config.cullMode = LV_CULL_MODE_FRONT_BIT;
    shadowGraphicsPipelineCreateInfo.config.depthTest = LV_TRUE;

	lv::GraphicsPipeline shadowGraphicsPipeline(shadowGraphicsPipelineCreateInfo);
#pragma endregion SHADOW_SHADER

    // *************** SSAO shader ***************
#pragma region SSAO_SHADER
#ifdef LV_BACKEND_VULKAN
    ssaoLayout.pushConstantRanges.resize(1);
	ssaoLayout.pushConstantRanges[0].stageFlags = LV_SHADER_STAGE_FRAGMENT_BIT;
	ssaoLayout.pushConstantRanges[0].offset = 0;
	ssaoLayout.pushConstantRanges[0].size = sizeof(PCSsaoVP);

    ssaoLayout.init();
#endif

    //Vertex
    lv::ShaderModuleCreateInfo vertTriangleCreateInfo{};
#ifdef LV_BACKEND_VULKAN
    vertTriangleCreateInfo.shaderType = LV_SHADER_STAGE_VERTEX_BIT;
    vertTriangleCreateInfo.filename = "assets/shaders/vulkan/compiled/vertex/triangle.spv";
#elif defined LV_BACKEND_METAL
    vertTriangleCreateInfo.filename = "assets/shaders/metal/compiled/vertex/triangle.metallib";
#endif

    lv::ShaderModule vertTriangleModule(vertTriangleCreateInfo);

    //Fragment
    lv::ShaderModuleCreateInfo fragSsaoCreateInfo{};
#ifdef LV_BACKEND_VULKAN
    fragSsaoCreateInfo.shaderType = LV_SHADER_STAGE_FRAGMENT_BIT;
    fragSsaoCreateInfo.filename = "assets/shaders/vulkan/compiled/fragment/ssao.spv";
#elif defined LV_BACKEND_METAL
    fragSsaoCreateInfo.filename = "assets/shaders/metal/compiled/fragment/ssao.metallib";
#endif

    lv::ShaderModule fragSsaoModule(fragSsaoCreateInfo);
    
    //Shader
	lv::GraphicsPipelineCreateInfo ssaoGraphicsPipelineCreateInfo{};
	
	ssaoGraphicsPipelineCreateInfo.vertexShaderModule = &vertTriangleModule;
	ssaoGraphicsPipelineCreateInfo.fragmentShaderModule = &fragSsaoModule;
#ifdef LV_BACKEND_VULKAN
	ssaoGraphicsPipelineCreateInfo.pipelineLayout = &ssaoLayout;
	ssaoGraphicsPipelineCreateInfo.renderPass = &ssaoRenderPass.renderPass;
#elif defined LV_BACKEND_METAL
    ssaoGraphicsPipelineCreateInfo.framebuffer = &ssaoRenderPass.framebuffer;
#endif

    ssaoGraphicsPipelineCreateInfo.config.depthTest = LV_TRUE;
    ssaoGraphicsPipelineCreateInfo.config.depthWrite = LV_FALSE;
    ssaoGraphicsPipelineCreateInfo.config.depthOp = LV_COMPARE_OP_NOT_EQUAL;

	lv::GraphicsPipeline ssaoGraphicsPipeline(ssaoGraphicsPipelineCreateInfo);
#pragma endregion SSAO_SHADER

    // *************** SSAO blur shader ***************
#pragma region SSAO_BLUR_SHADER
#ifdef LV_BACKEND_VULKAN
    ssaoBlurLayout.init();
#endif

    //Fragment
    lv::ShaderModuleCreateInfo fragSsaoBlurCreateInfo{};
#ifdef LV_BACKEND_VULKAN
    fragSsaoBlurCreateInfo.shaderType = LV_SHADER_STAGE_FRAGMENT_BIT;
    fragSsaoBlurCreateInfo.filename = "assets/shaders/vulkan/compiled/fragment/blur.spv";
#elif defined LV_BACKEND_METAL
    fragSsaoBlurCreateInfo.filename = "assets/shaders/metal/compiled/fragment/blur.metallib";
#endif

    lv::ShaderModule fragSsaoBlurModule(fragSsaoBlurCreateInfo);
    
    //Shader
	lv::GraphicsPipelineCreateInfo ssaoBlurGraphicsPipelineCreateInfo{};
	
	ssaoBlurGraphicsPipelineCreateInfo.vertexShaderModule = &vertTriangleModule;
	ssaoBlurGraphicsPipelineCreateInfo.fragmentShaderModule = &fragSsaoBlurModule;
#ifdef LV_BACKEND_VULKAN
	ssaoBlurGraphicsPipelineCreateInfo.pipelineLayout = &ssaoBlurLayout;
	ssaoBlurGraphicsPipelineCreateInfo.renderPass = &ssaoBlurRenderPass.renderPass;
#elif defined LV_BACKEND_METAL
    ssaoBlurGraphicsPipelineCreateInfo.framebuffer = &ssaoBlurRenderPass.framebuffer;
#endif

    ssaoBlurGraphicsPipelineCreateInfo.config.depthTest = LV_TRUE;
    ssaoBlurGraphicsPipelineCreateInfo.config.depthWrite = LV_FALSE;
    ssaoBlurGraphicsPipelineCreateInfo.config.depthOp = LV_COMPARE_OP_NOT_EQUAL;

	lv::GraphicsPipeline ssaoBlurGraphicsPipeline(ssaoBlurGraphicsPipelineCreateInfo);
#pragma endregion SSAO_BLUR_SHADER

    // *************** Skylight shader ***************
#pragma region SKYLIGHT_SHADER
#ifdef LV_BACKEND_VULKAN
    skylightLayout.pushConstantRanges.resize(1);
	skylightLayout.pushConstantRanges[0].stageFlags = LV_SHADER_STAGE_VERTEX_BIT | LV_SHADER_STAGE_FRAGMENT_BIT;
	skylightLayout.pushConstantRanges[0].offset = 0;
	skylightLayout.pushConstantRanges[0].size = sizeof(glm::mat4);

    skylightLayout.init();
#endif

    //Vertex
    lv::ShaderModuleCreateInfo vertSkylightCreateInfo{};
#ifdef LV_BACKEND_VULKAN
    vertSkylightCreateInfo.shaderType = LV_SHADER_STAGE_VERTEX_BIT;
    vertSkylightCreateInfo.filename = "assets/shaders/vulkan/compiled/vertex/skylight.spv";
#elif defined LV_BACKEND_METAL
    vertSkylightCreateInfo.filename = "assets/shaders/metal/compiled/vertex/skylight.metallib";
#endif

    lv::ShaderModule vertSkylightModule(vertSkylightCreateInfo);

    //Fragment
    lv::ShaderModuleCreateInfo fragSkylightCreateInfo{};
#ifdef LV_BACKEND_VULKAN
    fragSkylightCreateInfo.shaderType = LV_SHADER_STAGE_FRAGMENT_BIT;
    fragSkylightCreateInfo.filename = "assets/shaders/vulkan/compiled/fragment/skylight.spv";
#elif defined LV_BACKEND_METAL
    fragSkylightCreateInfo.filename = "assets/shaders/metal/compiled/fragment/skylight.metallib";
#endif

    lv::ShaderModule fragSkylightModule(fragSkylightCreateInfo);
    
    //Shader
	lv::GraphicsPipelineCreateInfo skylightGraphicsPipelineCreateInfo{};
	
	skylightGraphicsPipelineCreateInfo.vertexShaderModule = &vertSkylightModule;
	skylightGraphicsPipelineCreateInfo.fragmentShaderModule = &fragSkylightModule;
#ifdef LV_BACKEND_VULKAN
	skylightGraphicsPipelineCreateInfo.pipelineLayout = &skylightLayout;
	skylightGraphicsPipelineCreateInfo.renderPass = &mainRenderPass.renderPass;
#elif defined LV_BACKEND_METAL
    skylightGraphicsPipelineCreateInfo.framebuffer = &mainRenderPass.framebuffer;
#endif

    skylightGraphicsPipelineCreateInfo.vertexDescriptor = lv::Vertex3D::getVertexDescriptor();

    skylightGraphicsPipelineCreateInfo.config.depthWrite = LV_FALSE;

	lv::GraphicsPipeline skylightGraphicsPipeline(skylightGraphicsPipelineCreateInfo);
#pragma endregion SKYLIGHT_SHADER

    // *************** Main shader ***************
#pragma region MAIN_SHADER
#ifdef LV_BACKEND_VULKAN
    mainLayout.init();
#endif

    //Fragment
    lv::ShaderModuleCreateInfo fragMainCreateInfo{};
#ifdef LV_BACKEND_VULKAN
    fragMainCreateInfo.shaderType = LV_SHADER_STAGE_FRAGMENT_BIT;
    fragMainCreateInfo.filename = "assets/shaders/vulkan/compiled/fragment/main.spv";
#elif defined LV_BACKEND_METAL
    fragMainCreateInfo.filename = "assets/shaders/metal/compiled/fragment/main.metallib";
#endif

    lv::ShaderModule fragMainModule(fragMainCreateInfo);
    
    //Shader
	lv::GraphicsPipelineCreateInfo mainGraphicsPipelineCreateInfo{};
	
	mainGraphicsPipelineCreateInfo.vertexShaderModule = &vertTriangleModule;
	mainGraphicsPipelineCreateInfo.fragmentShaderModule = &fragMainModule;
#ifdef LV_BACKEND_VULKAN
	mainGraphicsPipelineCreateInfo.pipelineLayout = &mainLayout;
	mainGraphicsPipelineCreateInfo.renderPass = &mainRenderPass.renderPass;
#elif defined LV_BACKEND_METAL
    mainGraphicsPipelineCreateInfo.framebuffer = &mainRenderPass.framebuffer;
#endif

    mainGraphicsPipelineCreateInfo.config.depthTest = LV_TRUE;
    mainGraphicsPipelineCreateInfo.config.depthWrite = LV_FALSE;
    mainGraphicsPipelineCreateInfo.config.depthOp = LV_COMPARE_OP_NOT_EQUAL;

	lv::GraphicsPipeline mainGraphicsPipeline(mainGraphicsPipelineCreateInfo);
#pragma endregion MAIN_SHADER

    // *************** Point light shader ***************
#pragma region POINT_LIGHT_SHADER
#ifdef LV_BACKEND_VULKAN
    pointLightLayout.pushConstantRanges.resize(1);
	pointLightLayout.pushConstantRanges[0].stageFlags = LV_SHADER_STAGE_VERTEX_BIT;
	pointLightLayout.pushConstantRanges[0].offset = 0;
	pointLightLayout.pushConstantRanges[0].size = sizeof(glm::vec3);

    pointLightLayout.init();
#endif

    //Vertex
    lv::ShaderModuleCreateInfo vertPointLightCreateInfo{};
#ifdef LV_BACKEND_VULKAN
    vertPointLightCreateInfo.shaderType = LV_SHADER_STAGE_VERTEX_BIT;
    vertPointLightCreateInfo.filename = "assets/shaders/vulkan/compiled/vertex/point_light.spv";
#elif defined LV_BACKEND_METAL
    vertPointLightCreateInfo.filename = "assets/shaders/metal/compiled/vertex/point_light.metallib";
#endif

    lv::ShaderModule vertPointLightModule(vertPointLightCreateInfo);

    //Fragment
    lv::ShaderModuleCreateInfo fragPointLightCreateInfo{};
#ifdef LV_BACKEND_VULKAN
    fragPointLightCreateInfo.shaderType = LV_SHADER_STAGE_FRAGMENT_BIT;
    fragPointLightCreateInfo.filename = "assets/shaders/vulkan/compiled/fragment/point_light.spv";
#elif defined LV_BACKEND_METAL
    fragPointLightCreateInfo.filename = "assets/shaders/metal/compiled/fragment/point_light.metallib";
#endif

    lv::ShaderModule fragPointLightModule(fragPointLightCreateInfo);
    
    //Shader
	lv::GraphicsPipelineCreateInfo pointLightGraphicsPipelineCreateInfo{};
	
	pointLightGraphicsPipelineCreateInfo.vertexShaderModule = &vertPointLightModule;
	pointLightGraphicsPipelineCreateInfo.fragmentShaderModule = &fragPointLightModule;
#ifdef LV_BACKEND_VULKAN
	pointLightGraphicsPipelineCreateInfo.pipelineLayout = &pointLightLayout;
	pointLightGraphicsPipelineCreateInfo.renderPass = &mainRenderPass.renderPass;
#elif defined LV_BACKEND_METAL
    pointLightGraphicsPipelineCreateInfo.framebuffer = &mainRenderPass.framebuffer;
#endif

    pointLightGraphicsPipelineCreateInfo.vertexDescriptor = lv::Vertex3D::getVertexDescriptor();

    pointLightGraphicsPipelineCreateInfo.config.depthTest = LV_TRUE;
    pointLightGraphicsPipelineCreateInfo.config.depthWrite = LV_FALSE;
    //pointLightGraphicsPipelineCreateInfo.config.depthOp = LV_COMPARE_OP_NOT_EQUAL;
    //TODO: configure the blending

	lv::GraphicsPipeline pointLightGraphicsPipeline(pointLightGraphicsPipelineCreateInfo);
#pragma endregion POINT_LIGHT_SHADER

    // *************** HDR shader ***************
#pragma region HDR_SHADER
    /*
    lv::GET_SHADER_LAYOUT(SHADER_TYPE_HDR).pushConstantRanges.resize(1);
	lv::GET_SHADER_LAYOUT(SHADER_TYPE_HDR).pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	lv::GET_SHADER_LAYOUT(SHADER_TYPE_HDR).pushConstantRanges[0].offset = 0;
	lv::GET_SHADER_LAYOUT(SHADER_TYPE_HDR).pushConstantRanges[0].size = sizeof(PCSkylightVP);
    */

#ifdef LV_BACKEND_VULKAN
    hdrLayout.init();
#endif

    //Fragment
    lv::ShaderModuleCreateInfo fragHdrCreateInfo{};
#ifdef LV_BACKEND_VULKAN
    fragHdrCreateInfo.shaderType = LV_SHADER_STAGE_FRAGMENT_BIT;
    fragHdrCreateInfo.filename = "assets/shaders/vulkan/compiled/fragment/hdr.spv";
#elif defined LV_BACKEND_METAL
    fragHdrCreateInfo.filename = "assets/shaders/metal/compiled/fragment/hdr.metallib";
#endif

    lv::ShaderModule fragHdrModule(fragHdrCreateInfo);
    
    //Shader
	lv::GraphicsPipelineCreateInfo hdrGraphicsPipelineCreateInfo{};
	
	hdrGraphicsPipelineCreateInfo.vertexShaderModule = &vertTriangleModule;
	hdrGraphicsPipelineCreateInfo.fragmentShaderModule = &fragHdrModule;
#ifdef LV_BACKEND_VULKAN
	hdrGraphicsPipelineCreateInfo.pipelineLayout = &hdrLayout;
	hdrGraphicsPipelineCreateInfo.renderPass = &hdrRenderPass.renderPass;
#elif defined LV_BACKEND_METAL
    hdrGraphicsPipelineCreateInfo.framebuffer = &hdrRenderPass.framebuffer;
#endif

	//hdrGraphicsPipelineCreateInfo.vertexBindingDescriptions = MainVertex::getBindingDescriptions();
	//hdrGraphicsPipelineCreateInfo.vertexAttributeDescriptions = MainVertex::getAttributeDescriptions();

	lv::GraphicsPipeline hdrGraphicsPipeline(hdrGraphicsPipelineCreateInfo);
#pragma endregion HDR_SHADER

    //Light
    lv::DirectLight directLight;
    directLight.light.direction = glm::normalize(glm::vec3(2.0f, -4.0f, 1.0f));

#ifdef LV_BACKEND_VULKAN
    //Uniform buffers
    lv::UniformBuffer deferredVPUniformBuffer(sizeof(glm::mat4));
    lv::UniformBuffer shadowVPUniformBuffers[CASCADE_COUNT] = { lv::UniformBuffer(sizeof(PCShadowVP)), lv::UniformBuffer(sizeof(PCShadowVP)), lv::UniformBuffer(sizeof(PCShadowVP)), lv::UniformBuffer(sizeof(PCShadowVP)) };
    lv::UniformBuffer mainShadowUniformBuffer(sizeof(glm::mat4) * CASCADE_COUNT);
    lv::UniformBuffer mainVPUniformBuffer(sizeof(UBOMainVP));

    //Deferred descriptor set
    lv::DescriptorSet deferredDecriptorSet(deferredLayout, 0);
    deferredDecriptorSet.addBufferBinding(deferredVPUniformBuffer.descriptorInfo(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    deferredDecriptorSet.init();

    //Shadow descriptor set
    lv::DescriptorSet shadowDecriptorSets[CASCADE_COUNT] = { lv::DescriptorSet(shadowLayout, 0), lv::DescriptorSet(shadowLayout, 0), lv::DescriptorSet(shadowLayout, 0), lv::DescriptorSet(shadowLayout, 0) };
    for (uint8_t i = 0; i < CASCADE_COUNT; i++) {
        shadowDecriptorSets[i].addBufferBinding(shadowVPUniformBuffers[i].descriptorInfo(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

        shadowDecriptorSets[i].init();
    }
#endif

    //Game
#ifdef LV_BACKEND_VULKAN
    Game game(deferredLayout);
#elif defined LV_BACKEND_METAL
    Game game;
#endif
    //game.filename = "sandbox/game.json";
    /*
    Scene::meshDataDir = "sandbox/assets/scenes/meshData";
    game.addScene();
    game.scene().filename = game.filename.substr(0, game.filename.find_last_of("/")) + "/assets/scenes/scene" + std::to_string(0) + ".json";
    game.scene().loaded = true;
    */
    game.loadFromFile();

    //Scene& scene = game.scenes[0];

    //Scene
    //Scene& scene = game.addScene();
    //scene.name = "scene0";
    //scene.filename = "sandbox/assets/scenes/scene0.json";

    //scene.loadFromFile();

    //Models

    //Model 1
    /*
    Entity entity(game.scene().addEntity(), game.scene().registry);
    lv::MeshLoader meshLoader
#ifdef LV_BACKEND_VULKAN
        (deferredLayout)
#endif
    ;// = entity.addComponent<lv::ModelComponent>(SHADER_TYPE_DEFERRED);
    //lv::ModelComponent model(SHADER_TYPE_DEFERRED);
    meshLoader.loadFromFile("sandbox/assets/models/backpack/backpack.obj");
    //meshLoader.init();

    for (uint8_t i = 0; i < meshLoader.meshes.size(); i++) {
        entt::entity entityID = game.scene().addEntity();
        game.scene().registry.emplace<lv::TransformComponent>(entityID);
        game.scene().registry.emplace<lv::MeshComponent>(entityID, meshLoader.meshes[i]);
        game.scene().registry.emplace<lv::MaterialComponent>(entityID
#ifdef LV_BACKEND_VULKAN
        , deferredLayout
#endif
        );
        entity.getComponent<lv::NodeComponent>().childs.push_back(entityID);
        game.scene().registry.get<lv::NodeComponent>(entityID).parent = entity.ID;
    }

    lv::TransformComponent& transformComponent = entity.addComponent<lv::TransformComponent>();
    //lv::TransformComponent transform;
    transformComponent.scale = glm::vec3(0.5f);
    //transform.position.y = 1.0f;

    entity.addComponent<lv::MaterialComponent>(
#ifdef LV_BACKEND_VULKAN
        deferredLayout
#endif
    );
    //lv::MaterialComponent material(SHADER_TYPE_DEFERRED);
    //materialComponent.init();
    //material.material.roughness = 0.6f;
    //material.material.metallic = 0.8f;

    //Model 2
    //lv::ModelComponent model(SHADER_TYPE_DEFERRED);
    Entity entity2(game.scene().addEntity(), game.scene().registry);
    std::array<lv::Texture*, 3> textures;
    std::vector<std::string> textureFilenames = {
        "albedo.png", "roughness.png", "metallic.png"//, "normal.png"
    };
    for (uint8_t i = 0; i < textures.size(); i++) {
        textures[i] = new lv::Texture;
        textures[i]->load(("sandbox/assets/textures/rust/" + textureFilenames[i]).c_str());
        textures[i]->init();
    }
    lv::MeshComponent& meshComponent2 = entity2.addComponent<lv::MeshComponent>(
#ifdef LV_BACKEND_VULKAN
        deferredLayout
#endif
    );
    meshComponent2.createPlane();
    for (uint8_t i = 0; i < textures.size(); i++) {
        //if (textures[i] != nullptr)
        meshComponent2.addTexture(textures[i], i
#ifdef LV_BACKEND_METAL
            , lv::MeshComponent::bindingIndices[i]
#endif
        );
    }
#ifdef LV_BACKEND_VULKAN
    entity2.getComponent<lv::MeshComponent>().descriptorSet.init();
#endif

    lv::TransformComponent& transformComponent2 = entity2.addComponent<lv::TransformComponent>();
    //lv::TransformComponent transform;
    transformComponent2.scale = glm::vec3(0.5f);
    //transform.position.y = 1.0f;

    entity2.addComponent<lv::MaterialComponent>(
#ifdef LV_BACKEND_VULKAN
        deferredLayout
#endif
    );
    */

    //Textures
    std::default_random_engine rndEngine((unsigned)time(0));
	std::uniform_real_distribution<int8_t> rndDist(-128, 127);

    std::vector<glm::i8vec4> ssaoNoise(SSAO_NOISE_SIZE * SSAO_NOISE_SIZE);
    for (uint32_t i = 0; i < ssaoNoise.size(); i++) {
        ssaoNoise[i] = glm::i8vec4(rndDist(rndEngine), rndDist(rndEngine), 0, 0);
    }

    lv::Texture ssaoNoiseTex;
    //ssaoNoiseTex.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    ssaoNoiseTex.textureData = ssaoNoise.data();
    ssaoNoiseTex.width = SSAO_NOISE_SIZE;
    ssaoNoiseTex.height = SSAO_NOISE_SIZE;
    ssaoNoiseTex.sampler.addressMode = LV_SAMPLER_ADDRESS_MODE_REPEAT;
    ssaoNoiseTex.init();

    /*
    lv::TransformComponent terrainTransform;
    //terrainTransform.position.x = 10.0f;

    lv::MaterialComponent terrainMaterial(SHADER_TYPE_DEFERRED);
    terrainMaterial.init();
    terrainMaterial.material.roughness = 0.8f;
    terrainMaterial.material.metallic = 0.1f;
    */

    //Textures
    /*
    lv::Texture grassTexture;
    grassTexture.init("Assets/Textures/Grass/grass.jpg");

    lv::DescriptorSet grassDS(SHADER_TYPE_DEFERRED, 2);
    grassDS.addImageBinding(grassTexture.descriptorInfo(), 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    grassDS.init();
    */

    //Skylight
    lv::Skylight skylight;
    skylight.load("assets/skylight/canyon.png", equiToCubeShaderCreateInfo);
    skylight.createIrradianceMap(irradianceShaderCreateInfo);
    skylight.createPrefilteredMap(prefilteredShaderCreateInfo);

    lv::Texture brdfLutTexture;
    brdfLutTexture.load("assets/textures/brdf_lut.png");
    brdfLutTexture.init();

#ifdef LV_BACKEND_VULKAN
    //Descriptor sets

    //Skylight descriptor set
    lv::DescriptorSet skylightDescriptorSet(skylightLayout, 0);
    skylightDescriptorSet.addImageBinding(skylight.environmentMapSampler.descriptorInfo(skylight.environmentMapImageView), 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    skylightDescriptorSet.init();
    lv::DescriptorSet ssaoDescriptorSet(ssaoLayout, 0);
    SETUP_SSAO_DESCRIPTORS
    ssaoDescriptorSet.init();

    lv::DescriptorSet ssaoBlurDescriptorSet(ssaoBlurLayout, 0);
    SETUP_SSAO_BLUR_DESCRIPTORS
    ssaoBlurDescriptorSet.init();

    lv::DescriptorSet mainDescriptorSet0(mainLayout, 0);
    SETUP_MAIN_0_DESCRIPTORS
    mainDescriptorSet0.init();

    lv::DescriptorSet mainDescriptorSet1(mainLayout, 1);
    SETUP_MAIN_1_DESCRIPTORS
    mainDescriptorSet1.init();

    lv::DescriptorSet mainDescriptorSet2(mainLayout, 2);
    SETUP_MAIN_2_DESCRIPTORS
    mainDescriptorSet2.init();

    lv::DescriptorSet hdrDescriptorSet(hdrLayout, 0);
    SETUP_HDR_DESCRIPTORS
    hdrDescriptorSet.init();

    //Viewports
	lv::Viewport mainViewport(0, 0, SRC_WIDTH/* * 2*/, SRC_HEIGHT/* * 2*/);
    lv::Viewport shadowViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
#endif

    //Vertex buffers
    //VertexBuffer vertexBuffer(vertices.data(), vertices.size() * sizeof(Vertex), indices);

    //Terrain
    /*
    std::vector<lv::MainVertex> vertices;
    std::vector<uint32_t> indices;

    //Noise
    const siv::PerlinNoise::seed_type seed = (unsigned)time(0);
	const siv::PerlinNoise perlin{ seed };

    for (uint8_t z = 0; z < CHUNK_WIDTH; z++) {
        for (uint8_t x = 0; x < CHUNK_WIDTH; x++) {
            float noise = perlin.octave2D_01(x * 0.01f, z * 0.01f, 16);

            lv::MainVertex vertex;
            vertex.position = glm::vec3(((int)x - CHUNK_WIDTH * 0.5f) * 0.02f, noise, ((int)z - CHUNK_WIDTH * 0.5f) * 0.02f);
            vertex.texCoord = glm::vec2((float)x / 15.0f, (float)z / 15.0f);
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);

            vertices.push_back(vertex);
            
            const uint32_t crntIndices[6] = {0, CHUNK_WIDTH + 1, 1, 0, CHUNK_WIDTH, CHUNK_WIDTH + 1};
            if (z < CHUNK_WIDTH - 2 && x < CHUNK_WIDTH - 2 && x > 1 && z > 1) {
				for (int i = 0; i < 6; i++) {
					indices.push_back(vertices.size() + crntIndices[i]);
				}
			}
        }
    }

    lv::VertexBuffer vertexBuffer(vertices.data(), vertices.size() * sizeof(lv::MainVertex), indices);
    */

    //camera.aspectRatio = (float)SRC_WIDTH / (float)SRC_HEIGHT;
    g_game->scene().editorCamera.yScroll = g_game->scene().editorCamera.farPlane * 0.05f;

    //SSAO Kernel
    /*
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between [0.0, 1.0]
    std::default_random_engine generator;
    std::vector<glm::vec3> ssaoKernel;
    for (unsigned int i = 0; i < 64; ++i) {
        glm::vec3 sample(
            randomFloats(generator) * 2.0 - 1.0, 
            randomFloats(generator) * 2.0 - 1.0, 
            randomFloats(generator)
        );
        sample  = glm::normalize(sample);
        sample *= randomFloats(generator);
        
        float scale = (float)i / 64.0; 
        scale = lerp(0.1f, 1.0f, scale * scale);
        sample *= scale;
        ssaoKernel.push_back(sample);  
    }

    //Printing kernel
    std::cout << "const vec3 SSAO_KERNEL[64] = vec3[](" << std::endl;
    for (uint8_t i = 0; i < ssaoKernel.size(); i++) {
        std::cout << "    vec3(" << ssaoKernel[i].x << ", " << ssaoKernel[i].y << ", " << ssaoKernel[i].z << ")" << (i < ssaoKernel.size() - 1 ? "," : "") << std::endl;
    }
    std::cout << ");" << std::endl;
    */

    //Editor
    Editor editor(window
#ifdef LV_BACKEND_VULKAN
    , deferredLayout
#endif
    );
    editor.createViewportSet(
#ifdef LV_BACKEND_VULKAN
        hdrRenderPass.colorAttachmentView.imageViews, hdrRenderPass.colorAttachmentSampler.sampler
#elif defined LV_BACKEND_METAL
        hdrRenderPass.colorAttachment.images
#endif
    );

    while (lvndWindowIsOpen(window)) {
        //Delta time
        float crntTime = lvndGetTime();
        float dt = std::max(crntTime - lastTime, 0.0f);
        lastTime = crntTime;

		std::string dtStr = std::to_string(dt * 1000.0f);
		for (uint8_t i = 0; i < 12 - dtStr.size(); i++)
			dtStr += " ";
		std::string newTitle = std::string(SRC_TITLE) + " " + dtStr + " ms";
		lvndSetWindowTitle(window, newTitle.c_str());

        shadowFrameCounter = (shadowFrameCounter + 1) % 24;

        lvndPollEvents(window);
        //std::cout << "Shift: " << lvndGetModifier(window.window, LVND_MODIFIER_SHIFT) << " : " << "Command: " << lvndGetModifier(window.window, LVND_MODIFIER_SUPER) << std::endl;

        //lvndSetWindowFullscreenMode(window, true);

        //Mouse
		int32_t mouseX, mouseY;
		lvndGetCursorPosition(window, &mouseX, &mouseY);
        //glm::rotateX(game.scene().camera->direction, )

        //Window size
        uint16_t width, height;
        lvndGetWindowSize(window, &width, &height);

        //Camera
        game.scene().updateCameraTransforms();

        game.scene().camera->aspectRatio = float(editor.viewportWidth) / float(editor.viewportHeight);
        if (editor.activeViewport == VIEWPORT_TYPE_SCENE) {
            if (editor.viewportActive) {
                game.scene().editorCamera.inputs(window, dt, mouseX, mouseY, width, height);
            } else {
                game.scene().editorCamera.firstClick = true;
            }
        }
        //std::cout << game.scene().camera->aspectRatio << " : " << game.scene().camera-> << std::endl;
        game.scene().camera->loadViewProj();

        //transform.rotation.y += 10.0f * dt;
        game.scene().calculateModels();

        game.scene().update(dt);
        
        //terrainTransform.calcModel();

        swapChain.acquireNextImage();

        //Rendering

        //Deferred render pass
        deferredRenderPass.framebuffer.bind();

        deferredGraphicsPipeline.bind();
#ifdef LV_BACKEND_VULKAN
        mainViewport.bind();
        deferredDecriptorSet.bind();
#endif

        glm::mat4 viewProj = g_game->scene().camera->projection * g_game->scene().camera->view;

#ifdef LV_BACKEND_VULKAN
        deferredVPUniformBuffer.upload(&viewProj);
#elif defined LV_BACKEND_METAL
        deferredGraphicsPipeline.uploadPushConstants(&viewProj, 1, MSL_SIZEOF(glm::mat4), LV_SHADER_STAGE_VERTEX_BIT);
#endif

        //For each model
        /*
        material.uploadUniforms();
        deferredGraphicsPipeline.uploadPushConstants(&transform.model, 0);
        model.render();

        material2.uploadUniforms();
        deferredGraphicsPipeline.uploadPushConstants(&transform2.model, 0);
        model2.render();
        */
        game.scene().render(deferredGraphicsPipeline);
        /*
        materialComponent.uploadUniforms();
        deferredGraphicsPipeline.uploadPushConstants(&transformComponent.model, 0);
        modelComponent.render();
        */

        //Terrain
        /*
        terrainMaterial.uploadUniforms(lv::GET_SHADER_LAYOUT(SHADER_TYPE_DEFERRED).pipelineLayout);
        deferredGraphicsPipeline.uploadPushConstants(&terrainTransform.model, 0);
        grassDS.bind(lv::GET_SHADER_LAYOUT(SHADER_TYPE_DEFERRED).pipelineLayout);
        vertexBuffer.bind();
        vertexBuffer.render();
        */

        deferredRenderPass.framebuffer.unbind();

		deferredRenderPass.framebuffer.render();

        //glm::mat4 shadowViewProj;

        //Shadow matrices
        getLightMatrices(directLight.light.direction);

        //Shadow render pass

        //Shadow matrices
        //glm::mat4 shadowProj = glm::ortho(-2.0f, 2.0f, -2.0f, 2.0f, camera.nearPlane, camera.farPlane);
        //glm::mat4 shadowView = glm::lookAt(glm::vec3(-2.0f, 4.0f, -1.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        //shadowVPs[0] = shadowProj * shadowView;

        /*
        for (uint8_t i = 0; i < CASCADE_COUNT; i++) {
            glm::mat4 shadowProj = glm::ortho(-2.0f, 2.0f, -2.0f, 2.0f, camera.nearPlane, camera.farPlane);
            glm::mat4 shadowView = glm::lookAt(glm::vec3(-2.0f, 4.0f, -1.0f), glm::vec3(i - 1, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

            //glm::mat4 shadowvp = shadowProj * shadowView;
            shadowVPs[i] = shadowProj * shadowView;
        }
        */
        shadowRenderPass.framebuffer.bind();

        //PCShadowVP pcShadowVPs[CASCADE_COUNT];
        for (uint8_t i = 0; i < CASCADE_COUNT; i++) {
            //if ((shadowFrameCounter + shadowStartingFrames[i]) % shadowRefreshFrames[i] == 0) {
                //std::cout << "Cascade: " << (int)i << std::endl;

                shadowGraphicsPipeline.bind();
#ifdef LV_BACKEND_VULKAN
                shadowViewport.bind();
#endif

#ifdef LV_BACKEND_VULKAN
                shadowDecriptorSets[i].bind();
#endif

                //PCShadowVP pcShadowVP = {shadowVPs[i]};
                PCShadowVP pcShadowVP{shadowVPs[i], (int)i};
                //std::cout << pcShadowVP.layerIndex << std::endl;
#ifdef LV_BACKEND_VULKAN
                shadowVPUniformBuffers[i].upload(&pcShadowVP);
#elif defined LV_BACKEND_METAL
                shadowGraphicsPipeline.uploadPushConstants(&pcShadowVP, 0, MSL_SIZEOF(PCShadowVP), LV_SHADER_STAGE_VERTEX_BIT);
#endif

                //For each model
                /*
                shadowGraphicsPipeline.uploadPushConstants(&transform.model.model, 0);
                model.renderShadows();

                shadowGraphicsPipeline.uploadPushConstants(&transform2.model.model, 0);
                model2.renderShadows();
                */
                game.scene().renderShadows(shadowGraphicsPipeline);

                //Terrain
                /*
                shadowGraphicsPipeline.uploadPushConstants(&terrainTransform.model, 0);
                vertexBuffer.bind();
                vertexBuffer.render();
                */
            //}
        }

        shadowRenderPass.framebuffer.unbind();

        shadowRenderPass.framebuffer.render();

        //SSAO render pass
        ssaoRenderPass.framebuffer.bind();

        ssaoGraphicsPipeline.bind();
#ifdef LV_BACKEND_VULKAN
        mainViewport.bind();
        ssaoDescriptorSet.bind();
#elif defined LV_BACKEND_METAL
        deferredRenderPass.depthAttachment.bind(0);
        deferredRenderPass.depthAttachmentSampler.bind(0);

        deferredRenderPass.normalRoughnessAttachment.bind(1);
        deferredRenderPass.normalRoughnessAttachmentSampler.bind(1);

        ssaoNoiseTex.image.bind(2);
        ssaoNoiseTex.sampler.bind(2);
#endif

        PCSsaoVP pcSsaoVP{g_game->scene().camera->projection, g_game->scene().camera->view, glm::inverse(viewProj)};
        ssaoGraphicsPipeline.uploadPushConstants(&pcSsaoVP, 0
#ifdef LV_BACKEND_METAL
        , MSL_SIZEOF(PCSsaoVP), LV_SHADER_STAGE_FRAGMENT_BIT
#endif
        );

        swapChain.renderFullscreenTriangle();

        ssaoRenderPass.framebuffer.unbind();

        ssaoRenderPass.framebuffer.render();

        //SSAO blur render pass
        ssaoBlurRenderPass.framebuffer.bind();

        ssaoBlurGraphicsPipeline.bind();
#ifdef LV_BACKEND_VULKAN
        mainViewport.bind();
        ssaoBlurDescriptorSet.bind();
#elif defined LV_BACKEND_METAL
        ssaoRenderPass.colorAttachment.bind(0);
        ssaoRenderPass.colorAttachmentSampler.bind(0);
#endif

        swapChain.renderFullscreenTriangle();

        ssaoBlurRenderPass.framebuffer.unbind();

        ssaoBlurRenderPass.framebuffer.render();

        //Main render pass
        mainRenderPass.framebuffer.bind();

        //Skylight
        skylightGraphicsPipeline.bind();
#ifdef LV_BACKEND_VULKAN
        mainViewport.bind();
        skylightDescriptorSet.bind();
#elif defined LV_BACKEND_METAL
        skylight.environmentMapImage.bind(0);
        skylight.environmentMapSampler.bind(0);
#endif

        glm::mat4 skylightViewProj = g_game->scene().camera->projection * glm::mat4(glm::mat3(g_game->scene().camera->view));
    
#ifdef LV_BACKEND_VULKAN
        skylightGraphicsPipeline.uploadPushConstants(&skylightViewProj, 0);
#elif defined LV_BACKEND_METAL
        skylightGraphicsPipeline.uploadPushConstants(&skylightViewProj, 0, MSL_SIZEOF(glm::mat4), LV_SHADER_STAGE_VERTEX_BIT);
#endif

		//swapChain.activeFramebuffer->encoder->setCullMode(LV_CULL_MODE_NONE);

        //std::cout << lv::CubeVertexBuffer::vertices.size() << " : " << lv::CubeVertexBuffer::indices.size() << std::endl;

#ifdef LV_BACKEND_VULKAN
        skylight.vertexBuffer->bindVertexBuffer();
        skylight.indexBuffer->bindIndexBuffer(LV_INDEX_TYPE_UINT32);
        skylight.indexBuffer->renderIndexed(sizeof(uint32_t));
#elif defined LV_BACKEND_METAL
        skylight.vertexBuffer->bindVertexBuffer(lv::Vertex3D::BINDING_INDEX);
        skylight.indexBuffer->renderIndexed(LV_INDEX_TYPE_UINT32, sizeof(uint32_t));
#endif

        //Main
        mainGraphicsPipeline.bind();
#ifdef LV_BACKEND_VULKAN
        mainViewport.bind();
        mainDescriptorSet0.bind();
        mainDescriptorSet1.bind();
        mainDescriptorSet2.bind();
#elif defined LV_BACKEND_METAL
		shadowRenderPass.depthAttachment.bind(0);
		shadowRenderPass.depthAttachmentSampler.bind(0);

		deferredRenderPass.depthAttachment.bind(1);
		deferredRenderPass.depthAttachmentSampler.bind(1);

        deferredRenderPass.normalRoughnessAttachment.bind(2);
		deferredRenderPass.normalRoughnessAttachmentSampler.bind(2);

		deferredRenderPass.albedoMetallicAttachment.bind(3);
		deferredRenderPass.albedoMetallicAttachmentSampler.bind(3);

        skylight.irradianceMapImage.bind(4);
        skylight.irradianceMapSampler.bind(4);

        skylight.prefilteredMapImage.bind(5);
        skylight.prefilteredMapSampler.bind(5);

        brdfLutTexture.image.bind(6);
        brdfLutTexture.sampler.bind(6);

		ssaoBlurRenderPass.colorAttachment.bind(7);
		ssaoBlurRenderPass.colorAttachmentSampler.bind(7);
#endif

        UBOMainVP uboMainVP{glm::inverse(viewProj), g_game->scene().camera->position};
#ifdef LV_BACKEND_VULKAN
        mainVPUniformBuffer.upload(&uboMainVP);
#elif defined LV_BACKEND_METAL
        mainGraphicsPipeline.uploadPushConstants(&uboMainVP, 2, MSL_SIZEOF(UBOMainVP), LV_SHADER_STAGE_FRAGMENT_BIT);
#endif

        directLight.uploadUniforms();
#ifdef LV_BACKEND_VULKAN
        //UBOMainShadow uboMainShadow{};
        //for (uint8_t i = 0; i < CASCADE_COUNT; i++) uboMainShadow.VPs[i] = shadowVPs[i];
        mainShadowUniformBuffer.upload(shadowVPs.data());
#elif defined LV_BACKEND_METAL
        directLight.lightUniformBuffer.bindToFragmentShader(0);

        mainGraphicsPipeline.uploadPushConstants(shadowVPs.data(), 1, shadowVPs.size() * MSL_SIZEOF(glm::mat4), LV_SHADER_STAGE_FRAGMENT_BIT);
#endif

        swapChain.renderFullscreenTriangle();

        mainRenderPass.framebuffer.unbind();

        mainRenderPass.framebuffer.render();

        //HDR render pass
        hdrRenderPass.framebuffer.bind();

        hdrGraphicsPipeline.bind();
#ifdef LV_BACKEND_VULKAN
        mainViewport.bind();
        hdrDescriptorSet.bind();
#elif defined LV_BACKEND_METAL
        mainRenderPass.colorAttachment.bind(0);
        mainRenderPass.colorAttachmentSampler.bind(0);
#endif

        //PCSkylightVP pcHdrVP{camera.projection, camera.view, camera.position};
        //hdrGraphicsPipeline.uploadPushConstants(&pcHdrVP, 0);

        swapChain.renderFullscreenTriangle();

        hdrRenderPass.framebuffer.unbind();

        hdrRenderPass.framebuffer.render();

        //Present
        swapChain.framebuffer.bind();

#ifdef LV_BACKEND_METAL
		swapChain.synchronize();
#endif
        
        //GUI
        editor.newFrame();

        editor.beginDockspace();

        editor.createSceneViewport();
        editor.createGameViewport();
        editor.createSceneHierarchy();
        editor.createPropertiesPanel();
        editor.createAssetsPanel();
        //ImGui::ShowDemoWindow();

        editor.endDockspace();

        editor.render();
    
		swapChain.framebuffer.unbind();

        swapChain.renderAndPresent();
    }

#ifdef LV_BACKEND_VULKAN
    device.waitIdle();

    directLight.destroy();

    std::cout << "Light destroyed" << std::endl;

    /*
    model.destroy();
    material.destroy();

    model2.destroy();
    material2.destroy();
    */
    game.destroy();

    std::cout << "Game destroyed" << std::endl;

    deferredVPUniformBuffer.destroy();
    for (auto& shadowVPUniformBuffer : shadowVPUniformBuffers)
        shadowVPUniformBuffer.destroy();
    mainShadowUniformBuffer.destroy();
    mainVPUniformBuffer.destroy();

    std::cout << "Uniform buffers destroyed" << std::endl;

    deferredRenderPass.renderPass.destroy();
    deferredRenderPass.framebuffer.destroy();

    shadowRenderPass.renderPass.destroy();
    //for (uint8_t i = 0; i < CASCADE_COUNT; i++)
        shadowRenderPass.framebuffer/*s[i]*/.destroy();

    ssaoRenderPass.renderPass.destroy();
    ssaoRenderPass.framebuffer.destroy();

    ssaoBlurRenderPass.renderPass.destroy();
    ssaoBlurRenderPass.framebuffer.destroy();

    mainRenderPass.renderPass.destroy();
    mainRenderPass.framebuffer.destroy();

    std::cout << "Framebuffers destroyed" << std::endl;

    vertCubemapModule.destroy();
    fragEquiToCubeModule.destroy();

    fragIrradianceModule.destroy();

    vertDeferredModule.destroy();
    fragDeferredModule.destroy();
    deferredGraphicsPipeline.destroy();

    vertShadowModule.destroy();
    fragShadowModule.destroy();
    shadowGraphicsPipeline.destroy();

    vertTriangleModule.destroy();
    fragSsaoModule.destroy();
    ssaoGraphicsPipeline.destroy();

    fragSsaoBlurModule.destroy();
    ssaoBlurGraphicsPipeline.destroy();

    vertSkylightModule.destroy();
    fragSkylightModule.destroy();
    skylightGraphicsPipeline.destroy();

    fragMainModule.destroy();
    mainGraphicsPipeline.destroy();

    fragHdrModule.destroy();
    hdrGraphicsPipeline.destroy();

    std::cout << "Shaders destroyed" << std::endl;

    /*
    deferredRenderPass.positionDepthAttachment.destroy();
    deferredRenderPass.positionDepthAttachmentView.destroy();
    deferredRenderPass.positionDepthAttachmentSampler.destroy();
    */

    deferredRenderPass.normalRoughnessAttachment.destroy();
    deferredRenderPass.normalRoughnessAttachmentView.destroy();
    deferredRenderPass.normalRoughnessAttachmentSampler.destroy();

    deferredRenderPass.albedoMetallicAttachment.destroy();
    deferredRenderPass.albedoMetallicAttachmentView.destroy();
    deferredRenderPass.albedoMetallicAttachmentSampler.destroy();

    deferredRenderPass.depthAttachment.destroy();
    deferredRenderPass.depthAttachmentView.destroy();

    shadowRenderPass.depthAttachment.destroy();
    shadowRenderPass.depthAttachmentView.destroy();
    //for (uint8_t i = 0; i < CASCADE_COUNT; i++)
    //    shadowRenderPass.depthAttachmentViews[i].destroy();
    shadowRenderPass.depthAttachmentSampler.destroy();

    ssaoRenderPass.colorAttachment.destroy();
    ssaoRenderPass.colorAttachmentView.destroy();
    ssaoRenderPass.colorAttachmentSampler.destroy();

    ssaoBlurRenderPass.colorAttachment.destroy();
    ssaoBlurRenderPass.colorAttachmentView.destroy();
    ssaoBlurRenderPass.colorAttachmentSampler.destroy();

    mainRenderPass.colorAttachment.destroy();
    mainRenderPass.colorAttachmentView.destroy();
    mainRenderPass.colorAttachmentSampler.destroy();

    std::cout << "Attachments destroyed" << std::endl;

    editor.destroy();

    descriptorPool.destroy();
    allocator.destroy();
    swapChain.destroy();
	device.destroy();
    instance.destroy();

    lvndVulkanDestroyLayer(window);
#elif defined LV_BACKEND_METAL
    game.destroy();

	device.destroy();

    lvndMetalDestroyLayer(window);
#endif

    lvndDestroyWindow(window);

    //scene.saveToFile();
    //game.saveToFile();

    return 0;
}

//Callbacks
void windowResizeCallback(LvndWindow* window, uint16_t width, uint16_t height) {
    //Wait for device
#ifdef LV_BACKEND_VULKAN
    lv::g_device->waitIdle();
#endif
    
    lv::g_swapChain->resize(window);

    /*
    camera.aspectRatio = (float)window.width / (float)window.height;

    //Resizing framebuffers
    deferredRenderPass.framebuffer.resize(window.width, window.height);
    
    ssaoRenderPass.framebuffer.resize(window.width, window.height);
    
    ssaoBlurRenderPass.framebuffer.resize(window.width, window.height);

    mainRenderPass.framebuffer.resize(window.width, window.height);

    //mainDescriptorSet0.reset();
    //SETUP_MAIN_0_DESCRIPTORS
    //mainDescriptorSet0.init();

    ssaoDescriptorSet.reset();
    SETUP_SSAO_DESCRIPTORS
    ssaoDescriptorSet.init();

    ssaoBlurDescriptorSet.reset();
    SETUP_SSAO_BLUR_DESCRIPTORS
    ssaoBlurDescriptorSet.init();

    mainDescriptorSet2.reset();
    SETUP_MAIN_2_DESCRIPTORS
    mainDescriptorSet2.init();

    hdrDescriptorSet.reset();
    SETUP_HDR_DESCRIPTORS
    hdrDescriptorSet.init();

    //Editor
    editor.resize();

    //Setting the total height for all viewports that do not match the size of framebuffer
    //guiViewport.setTotalHeight(window.height);

    mainViewport.setViewport(0, 0, window.width, window.height);
    */
    //std::cout << (int)width << ", " << (int)height << " : " << window->framebufferWidth << ", " << window->framebufferHeight << " : " << window->framebufferScaleX << ", " << window->framebufferScaleY << std::endl;
}

void scrollCallback(LvndWindow* window, double xoffset, double yoffset) {
    if (g_editor->activeViewport == VIEWPORT_TYPE_SCENE) {
        g_game->scene().editorCamera.yScroll -= yoffset * g_game->scene().editorCamera.yScroll / (g_game->scene().editorCamera.yScroll + 1.0f) * SCROLL_SENSITIVITY;
        g_game->scene().editorCamera.yScroll = std::fmax(std::fmin(g_game->scene().editorCamera.yScroll, g_game->scene().editorCamera.farPlane), g_game->scene().editorCamera.farPlane / 100000.0f);
    }
}

//Functions

//Shadows
std::vector<glm::vec4> getFrustumCorners(const glm::mat4& proj, const glm::mat4& view) {
    std::vector<glm::vec4> frustumCorners;
    const auto inv = glm::inverse(proj * view);

    for (unsigned int x = 0; x < 2; ++x) {
        for (unsigned int y = 0; y < 2; ++y) {
            for (unsigned int z = 0; z < 2; ++z) {
                const glm::vec4 pt =
                inv * glm::vec4(
                    (2.0f * x - 1.0f),
                    (2.0f * y - 1.0f),
                    (2.0f * z - 1.0f),
                    1.0f);
                frustumCorners.push_back(pt / pt.w);
            }
        }
    }

    return frustumCorners;
}

void getLightMatrix(glm::vec3 lightDir, float nearPlane, float farPlane, bool firstCascade) {
    glm::mat4 lightProj = glm::perspective(glm::radians(g_game->scene().camera->fov), g_game->scene().camera->aspectRatio, nearPlane, farPlane);

    std::vector<glm::vec4> frustumCorners = getFrustumCorners(lightProj, g_game->scene().camera->view);

    //Get frustum center
    glm::vec3 center = glm::vec3(0.0f, 0.0f, 0.0f);
    for (auto &corner : frustumCorners) {
        center += glm::vec3(corner);
    }
    center /= frustumCorners.size();

    glm::mat4 view = glm::lookAt(center, center + lightDir, glm::vec3(0.0f, 1.0f, 0.0f));

    //Get min/max corner coordinates
    float maxX = std::numeric_limits<float>::min();
    float maxY = std::numeric_limits<float>::min();
    float maxZ = std::numeric_limits<float>::min();
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float minZ = std::numeric_limits<float>::max();

    for (auto &corner : frustumCorners) {
        auto trf = view * corner;

        minX = std::min(minX, trf.x);
        maxX = std::max(maxX, trf.x);
        minY = std::min(minY, trf.y);
        maxY = std::max(maxY, trf.y);
        minZ = std::min(minZ, trf.z);
        maxZ = std::max(maxZ, trf.z);
    }

    float zAdd = g_game->scene().camera->farPlane * 0.5f;
    minZ -= zAdd;
    maxZ += zAdd;
    /*
    float xyAdd = SHADOW_FAR_PLANE * 0.08f;
    if (firstCascade) {
        minX -= xyAdd;
        maxX += xyAdd;
        minY -= xyAdd;
        maxY += xyAdd;
    }
    */
    /*
    std::cout << nearPlane << ", " << farPlane << std::endl;
    std::cout << minX << ", " << maxX << " : " << minY << ", " << maxY << " : " << minZ << ", " << maxZ << std::endl;
    */

    glm::mat4 projection = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);

    shadowVPs.push_back(projection * view);
}

void getLightMatrices(glm::vec3 lightDir) {
    //Clearing
    shadowVPs.clear();

    for (int i = 0; i < CASCADE_COUNT; i++) {
        if (i == 0) {
            getLightMatrix(lightDir, g_game->scene().camera->nearPlane, cascadeLevels[0], true);
        } else {
            getLightMatrix(lightDir, cascadeLevels[i - 1], cascadeLevels[i], false);
        }
    }
}

//Menu bar actions
void saveGame() {
    g_game->saveToFile();
}

void newEntityCreateEmpty() {
    g_game->scene().addEntity();
}

void newEntityLoadModel() {
    std::string filename = Editor::getFileDialogResult("ModelFileDialog", "Model files", "gltf,obj,fbx,blend");

    if (filename != "") {
        lv::Entity entity(g_game->scene().addEntity(), g_game->scene().registry);
        entity.getComponent<lv::TagComponent>().name = std::filesystem::path(filename).stem().string();

        entity.addComponent<lv::TransformComponent>();
        
        lv::MeshLoader meshLoader
#ifdef LV_BACKEND_VULKAN
            (g_game->deferredLayout)
#endif
        ;
        meshLoader.loadFromFile(filename.c_str());
        for (unsigned int i = 0; i < meshLoader.meshes.size(); i++) {
            lv::Entity entity2(g_game->scene().addEntity(), g_game->scene().registry);
            entity2.addComponent<lv::TransformComponent>();
            entity2.addComponent<lv::MeshComponent>(meshLoader.meshes[i]);
            entity2.addComponent<lv::MaterialComponent>(
#ifdef LV_BACKEND_VULKAN
                g_game->deferredLayout
#endif
            );

            entity.getComponent<lv::NodeComponent>().childs.push_back(entity2.ID);
            entity2.getComponent<lv::NodeComponent>().parent = entity.ID;
        }
    }
}

void newEntityPlane() {
    lv::Entity entity(g_game->scene().addEntity(), g_game->scene().registry);
    entity.getComponent<lv::TagComponent>().name = "plane";
    entity.addComponent<lv::TransformComponent>();
    lv::MeshComponent& meshComponent = entity.addComponent<lv::MeshComponent>(
#ifdef LV_BACKEND_VULKAN
        g_game->deferredLayout
#endif
    );
    meshComponent.createPlane();
    for (uint8_t i = 0; i < 3; i++) {
        meshComponent.addTexture(nullptr, i);
    }
    entity.addComponent<lv::MaterialComponent>(
#ifdef LV_BACKEND_VULKAN
        g_game->deferredLayout
#endif
    );
}
