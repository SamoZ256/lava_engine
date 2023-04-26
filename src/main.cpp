#include <random>
#include <array>
#include <iostream>

#define LVND_DEBUG
#ifdef LV_BACKEND_VULKAN
#define LVND_BACKEND_VULKAN
#elif defined(LV_BACKEND_METAL)
#define LVND_BACKEND_METAL
#elif defined(LV_BACKEND_OPENGL)
#define LVND_BACKEND_OPENGL
#endif
#include "lvnd/lvnd.h"

#include "lvcore/core/shader_module.hpp"
#include "lvcore/core/command_buffer.hpp"
#include "lvcore/core/semaphore.hpp"
#include "lvcore/core/framebuffer.hpp"
#include "lvcore/core/viewport.hpp"

#include "lvutils/camera/arcball_camera.hpp"

#include "lvutils/entity/mesh_loader.hpp"
#include "lvutils/entity/transform.hpp"
#include "lvutils/entity/material.hpp"

#include "lvutils/light/direct_light.hpp"

#include "lvutils/skylight/skylight.hpp"

#include "lvutils/math/math.hpp"

#include "editor/editor.hpp"

#define GET_SHADER_FILENAME(name) "assets/shaders/compiled/" name ".json"

//Callbacks
void windowResizeCallback(LvndWindow* window, uint16_t width, uint16_t height);

void scrollCallback(LvndWindow* window, double xoffset, double yoffset);

//Functions

//Shadows
std::vector<glm::vec4> getFrustumCorners(const glm::mat4& proj, const glm::mat4& view);

void getLightMatrix(glm::vec3 lightDir, float nearPlane, float farPlane, uint8_t index);

void getLightMatrices(glm::vec3 lightDir);

//Menu bar actions
void saveGame();

void newEntityCreateEmpty();

void newEntityLoadModel();

void newEntityPlane();

void duplicateEntity();

//Macros

//Shadows
#define SHADOW_MAP_SIZE 2048
#define CASCADE_COUNT 3
#define SHADOW_FAR_PLANE 64.0f
#define SHADOW_FRAME_COUNT 4

//AO
#define AO_NOISE_TEX_SIZE 8
#define SSAO_KERNEL_SIZE 24

//Bloom
#define BLOOM_MIP_COUNT 7

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

struct PCSsaoVP {
    glm::mat4 projection;
	glm::mat4 view;
    glm::mat4 invViewProj;
};

//Render passes

//Shadow render pass
struct ShadowRenderPass {
    lv::Subpass subpass;
    lv::RenderPass renderPass;
    lv::Framebuffer framebuffers[CASCADE_COUNT];
    lv::CommandBuffer commandBuffer;

    lv::Image depthImage;
    lv::Image depthImageViews[CASCADE_COUNT];
    lv::Sampler depthSampler;
};

//Main render pass
struct MainRenderPass {
    lv::Subpass deferredSubpass;
    lv::Subpass mainSubpass;
    lv::RenderPass renderPass;
    lv::Framebuffer framebuffer;
    lv::CommandBuffer commandBuffer;

    lv::Image normalRoughnessImage;
    lv::Sampler normalRoughnessSampler;

    lv::Image albedoMetallicImage;
    lv::Sampler albedoMetallicSampler;

    lv::Image depthImage;
    lv::Sampler depthSampler;

    lv::Image colorImage;
    lv::Sampler colorSampler;

    lv::Image halfDepthImage;
    lv::Sampler halfDepthSampler;

#ifdef LV_BACKEND_METAL
    lv::Image depthAsColorImage;
#endif
};

#define SETUP_MAIN_0_DESCRIPTORS \
mainDescriptorSet0.addBinding(directLight.lightUniformBuffer.descriptorInfo(), 0); \
mainDescriptorSet0.addBinding(mainVPUniformBuffer.descriptorInfo(), 1); \
mainDescriptorSet0.addBinding(mainShadowUniformBuffer.descriptorInfo(), 2); \
mainDescriptorSet0.addBinding(shadowRenderPass.depthSampler.descriptorInfo(shadowRenderPass.depthImage), 3); \

#define SETUP_MAIN_1_DESCRIPTORS \
mainDescriptorSet1.addBinding(skylight.sampler.descriptorInfo(skylight.irradianceMapImage), 0); \
mainDescriptorSet1.addBinding(skylight.prefilteredMapSampler.descriptorInfo(skylight.prefilteredMapImage), 1); \
mainDescriptorSet1.addBinding(brdfLutSampler.descriptorInfo(brdfLutTexture.image), 2);

//SSAO render pass
struct SSAORenderPass {
    lv::Subpass subpass;
    lv::RenderPass renderPass;
    lv::Framebuffer framebuffer;
    lv::CommandBuffer commandBuffer;

    lv::Image colorImage;
    lv::Sampler colorSampler;
};

#define SETUP_SSAO_DESCRIPTORS \
ssaoDescriptorSet.addBinding(mainRenderPass.halfDepthSampler.descriptorInfo(mainRenderPass.halfDepthImage, LV_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL), 0); /*disasmComputePass.outputColorAttachmentSampler.descriptorInfo(disasmComputePass.outputColorAttachmentView)*/ \
ssaoDescriptorSet.addBinding(mainRenderPass.normalRoughnessSampler.descriptorInfo(mainRenderPass.normalRoughnessImage), 1); \
ssaoDescriptorSet.addBinding(aoNoiseSampler.descriptorInfo(aoNoiseTex.image), 2);

//SSAO blur render pass
struct SSAOBlurRenderPass {
    lv::Subpass subpass;
    lv::RenderPass renderPass;
    lv::Framebuffer framebuffer;
    lv::CommandBuffer commandBuffer;

    lv::Image colorImage;
    lv::Sampler colorSampler;
};

#define SETUP_SSAO_BLUR_DESCRIPTORS \
ssaoBlurDescriptorSet.addBinding(ssaoRenderPass.colorSampler.descriptorInfo(ssaoRenderPass.colorImage), 0);

//Bloom render pass
struct BloomRenderPass {
    lv::Subpass subpass;
    lv::RenderPass renderPasses[2];
    lv::Framebuffer downsampleFramebuffers[BLOOM_MIP_COUNT];
    lv::Framebuffer upsampleFramebuffers[BLOOM_MIP_COUNT - 1];
    lv::CommandBuffer downsampleCommandBuffer;
    lv::CommandBuffer upsampleCommandBuffer;

    lv::Viewport viewports[BLOOM_MIP_COUNT];

    lv::Image colorImages[BLOOM_MIP_COUNT];

    lv::Sampler downsampleSampler;
    lv::Sampler upsampleSampler;
};

//HDR render pass
struct HdrRenderPass {
    lv::Subpass subpass;
    lv::RenderPass renderPass;
    lv::Framebuffer framebuffer;
    lv::CommandBuffer commandBuffer;

    lv::Image colorImage;
    lv::Sampler colorSampler;
};

#define SETUP_HDR_DESCRIPTORS \
hdrDescriptorSet.addBinding(mainRenderPass.colorSampler.descriptorInfo(mainRenderPass.colorImage), 0); \
hdrDescriptorSet.addBinding(bloomRenderPass.downsampleSampler.descriptorInfo(bloomRenderPass.colorImages[0]), 1); \
hdrDescriptorSet.addBinding(ssaoBlurRenderPass.colorSampler.descriptorInfo(ssaoBlurRenderPass.colorImage), 2);

/*
struct DisasmComputePass {
    lv::Image outputColorAttachment;
    lv::Sampler outputColorAttachmentSampler;
};
*/

//Shadows
const float cascadeLevels[CASCADE_COUNT] = {
    SHADOW_FAR_PLANE * 0.08f, SHADOW_FAR_PLANE * 0.22f, SHADOW_FAR_PLANE
};

glm::mat4 shadowVPs[CASCADE_COUNT];
bool updateShadowFrames[CASCADE_COUNT];

uint8_t shadowFrameCounter = 0;
const uint8_t shadowRefreshFrames[CASCADE_COUNT] = {
    2, 4, 4
};

const uint8_t shadowStartingFrames[CASCADE_COUNT] = {
    0, 1, 3
};

//Time
float lastTime = 0.0f;

//Window
bool windowResized = false;

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

    lvndMenuAddSeparator(entityMenu);

    LvndMenuItem* entityDuplicateMenuItem = lvndCreateMenuItem("Duplicate", duplicateEntity, "d");
    lvndMenuAddMenuItem(entityMenu, entityDuplicateMenuItem);

    /*
    lvndMenuAddSeparator(entityMenu);
    LvndMenuItem* fooMenuItem = lvndCreateMenuItem("Foo", nullptr, "");
    lvndMenuAddMenuItem(entityMenu, fooMenuItem);
    */

    lvndSetGlobalMenuBar(menuBar);

    lvndInit();

    //srand((unsigned)time(0));
    lvndSetWindowParamValue(LVND_WINDOW_PARAM_OPENGL_VERSION_MAJOR, 4);
    lvndSetWindowParamValue(LVND_WINDOW_PARAM_OPENGL_VERSION_MINOR, 1);
    LvndWindow* window = lvndCreateWindow(SRC_WIDTH, SRC_HEIGHT, SRC_TITLE);

    //Callbacks
    lvndSetWindowResizeCallback(window, windowResizeCallback);
    lvndSetScrollCallback(window, scrollCallback);

    lv::ThreadPoolCreateInfo threadPoolCreateInfo;
    lv::ThreadPool threadPool;
    threadPool.init(threadPoolCreateInfo);

    lv::InstanceCreateInfo instanceCreateInfo;
	instanceCreateInfo.applicationName = "Lava Engine";
	instanceCreateInfo.validationEnable = true;
	lv::Instance instance;
    instance.init(instanceCreateInfo);

	lv::DeviceCreateInfo deviceCreateInfo;
	deviceCreateInfo.window = window;
    deviceCreateInfo.threadPool = &threadPool;
	deviceCreateInfo.descriptorPoolSizes[LV_DESCRIPTOR_TYPE_UNIFORM_BUFFER] = 128 * 3;
	deviceCreateInfo.descriptorPoolSizes[LV_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER] = 512 * 3;
    deviceCreateInfo.descriptorPoolSizes[LV_DESCRIPTOR_TYPE_INPUT_ATTACHMENT] = 8 * 3;
	lv::Device device;
    device.init(deviceCreateInfo);

	lv::SwapChainCreateInfo swapChainCreateInfo;
	swapChainCreateInfo.window = window;
	swapChainCreateInfo.vsyncEnabled = true;
    swapChainCreateInfo.maxFramesInFlight = 3;
	lv::SwapChain swapChain;
    swapChain.init(swapChainCreateInfo);
    
    uint8_t maxUint8 = std::numeric_limits<uint8_t>::max();

    glm::u8vec3 neutralColor(maxUint8, maxUint8, maxUint8);
    lv::MeshComponent::neutralTexture.image.format = LV_FORMAT_R8G8B8A8_UNORM;
    lv::MeshComponent::neutralTexture.image.usage = LV_IMAGE_USAGE_SAMPLED_BIT | LV_IMAGE_USAGE_TRANSFER_DST_BIT;
    lv::MeshComponent::neutralTexture.image.init(1, 1);
    lv::MeshComponent::neutralTexture.image.copyDataTo(0, &neutralColor);
    
    glm::u8vec3 normalNeutralColor(maxUint8 / 2, maxUint8 / 2, maxUint8);
    lv::MeshComponent::normalNeutralTexture.image.format = LV_FORMAT_R8G8B8A8_UNORM;
    lv::MeshComponent::normalNeutralTexture.image.usage = LV_IMAGE_USAGE_SAMPLED_BIT | LV_IMAGE_USAGE_TRANSFER_DST_BIT;
    lv::MeshComponent::normalNeutralTexture.image.init(1, 1);
    lv::MeshComponent::normalNeutralTexture.image.copyDataTo(0, &normalNeutralColor);

    //Equirectangular to cubemap shader
    lv::PipelineLayout equiToCubeLayout;
    equiToCubeLayout.descriptorSetLayouts.resize(1);

	equiToCubeLayout.descriptorSetLayouts[0].addBinding(0, LV_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LV_SHADER_STAGE_VERTEX_BIT | LV_SHADER_STAGE_FRAGMENT_BIT); //View projection
	equiToCubeLayout.descriptorSetLayouts[0].addBinding(1, LV_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Environment map

    equiToCubeLayout.pushConstantRanges.resize(1);
	equiToCubeLayout.pushConstantRanges[0].stageFlags = LV_SHADER_STAGE_VERTEX_BIT | LV_SHADER_STAGE_FRAGMENT_BIT;
	equiToCubeLayout.pushConstantRanges[0].offset = 0;
	equiToCubeLayout.pushConstantRanges[0].size = sizeof(lv::UBOCubemapVP);

    equiToCubeLayout.init();

    //Irradiance shader
    lv::PipelineLayout irradianceLayout;
    irradianceLayout.descriptorSetLayouts.resize(1);

	irradianceLayout.descriptorSetLayouts[0].addBinding(0, LV_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LV_SHADER_STAGE_VERTEX_BIT | LV_SHADER_STAGE_FRAGMENT_BIT); //View projection
	irradianceLayout.descriptorSetLayouts[0].addBinding(1, LV_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Environment map

    irradianceLayout.init();

    //Prefiltered shader
    lv::PipelineLayout prefilterLayout;
    prefilterLayout.descriptorSetLayouts.resize(1);

	prefilterLayout.descriptorSetLayouts[0].addBinding(0, LV_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LV_SHADER_STAGE_VERTEX_BIT | LV_SHADER_STAGE_FRAGMENT_BIT); //View projection
	prefilterLayout.descriptorSetLayouts[0].addBinding(1, LV_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Environment map

    prefilterLayout.pushConstantRanges.resize(1);
	prefilterLayout.pushConstantRanges[0].stageFlags = LV_SHADER_STAGE_VERTEX_BIT | LV_SHADER_STAGE_FRAGMENT_BIT;
	prefilterLayout.pushConstantRanges[0].offset = 0;
	prefilterLayout.pushConstantRanges[0].size = sizeof(float);

    prefilterLayout.init();

    //Deferred shader
    lv::PipelineLayout deferredLayout;
    deferredLayout.descriptorSetLayouts.resize(3);

	deferredLayout.descriptorSetLayouts[0].addBinding(0, LV_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LV_SHADER_STAGE_VERTEX_BIT); //View projection

	deferredLayout.descriptorSetLayouts[1].addBinding(0, LV_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LV_SHADER_STAGE_FRAGMENT_BIT); //Material

	deferredLayout.descriptorSetLayouts[2].addBinding(0, LV_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Albedo texture
	deferredLayout.descriptorSetLayouts[2].addBinding(1, LV_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Metallic roughness texture
	deferredLayout.descriptorSetLayouts[2].addBinding(2, LV_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Normal texture

    deferredLayout.pushConstantRanges.resize(1);
	deferredLayout.pushConstantRanges[0].stageFlags = LV_SHADER_STAGE_VERTEX_BIT;
	deferredLayout.pushConstantRanges[0].offset = 0;
	deferredLayout.pushConstantRanges[0].size = sizeof(lv::PushConstantModel);

    deferredLayout.init();

    //Shadow shader
    lv::PipelineLayout shadowLayout;
    shadowLayout.descriptorSetLayouts.resize(1);

	shadowLayout.descriptorSetLayouts[0].addBinding(0, LV_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LV_SHADER_STAGE_VERTEX_BIT); //View projection

    shadowLayout.pushConstantRanges.resize(1);
	shadowLayout.pushConstantRanges[0].stageFlags = LV_SHADER_STAGE_VERTEX_BIT;
	shadowLayout.pushConstantRanges[0].offset = 0;
	shadowLayout.pushConstantRanges[0].size = sizeof(glm::mat4);

    shadowLayout.init();

    //SSAO shader
    lv::PipelineLayout ssaoLayout;
    ssaoLayout.descriptorSetLayouts.resize(1);

	ssaoLayout.descriptorSetLayouts[0].addBinding(0, LV_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Position depth
	ssaoLayout.descriptorSetLayouts[0].addBinding(1, LV_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Normal roughness
    ssaoLayout.descriptorSetLayouts[0].addBinding(2, LV_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //SSAO noise

    ssaoLayout.pushConstantRanges.resize(1);
	ssaoLayout.pushConstantRanges[0].stageFlags = LV_SHADER_STAGE_FRAGMENT_BIT;
	ssaoLayout.pushConstantRanges[0].offset = 0;
	ssaoLayout.pushConstantRanges[0].size = sizeof(PCSsaoVP);

    ssaoLayout.init();

    //SSAO blur shader
    lv::PipelineLayout ssaoBlurLayout;
    ssaoBlurLayout.descriptorSetLayouts.resize(1);

	ssaoBlurLayout.descriptorSetLayouts[0].addBinding(0, LV_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //SSAO attachment

    ssaoBlurLayout.init();

    //Skylight shader
    lv::PipelineLayout skylightLayout;
    skylightLayout.descriptorSetLayouts.resize(1);

	skylightLayout.descriptorSetLayouts[0].addBinding(0, LV_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Environment map

    skylightLayout.pushConstantRanges.resize(1);
	skylightLayout.pushConstantRanges[0].stageFlags = LV_SHADER_STAGE_VERTEX_BIT;
	skylightLayout.pushConstantRanges[0].offset = 0;
	skylightLayout.pushConstantRanges[0].size = sizeof(glm::mat4);

    skylightLayout.init();

    //Main shader
    lv::PipelineLayout mainLayout;
    mainLayout.descriptorSetLayouts.resize(3);

	mainLayout.descriptorSetLayouts[0].addBinding(0, LV_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LV_SHADER_STAGE_FRAGMENT_BIT); //Light
    mainLayout.descriptorSetLayouts[0].addBinding(1, LV_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LV_SHADER_STAGE_FRAGMENT_BIT); //Inverse view projection
	mainLayout.descriptorSetLayouts[0].addBinding(2, LV_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LV_SHADER_STAGE_FRAGMENT_BIT); //Shadow
    mainLayout.descriptorSetLayouts[0].addBinding(3, LV_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Shadow map

    mainLayout.descriptorSetLayouts[1].addBinding(0, LV_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Irradiance map
    mainLayout.descriptorSetLayouts[1].addBinding(1, LV_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Prefiltered map
    mainLayout.descriptorSetLayouts[1].addBinding(2, LV_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //BRDF Lut map

    mainLayout.descriptorSetLayouts[2].addBinding(0, LV_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, LV_SHADER_STAGE_FRAGMENT_BIT); //Depth
    mainLayout.descriptorSetLayouts[2].addBinding(1, LV_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, LV_SHADER_STAGE_FRAGMENT_BIT); //Normal roughness
    mainLayout.descriptorSetLayouts[2].addBinding(2, LV_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, LV_SHADER_STAGE_FRAGMENT_BIT); //Albedo metallic

    mainLayout.init();

    //Point light shader
    /*
    lv::PipelineLayout pointLightLayout;
    pointLightLayout.descriptorSetLayouts.resize(1);

	pointLightLayout.descriptorSetLayouts[0].addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LV_SHADER_STAGE_VERTEX_BIT); //View projection
    pointLightLayout.descriptorSetLayouts[0].addBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LV_SHADER_STAGE_FRAGMENT_BIT); //Inverse view projection
    pointLightLayout.descriptorSetLayouts[0].addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Depth
    pointLightLayout.descriptorSetLayouts[0].addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Normal roughness
    pointLightLayout.descriptorSetLayouts[0].addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Albedo metallic

    pointLightLayout.pushConstantRanges.resize(1);
	pointLightLayout.pushConstantRanges[0].stageFlags = LV_SHADER_STAGE_VERTEX_BIT;
	pointLightLayout.pushConstantRanges[0].offset = 0;
	pointLightLayout.pushConstantRanges[0].size = sizeof(glm::vec3);

    pointLightLayout.init();
    */

    //Bloom shader
    lv::PipelineLayout bloomLayout;
    bloomLayout.descriptorSetLayouts.resize(1);

    bloomLayout.descriptorSetLayouts[0].addBinding(0, LV_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Source

    bloomLayout.init();

    //HDR shader
    lv::PipelineLayout hdrLayout;
    hdrLayout.descriptorSetLayouts.resize(1);

	hdrLayout.descriptorSetLayouts[0].addBinding(0, LV_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Input image
	hdrLayout.descriptorSetLayouts[0].addBinding(1, LV_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //Bloom image
    hdrLayout.descriptorSetLayouts[0].addBinding(2, LV_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, LV_SHADER_STAGE_FRAGMENT_BIT); //SSAO

    hdrLayout.init();

    //Disassemble depth shader
    /*
    lv::PipelineLayout disasmLayout;
    disasmLayout.descriptorSetLayouts.resize(1);

    disasmLayout.descriptorSetLayouts[0].addBinding(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, LV_SHADER_STAGE_COMPUTE_BIT); //Input depth
    disasmLayout.descriptorSetLayouts[0].addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, LV_SHADER_STAGE_COMPUTE_BIT); //Output depth

    disasmLayout.init();
    */

    //Game
    Game game(deferredLayout);
    game.loadFromFile();

    //Render passes

    //Skylight bake render pass
#pragma region SKYLIGHT_BAKE_RENDER_PASS
    lv::Subpass skylightBakeSubpass;
    lv::RenderPass skylightBakeRenderPass;

    skylightBakeSubpass.addColorAttachment({
        .index = 0,
        .layout = LV_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    });

    skylightBakeRenderPass.addSubpass(&skylightBakeSubpass);
    
    skylightBakeRenderPass.addAttachment({
        .format = LV_FORMAT_R8G8B8A8_UNORM,
        .index = 0,
        .finalLayout = LV_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    });

    skylightBakeRenderPass.init();
#pragma endregion SKYLIGHT_BAKE_RENDER_PASS

    //Shadow render pass
#pragma region SHADOW_RENDER_PASS
    ShadowRenderPass shadowRenderPass{};
    shadowRenderPass.depthImage.usage = LV_IMAGE_USAGE_SAMPLED_BIT | LV_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    shadowRenderPass.depthImage.format = swapChain.depthFormat;
    shadowRenderPass.depthImage.layerCount = CASCADE_COUNT;
    shadowRenderPass.depthImage.aspectMask = LV_IMAGE_ASPECT_DEPTH_BIT;
    shadowRenderPass.depthImage.viewType = LV_IMAGE_VIEW_TYPE_2D_ARRAY;
    shadowRenderPass.depthImage.init(SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
    for (uint8_t i = 0; i < CASCADE_COUNT; i++)
        shadowRenderPass.depthImage.newImageView(shadowRenderPass.depthImageViews[i], LV_IMAGE_VIEW_TYPE_2D, i, 1, 0, 1);
    shadowRenderPass.depthSampler.filter = LV_FILTER_LINEAR;
    shadowRenderPass.depthSampler.compareEnable = LV_TRUE;
    shadowRenderPass.depthSampler.compareOp = LV_COMPARE_OP_LESS;
    shadowRenderPass.depthSampler.init();

    shadowRenderPass.subpass.setDepthAttachment({
        .index = 0,
        .layout = LV_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    });

    shadowRenderPass.renderPass.addSubpass(&shadowRenderPass.subpass);

    shadowRenderPass.renderPass.addAttachment({
        .format = shadowRenderPass.depthImage.format,
        .index = 0,
        .loadOp = LV_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = LV_ATTACHMENT_STORE_OP_STORE,
        .finalLayout = LV_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    });

    shadowRenderPass.renderPass.init();

    for (uint8_t i = 0; i < CASCADE_COUNT; i++) {
        shadowRenderPass.framebuffers[i].setDepthAttachment({
            .image = &shadowRenderPass.depthImageViews[i],
            .index = 0
        });

        shadowRenderPass.framebuffers[i].init(&shadowRenderPass.renderPass);
    }
    shadowRenderPass.commandBuffer.init();
#pragma endregion SHADOW_RENDER_PASS

    //Deferred render pass
#pragma region DEFERRED_RENDER_PASS
    MainRenderPass mainRenderPass;
    mainRenderPass.colorImage.usage = LV_IMAGE_USAGE_SAMPLED_BIT | LV_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    mainRenderPass.colorImage.format = LV_FORMAT_R16G16B16A16_UNORM;
    mainRenderPass.colorImage.aspectMask = LV_IMAGE_ASPECT_COLOR_BIT;
    mainRenderPass.colorImage.init(SRC_WIDTH, SRC_HEIGHT);
    mainRenderPass.colorSampler.init();

    mainRenderPass.normalRoughnessImage.usage = LV_IMAGE_USAGE_SAMPLED_BIT | LV_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | LV_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    mainRenderPass.normalRoughnessImage.format = LV_FORMAT_R16G16B16A16_SNORM;
    mainRenderPass.normalRoughnessImage.aspectMask = LV_IMAGE_ASPECT_COLOR_BIT;
    mainRenderPass.normalRoughnessImage.init(SRC_WIDTH, SRC_HEIGHT);
    mainRenderPass.normalRoughnessSampler.init();

    mainRenderPass.albedoMetallicImage.usage = LV_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | LV_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | LV_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    mainRenderPass.albedoMetallicImage.format = LV_FORMAT_R16G16B16A16_UNORM;
    //mainRenderPass.albedoMetallicImage.memoryType = LV_MEMORY_TYPE_MEMORYLESS;
    mainRenderPass.albedoMetallicImage.aspectMask = LV_IMAGE_ASPECT_COLOR_BIT;
    mainRenderPass.albedoMetallicImage.init(SRC_WIDTH, SRC_HEIGHT);
    mainRenderPass.albedoMetallicSampler.init();

    mainRenderPass.depthImage.usage = LV_IMAGE_USAGE_SAMPLED_BIT | LV_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | LV_IMAGE_USAGE_TRANSFER_SRC_BIT | LV_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    mainRenderPass.depthImage.format = swapChain.depthFormat;
    mainRenderPass.depthImage.aspectMask = LV_IMAGE_ASPECT_DEPTH_BIT;
    mainRenderPass.depthImage.init(SRC_WIDTH, SRC_HEIGHT);
    mainRenderPass.depthSampler.init();

    mainRenderPass.halfDepthImage.usage = LV_IMAGE_USAGE_SAMPLED_BIT | LV_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | LV_IMAGE_USAGE_TRANSFER_DST_BIT;
    mainRenderPass.halfDepthImage.format = swapChain.depthFormat;
    mainRenderPass.halfDepthImage.aspectMask = LV_IMAGE_ASPECT_DEPTH_BIT;
    mainRenderPass.halfDepthImage.init(SRC_WIDTH / 2, SRC_HEIGHT / 2);
    mainRenderPass.halfDepthSampler.init();

#ifdef LV_BACKEND_METAL
    mainRenderPass.depthAsColorImage.usage = LV_IMAGE_USAGE_SAMPLED_BIT | LV_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | LV_IMAGE_USAGE_TRANSFER_SRC_BIT | LV_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    mainRenderPass.depthAsColorImage.format = LV_FORMAT_R32_SFLOAT;
    mainRenderPass.depthAsColorImage.aspectMask = LV_IMAGE_ASPECT_COLOR_BIT;
    mainRenderPass.depthAsColorImage.memoryType = LV_MEMORY_TYPE_MEMORYLESS;
    mainRenderPass.depthAsColorImage.init(SRC_WIDTH, SRC_HEIGHT);
#endif

    mainRenderPass.deferredSubpass.addColorAttachment({
        .index = 1,
        .layout = LV_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    });

    mainRenderPass.deferredSubpass.addColorAttachment({
        .index = 2,
        .layout = LV_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    });

    mainRenderPass.deferredSubpass.setDepthAttachment({
        .index = 3,
        .layout = LV_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    });

#ifdef LV_BACKEND_METAL
    mainRenderPass.deferredSubpass.addColorAttachment({
        .index = 4,
        .layout = LV_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    });
#endif

    mainRenderPass.renderPass.addSubpass(&mainRenderPass.deferredSubpass);

    mainRenderPass.mainSubpass.addColorAttachment({
        .index = 0,
        .layout = LV_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    });

    mainRenderPass.mainSubpass.addInputAttachment({
        .index = 1,
        .layout = LV_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    });

    mainRenderPass.mainSubpass.addInputAttachment({
        .index = 2,
        .layout = LV_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    });

    mainRenderPass.mainSubpass.addInputAttachment({
        .index = 3,
        .layout = LV_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
    });

    mainRenderPass.mainSubpass.setDepthAttachment({
        .index = 3,
        .layout = LV_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
    });

#ifdef LV_BACKEND_METAL
    mainRenderPass.mainSubpass.addInputAttachment({
        .index = 4,
        .layout = LV_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
    });
#endif

    mainRenderPass.renderPass.addSubpass(&mainRenderPass.mainSubpass);

    mainRenderPass.renderPass.addAttachment({
        .format = mainRenderPass.colorImage.format,
        .index = 0,
        .storeOp = LV_ATTACHMENT_STORE_OP_STORE,
        .finalLayout = LV_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    });
    mainRenderPass.renderPass.addAttachment({
        .format = mainRenderPass.normalRoughnessImage.format,
        .index = 1,
        .storeOp = LV_ATTACHMENT_STORE_OP_STORE,
        .finalLayout = LV_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    });
    mainRenderPass.renderPass.addAttachment({
        .format = mainRenderPass.albedoMetallicImage.format,
        .index = 2,
#ifdef LV_BACKEND_METAL
        .storeOp = LV_ATTACHMENT_STORE_OP_STORE, //TODO: remove this
#endif
        .finalLayout = LV_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    });
    mainRenderPass.renderPass.addAttachment({
        .format = mainRenderPass.depthImage.format,
        .index = 3,
        .loadOp = LV_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = LV_ATTACHMENT_STORE_OP_STORE,
        .initialLayout = LV_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        .finalLayout = LV_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
    });
#ifdef LV_BACKEND_METAL
    mainRenderPass.renderPass.addAttachment({
        .format = mainRenderPass.depthAsColorImage.format,
        .index = 4,
        .finalLayout = LV_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    });
#endif

#ifdef LV_BACKEND_VULKAN
    //Dependencies
    mainRenderPass.renderPass.dependencies.resize(4);

    mainRenderPass.renderPass.dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    mainRenderPass.renderPass.dependencies[0].dstSubpass = 0;
    mainRenderPass.renderPass.dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    mainRenderPass.renderPass.dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    mainRenderPass.renderPass.dependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    mainRenderPass.renderPass.dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    mainRenderPass.renderPass.dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    mainRenderPass.renderPass.dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
    mainRenderPass.renderPass.dependencies[1].dstSubpass = 0;
    mainRenderPass.renderPass.dependencies[1].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    mainRenderPass.renderPass.dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    mainRenderPass.renderPass.dependencies[1].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    mainRenderPass.renderPass.dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    mainRenderPass.renderPass.dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // This dependency transitions the input attachment from color attachment to shader read
    mainRenderPass.renderPass.dependencies[2].srcSubpass = 0;
    mainRenderPass.renderPass.dependencies[2].dstSubpass = 1;
    mainRenderPass.renderPass.dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    mainRenderPass.renderPass.dependencies[2].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    mainRenderPass.renderPass.dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    mainRenderPass.renderPass.dependencies[2].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    mainRenderPass.renderPass.dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    mainRenderPass.renderPass.dependencies[3].srcSubpass = 1;
    mainRenderPass.renderPass.dependencies[3].dstSubpass = VK_SUBPASS_EXTERNAL;
    mainRenderPass.renderPass.dependencies[3].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    mainRenderPass.renderPass.dependencies[3].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    mainRenderPass.renderPass.dependencies[3].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    mainRenderPass.renderPass.dependencies[3].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    mainRenderPass.renderPass.dependencies[3].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
#endif

    mainRenderPass.renderPass.init();

    mainRenderPass.framebuffer.addColorAttachment({
        .image = &mainRenderPass.colorImage,
        .index = 0
    });
    mainRenderPass.framebuffer.addColorAttachment({
        .image = &mainRenderPass.normalRoughnessImage,
        .index = 1
    });
    mainRenderPass.framebuffer.addColorAttachment({
        .image = &mainRenderPass.albedoMetallicImage,
        .index = 2
    });
    mainRenderPass.framebuffer.setDepthAttachment({
        .image = &mainRenderPass.depthImage,
        .index = 3
    });
#ifdef LV_BACKEND_METAL
    mainRenderPass.framebuffer.addColorAttachment({
        .image = &mainRenderPass.depthAsColorImage,
        .index = 4
    });
#endif

    mainRenderPass.framebuffer.init(&mainRenderPass.renderPass);
    mainRenderPass.commandBuffer.init();
#pragma endregion DEFERRED_RENDER_PASS

    //SSAO render pass
#pragma region SSAO_RENDER_PASS
    SSAORenderPass ssaoRenderPass{};
    ssaoRenderPass.colorImage.usage |= LV_IMAGE_USAGE_SAMPLED_BIT | LV_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ssaoRenderPass.colorImage.format = LV_FORMAT_R8_UNORM;
    ssaoRenderPass.colorImage.aspectMask = LV_IMAGE_ASPECT_COLOR_BIT;
    ssaoRenderPass.colorImage.init(SRC_WIDTH / 2, SRC_HEIGHT / 2);
    ssaoRenderPass.colorSampler.filter = LV_FILTER_LINEAR;
    ssaoRenderPass.colorSampler.init();

    ssaoRenderPass.subpass.addColorAttachment({
        .index = 0,
        .layout = LV_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    });

    ssaoRenderPass.subpass.setDepthAttachment({
        .index = 1,
        .layout = LV_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
    });

    ssaoRenderPass.renderPass.addSubpass(&ssaoRenderPass.subpass);

    ssaoRenderPass.renderPass.addAttachment({
        .format = ssaoRenderPass.colorImage.format,
        .index = 0,
        .storeOp = LV_ATTACHMENT_STORE_OP_STORE,
        .finalLayout = LV_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    });

    ssaoRenderPass.renderPass.addAttachment({
        .format = mainRenderPass.halfDepthImage.format,
        .index = 1,
        .loadOp = LV_ATTACHMENT_LOAD_OP_LOAD,
        .initialLayout = LV_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .finalLayout = LV_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
    });

    ssaoRenderPass.renderPass.init();

    ssaoRenderPass.framebuffer.addColorAttachment({
        .image = &ssaoRenderPass.colorImage,
        .index = 0
    });

    ssaoRenderPass.framebuffer.setDepthAttachment({
        .image = &mainRenderPass.halfDepthImage,
        .index = 1
    });

    ssaoRenderPass.framebuffer.init(&ssaoRenderPass.renderPass);
    ssaoRenderPass.commandBuffer.init();
#pragma endregion SSAO_RENDER_PASS

    //SSAO blur render pass
#pragma region SSAO_BLUR_RENDER_PASS
    SSAOBlurRenderPass ssaoBlurRenderPass{};
    ssaoBlurRenderPass.colorImage.usage |= LV_IMAGE_USAGE_SAMPLED_BIT | LV_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ssaoBlurRenderPass.colorImage.format = LV_FORMAT_R8_UNORM;
    ssaoBlurRenderPass.colorImage.aspectMask = LV_IMAGE_ASPECT_COLOR_BIT;
    ssaoBlurRenderPass.colorImage.init(SRC_WIDTH, SRC_HEIGHT);
    ssaoBlurRenderPass.colorSampler.init();

    ssaoBlurRenderPass.subpass.addColorAttachment({
        .index = 0,
        .layout = LV_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    });

    ssaoBlurRenderPass.subpass.setDepthAttachment({
        .index = 1,
        .layout = LV_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
    });

    ssaoBlurRenderPass.renderPass.addSubpass(&ssaoBlurRenderPass.subpass);

    ssaoBlurRenderPass.renderPass.addAttachment({
        .format = ssaoBlurRenderPass.colorImage.format,
        .index = 0,
        .storeOp = LV_ATTACHMENT_STORE_OP_STORE,
        .finalLayout = LV_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .loadOp = LV_ATTACHMENT_LOAD_OP_CLEAR
    });

    ssaoBlurRenderPass.renderPass.addAttachment({
        .format = mainRenderPass.depthImage.format,
        .index = 1,
        .loadOp = LV_ATTACHMENT_LOAD_OP_LOAD,
        .initialLayout = LV_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .finalLayout = LV_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
    });

    ssaoBlurRenderPass.renderPass.init();

    ssaoBlurRenderPass.framebuffer.addColorAttachment({
        .image = &ssaoBlurRenderPass.colorImage,
        .index = 0,
        .clearValue = {
            .color = {1.0f, 1.0f, 1.0f, 1.0f}
        }
    });

    ssaoBlurRenderPass.framebuffer.setDepthAttachment({
        .image = &mainRenderPass.depthImage,
        .index = 1
    });

    ssaoBlurRenderPass.framebuffer.init(&ssaoBlurRenderPass.renderPass);
    ssaoBlurRenderPass.commandBuffer.init();
#pragma endregion SSAO_BLUR_RENDER_PASS

    //Downsample render pass
#pragma region DOWNSAMPLE_RENDER_PASS
    BloomRenderPass bloomRenderPass{};
    uint16_t bloomMipWidth = SRC_WIDTH, bloomMipHeight = SRC_HEIGHT;
    for (uint8_t i = 0; i < BLOOM_MIP_COUNT; i++) {
        bloomRenderPass.colorImages[i].usage |= LV_IMAGE_USAGE_SAMPLED_BIT | LV_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        bloomRenderPass.colorImages[i].format = LV_FORMAT_B10R11G11_UFLOAT;
        bloomRenderPass.colorImages[i].aspectMask = LV_IMAGE_ASPECT_COLOR_BIT;
        bloomRenderPass.colorImages[i].init(bloomMipWidth, bloomMipHeight);

        bloomRenderPass.viewports[i].setViewport(0, 0, bloomMipWidth, bloomMipHeight);

        bloomMipWidth /= 2;
        bloomMipHeight /= 2;
    }
    bloomRenderPass.downsampleSampler.init();
    bloomRenderPass.upsampleSampler.filter = LV_FILTER_LINEAR;
    bloomRenderPass.upsampleSampler.init();

    bloomRenderPass.subpass.addColorAttachment({
        .index = 0,
        .layout = LV_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    });

    for (uint8_t i = 0; i < 2; i++) {
        bloomRenderPass.renderPasses[i].addSubpass(&bloomRenderPass.subpass);

        bloomRenderPass.renderPasses[i].addAttachment({
            .format = bloomRenderPass.colorImages[0].format,
            .index = 0,
            .loadOp = LvAttachmentLoadOp(i == 0 ? LV_ATTACHMENT_LOAD_OP_DONT_CARE : LV_ATTACHMENT_LOAD_OP_LOAD),
            .storeOp = LV_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = LV_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .finalLayout = LV_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        });

        bloomRenderPass.renderPasses[i].init();
    }

    for (uint8_t i = 0; i < BLOOM_MIP_COUNT; i++) {
        bloomRenderPass.downsampleFramebuffers[i].addColorAttachment({
            .image = &bloomRenderPass.colorImages[i],
            .index = 0
        });

        bloomRenderPass.downsampleFramebuffers[i].init(&bloomRenderPass.renderPasses[0]);
    }

    for (uint8_t i = 0; i < BLOOM_MIP_COUNT - 1; i++) {
        bloomRenderPass.upsampleFramebuffers[i].addColorAttachment({
            .image = &bloomRenderPass.colorImages[i],
            .index = 0
        });

        bloomRenderPass.upsampleFramebuffers[i].init(&bloomRenderPass.renderPasses[1]);
    }
    bloomRenderPass.downsampleCommandBuffer.init();
    bloomRenderPass.upsampleCommandBuffer.init();
#pragma endregion DOWNSAMPLE_RENDER_PASS

    //HDR render pass
#pragma region HDR_RENDER_PASS
    HdrRenderPass hdrRenderPass{};
    hdrRenderPass.colorImage.usage |= LV_IMAGE_USAGE_SAMPLED_BIT | LV_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    hdrRenderPass.colorImage.format = LV_FORMAT_R16G16B16A16_UNORM;
    hdrRenderPass.colorImage.aspectMask = LV_IMAGE_ASPECT_COLOR_BIT;
    hdrRenderPass.colorImage.init(SRC_WIDTH, SRC_HEIGHT);
    hdrRenderPass.colorSampler.init();

    hdrRenderPass.subpass.addColorAttachment({
        .index = 0,
        .layout = LV_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    });

    hdrRenderPass.renderPass.addSubpass(&hdrRenderPass.subpass);

    hdrRenderPass.renderPass.addAttachment({
        .format = hdrRenderPass.colorImage.format,
        .index = 0,
        .storeOp = LV_ATTACHMENT_STORE_OP_STORE,
        .finalLayout = LV_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    });

    hdrRenderPass.renderPass.init();

    hdrRenderPass.framebuffer.addColorAttachment({
        .image = &hdrRenderPass.colorImage,
        .index = 0
    });

    hdrRenderPass.framebuffer.init(&hdrRenderPass.renderPass);
    hdrRenderPass.commandBuffer.init();
#pragma endregion HDR_RENDER_PASS

    //Disassemble depth compute pass
    /*
#pragma region DISASM_COMPUTE_PASS
    DisasmComputePass disasmComputePass{};
    disasmComputePass.outputColorAttachment.usage |= LV_IMAGE_USAGE_SAMPLED_BIT | LV_IMAGE_USAGE_STORAGE_BIT;
    disasmComputePass.outputColorAttachment.format = LV_FORMAT_R16_SNORM;
    disasmComputePass.outputColorAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    disasmComputePass.outputColorAttachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
    disasmComputePass.outputColorAttachment.init(SRC_WIDTH, SRC_HEIGHT);
    disasmComputePass.outputColorAttachmentView.init(&disasmComputePass.outputColorAttachment);
    disasmComputePass.outputColorAttachmentSampler.init();
#pragma endregion DISASM_COMPUTE_PASS
    */

    //Graphics pipelines

#ifndef LV_BACKEND_OPENGL
    // *************** Equirectangular to cubemap shader ***************
#pragma region EQUIRECTANGULAR_TO_CUBE_SHADER
    //Vertex
    lv::ShaderModuleCreateInfo vertCubemapCreateInfo{};
    vertCubemapCreateInfo.shaderStage = LV_SHADER_STAGE_VERTEX_BIT;
    vertCubemapCreateInfo.source = lv::readFile(GET_SHADER_FILENAME("vertex/cubemap"));

    lv::ShaderModule vertCubemapModule;
    vertCubemapModule.init(vertCubemapCreateInfo);

    //Fragment
    lv::ShaderModuleCreateInfo fragEquiToCubeCreateInfo{};
    fragEquiToCubeCreateInfo.shaderStage = LV_SHADER_STAGE_FRAGMENT_BIT;
    fragEquiToCubeCreateInfo.source = lv::readFile(GET_SHADER_FILENAME("fragment/equi_to_cube"));

    lv::ShaderModule fragEquiToCubeModule;
    fragEquiToCubeModule.init(fragEquiToCubeCreateInfo);
    
    //Shader
    lv::GraphicsPipeline equiToCubeGraphicsPipeline;
	
	equiToCubeGraphicsPipeline.vertexShaderModule = &vertCubemapModule;
	equiToCubeGraphicsPipeline.fragmentShaderModule = &fragEquiToCubeModule;
	equiToCubeGraphicsPipeline.pipelineLayout = &equiToCubeLayout;
	equiToCubeGraphicsPipeline.renderPass = &skylightBakeRenderPass;

    equiToCubeGraphicsPipeline.addColorBlendAttachment({0});

    equiToCubeGraphicsPipeline.init();
#pragma endregion EQUIRECTANGULAR_TO_CUBE_SHADER

    // *************** Irradiance shader ***************
#pragma region IRRADIANCE_SHADER
    //Fragment
    lv::ShaderModuleCreateInfo fragIrradianceCreateInfo{};
    fragIrradianceCreateInfo.shaderStage = LV_SHADER_STAGE_FRAGMENT_BIT;
    fragIrradianceCreateInfo.source = lv::readFile(GET_SHADER_FILENAME("fragment/irradiance"));

    lv::ShaderModule fragIrradianceModule;
    fragIrradianceModule.init(fragIrradianceCreateInfo);
    
    //Shader
    lv::GraphicsPipeline irradianceGraphicsPipeline;
	
	irradianceGraphicsPipeline.vertexShaderModule = &vertCubemapModule;
	irradianceGraphicsPipeline.fragmentShaderModule = &fragIrradianceModule;
	irradianceGraphicsPipeline.pipelineLayout = &irradianceLayout;
	irradianceGraphicsPipeline.renderPass = &skylightBakeRenderPass;

    irradianceGraphicsPipeline.addColorBlendAttachment({0});

    irradianceGraphicsPipeline.init();
#pragma endregion IRRADIANCE_SHADER

    // *************** Irradiance shader ***************
#pragma region PREFILTERED_SHADER
    //Fragment
    lv::ShaderModuleCreateInfo fragPrefilteredCreateInfo{};
    fragPrefilteredCreateInfo.shaderStage = LV_SHADER_STAGE_FRAGMENT_BIT;
    fragPrefilteredCreateInfo.source = lv::readFile(GET_SHADER_FILENAME("fragment/prefiltered"));

    lv::ShaderModule fragPrefilteredModule;
    fragPrefilteredModule.init(fragPrefilteredCreateInfo);
    
    //Shader
    lv::GraphicsPipeline prefilteredGraphicsPipeline;
	
	prefilteredGraphicsPipeline.vertexShaderModule = &vertCubemapModule;
	prefilteredGraphicsPipeline.fragmentShaderModule = &fragPrefilteredModule;
	prefilteredGraphicsPipeline.pipelineLayout = &prefilterLayout;
	prefilteredGraphicsPipeline.renderPass = &skylightBakeRenderPass;

    prefilteredGraphicsPipeline.addColorBlendAttachment({0});

    prefilteredGraphicsPipeline.init();
#pragma endregion PREFILTERED_SHADER
#endif

    // *************** Shadow shader ***************
#pragma region SHADOW_SHADER
    //Vertex
    lv::ShaderModuleCreateInfo vertShadowCreateInfo{};
    vertShadowCreateInfo.shaderStage = LV_SHADER_STAGE_VERTEX_BIT;
    vertShadowCreateInfo.source = lv::readFile(GET_SHADER_FILENAME("vertex/shadow"));

    lv::ShaderModule vertShadowModule;
    vertShadowModule.init(vertShadowCreateInfo);

    //Fragment
    lv::ShaderModuleCreateInfo fragShadowCreateInfo{};
    fragShadowCreateInfo.shaderStage = LV_SHADER_STAGE_FRAGMENT_BIT;
    fragShadowCreateInfo.source = lv::readFile(GET_SHADER_FILENAME("fragment/shadow"));

    lv::ShaderModule fragShadowModule;
    fragShadowModule.init(fragShadowCreateInfo);
    
    //Shader
	lv::GraphicsPipeline shadowGraphicsPipeline;
	
	shadowGraphicsPipeline.vertexShaderModule = &vertShadowModule;
	shadowGraphicsPipeline.fragmentShaderModule = &fragShadowModule;
	shadowGraphicsPipeline.pipelineLayout = &shadowLayout;
	shadowGraphicsPipeline.renderPass = &shadowRenderPass.renderPass;

    shadowGraphicsPipeline.vertexDescriptor = lv::MainVertex::getVertexDescriptorShadows();

    shadowGraphicsPipeline.config.cullMode = LV_CULL_MODE_FRONT_BIT;
    shadowGraphicsPipeline.config.depthTestEnable = LV_TRUE;

    shadowGraphicsPipeline.init();
#pragma endregion SHADOW_SHADER

    // *************** Deferred shader ***************
#pragma region DEFERRED_SHADER
    //Vertex
    lv::ShaderModuleCreateInfo vertDeferredCreateInfo{};
    vertDeferredCreateInfo.shaderStage = LV_SHADER_STAGE_VERTEX_BIT;
    vertDeferredCreateInfo.source = lv::readFile(GET_SHADER_FILENAME("vertex/deferred"));

    lv::ShaderModule vertDeferredModule;
    vertDeferredModule.init(vertDeferredCreateInfo);

    //Fragment
    lv::ShaderModuleCreateInfo fragDeferredCreateInfo{};
    fragDeferredCreateInfo.shaderStage = LV_SHADER_STAGE_FRAGMENT_BIT;
    fragDeferredCreateInfo.source = lv::readFile(GET_SHADER_FILENAME("fragment/deferred"));

    lv::ShaderModule fragDeferredModule;
    fragDeferredModule.init(fragDeferredCreateInfo);
    
    //Shader
	lv::GraphicsPipeline deferredGraphicsPipeline;
	deferredGraphicsPipeline.vertexShaderModule = &vertDeferredModule;
	deferredGraphicsPipeline.fragmentShaderModule = &fragDeferredModule;
	deferredGraphicsPipeline.pipelineLayout = &deferredLayout;
	deferredGraphicsPipeline.renderPass = &mainRenderPass.renderPass;

    deferredGraphicsPipeline.vertexDescriptor = lv::MainVertex::getVertexDescriptor();

    deferredGraphicsPipeline.addColorBlendAttachment({1});
    deferredGraphicsPipeline.addColorBlendAttachment({2});
#ifdef LV_BACKEND_METAL
    deferredGraphicsPipeline.addColorBlendAttachment({4});
#endif

    deferredGraphicsPipeline.config.cullMode = LV_CULL_MODE_BACK_BIT;
    deferredGraphicsPipeline.config.depthTestEnable = LV_TRUE;

    deferredGraphicsPipeline.init();
#pragma endregion DEFERRED_SHADER

    // *************** Skylight shader ***************
#pragma region SKYLIGHT_SHADER
    //Vertex
    lv::ShaderModuleCreateInfo vertSkylightCreateInfo{};
    vertSkylightCreateInfo.shaderStage = LV_SHADER_STAGE_VERTEX_BIT;
    vertSkylightCreateInfo.source = lv::readFile(GET_SHADER_FILENAME("vertex/skylight"));

    lv::ShaderModule vertSkylightModule;
    vertSkylightModule.init(vertSkylightCreateInfo);

    //Fragment
    lv::ShaderModuleCreateInfo fragSkylightCreateInfo{};
    fragSkylightCreateInfo.shaderStage = LV_SHADER_STAGE_FRAGMENT_BIT;
    fragSkylightCreateInfo.source = lv::readFile(GET_SHADER_FILENAME("fragment/skylight"));

    lv::ShaderModule fragSkylightModule;
    fragSkylightModule.init(fragSkylightCreateInfo);
    
    //Shader
	lv::GraphicsPipeline skylightGraphicsPipeline;
	
	skylightGraphicsPipeline.vertexShaderModule = &vertSkylightModule;
	skylightGraphicsPipeline.fragmentShaderModule = &fragSkylightModule;
	skylightGraphicsPipeline.pipelineLayout = &skylightLayout;
	skylightGraphicsPipeline.renderPass = &mainRenderPass.renderPass;
    skylightGraphicsPipeline.subpassIndex = 1;

    skylightGraphicsPipeline.vertexDescriptor = lv::Vertex3D::getVertexDescriptor();

    skylightGraphicsPipeline.addColorBlendAttachment({0});
    skylightGraphicsPipeline.addColorBlendAttachment({1});
    skylightGraphicsPipeline.addColorBlendAttachment({2});
#ifdef LV_BACKEND_METAL
    skylightGraphicsPipeline.addColorBlendAttachment({4});
#endif

    skylightGraphicsPipeline.config.depthWriteEnable = LV_FALSE;

    skylightGraphicsPipeline.init();
#pragma endregion SKYLIGHT_SHADER

    // *************** Main shader ***************
#pragma region MAIN_SHADER
    //Vertex
    lv::ShaderModuleCreateInfo vertTriangleCreateInfo{};
    vertTriangleCreateInfo.shaderStage = LV_SHADER_STAGE_VERTEX_BIT;
    vertTriangleCreateInfo.source = lv::readFile(GET_SHADER_FILENAME("vertex/triangle"));

    lv::ShaderModule vertTriangleModule;
    vertTriangleModule.init(vertTriangleCreateInfo);

    //Fragment
    lv::ShaderModuleCreateInfo fragMainCreateInfo{};
    fragMainCreateInfo.shaderStage = LV_SHADER_STAGE_FRAGMENT_BIT;
    fragMainCreateInfo.source = lv::readFile(GET_SHADER_FILENAME("fragment/main"));

    lv::ShaderModule fragMainModule;
    fragMainModule.init(fragMainCreateInfo);
    
    //Shader
	lv::GraphicsPipeline mainGraphicsPipeline;
	
	mainGraphicsPipeline.vertexShaderModule = &vertTriangleModule;
	mainGraphicsPipeline.fragmentShaderModule = &fragMainModule;
	mainGraphicsPipeline.pipelineLayout = &mainLayout;
	mainGraphicsPipeline.renderPass = &mainRenderPass.renderPass;
    mainGraphicsPipeline.subpassIndex = 1;

    mainGraphicsPipeline.addColorBlendAttachment({0});
    mainGraphicsPipeline.addColorBlendAttachment({1});
    mainGraphicsPipeline.addColorBlendAttachment({2});
#ifdef LV_BACKEND_METAL
    mainGraphicsPipeline.addColorBlendAttachment({4});
#endif

    mainGraphicsPipeline.config.depthTestEnable = LV_TRUE;
    mainGraphicsPipeline.config.depthWriteEnable = LV_FALSE;
    mainGraphicsPipeline.config.depthOp = LV_COMPARE_OP_NOT_EQUAL;

    mainGraphicsPipeline.init();
#pragma endregion MAIN_SHADER

    // *************** SSAO shader ***************
#pragma region SSAO_SHADER
    //Fragment
    lv::ShaderModuleCreateInfo fragSsaoCreateInfo{};
    fragSsaoCreateInfo.shaderStage = LV_SHADER_STAGE_FRAGMENT_BIT;
    fragSsaoCreateInfo.source = lv::readFile(GET_SHADER_FILENAME("fragment/ssao"));
    
    fragSsaoCreateInfo.specializationConstants.resize(1);
    fragSsaoCreateInfo.specializationConstants[0].constantID = 0;
    fragSsaoCreateInfo.specializationConstants[0].offset = 0;
    fragSsaoCreateInfo.specializationConstants[0].size = sizeof(int);
#ifdef LV_BACKEND_METAL
    fragSsaoCreateInfo.specializationConstants[0].dataType = 29 /*MTLDataTypeInt*/; //TODO: implement this enum
#endif

    fragSsaoCreateInfo.constantsData = &game.scene().graphicsSettings.aoType;
    fragSsaoCreateInfo.constantsSize = sizeof(int);

    lv::ShaderModule fragSsaoModule;
    fragSsaoModule.init(fragSsaoCreateInfo);
    
    //Shader
	lv::GraphicsPipeline ssaoGraphicsPipeline;
	
	ssaoGraphicsPipeline.vertexShaderModule = &vertTriangleModule;
	ssaoGraphicsPipeline.fragmentShaderModule = &fragSsaoModule;
	ssaoGraphicsPipeline.pipelineLayout = &ssaoLayout;
	ssaoGraphicsPipeline.renderPass = &ssaoRenderPass.renderPass;

    ssaoGraphicsPipeline.addColorBlendAttachment({0});

    ssaoGraphicsPipeline.config.depthTestEnable = LV_TRUE;
    ssaoGraphicsPipeline.config.depthWriteEnable = LV_FALSE;
    ssaoGraphicsPipeline.config.depthOp = LV_COMPARE_OP_NOT_EQUAL;

    ssaoGraphicsPipeline.init();
#pragma endregion SSAO_SHADER

    // *************** SSAO blur shader ***************
#pragma region SSAO_BLUR_SHADER
    //Fragment
    lv::ShaderModuleCreateInfo fragBlurCreateInfo{};
    fragBlurCreateInfo.shaderStage = LV_SHADER_STAGE_FRAGMENT_BIT;
    fragBlurCreateInfo.source = lv::readFile(GET_SHADER_FILENAME("fragment/blur"));

    lv::ShaderModule fragBlurModule;
    fragBlurModule.init(fragBlurCreateInfo);
    
    //Shader
	lv::GraphicsPipeline blurGraphicsPipeline;
	
	blurGraphicsPipeline.vertexShaderModule = &vertTriangleModule;
	blurGraphicsPipeline.fragmentShaderModule = &fragBlurModule;
	blurGraphicsPipeline.pipelineLayout = &ssaoBlurLayout;
	blurGraphicsPipeline.renderPass = &ssaoBlurRenderPass.renderPass;

    blurGraphicsPipeline.addColorBlendAttachment({0});

    blurGraphicsPipeline.config.depthTestEnable = LV_TRUE;
    blurGraphicsPipeline.config.depthWriteEnable = LV_FALSE;
    blurGraphicsPipeline.config.depthOp = LV_COMPARE_OP_NOT_EQUAL;

    blurGraphicsPipeline.init();
#pragma endregion SSAO_BLUR_SHADER

    // *************** Point light shader ***************
    /*
#pragma region POINT_LIGHT_SHADER
    //Vertex
    lv::ShaderModuleCreateInfo vertPointLightCreateInfo{};
    vertPointLightCreateInfo.shaderStage = LV_SHADER_STAGE_VERTEX_BIT;
    vertPointLightCreateInfo.source = lv::readFile(GET_SHADER_FILENAME("vertex/point_light"));

    lv::ShaderModule vertPointLightModule(vertPointLightCreateInfo);

    //Fragment
    lv::ShaderModuleCreateInfo fragPointLightCreateInfo{};
    fragPointLightCreateInfo.shaderStage = LV_SHADER_STAGE_FRAGMENT_BIT;
    fragPointLightCreateInfo.source = lv::readFile(GET_SHADER_FILENAME("fragment/point_light"));

    lv::ShaderModule fragPointLightModule(fragPointLightCreateInfo);
    
    //Shader
	lv::GraphicsPipelineCreateInfo pointLightGraphicsPipelineCreateInfo{};
	
	pointLightGraphicsPipelineCreateInfo.vertexShaderModule = &vertPointLightModule;
	pointLightGraphicsPipelineCreateInfo.fragmentShaderModule = &fragPointLightModule;
	pointLightGraphicsPipelineCreateInfo.pipelineLayout = &pointLightLayout;
	pointLightGraphicsPipelineCreateInfo.renderPass = &mainRenderPass.renderPass;

    pointLightGraphicsPipelineCreateInfo.vertexDescriptor = lv::Vertex3D::getVertexDescriptor();

    pointLightGraphicsPipelineCreateInfo.config.depthTestEnable = LV_TRUE;
    pointLightGraphicsPipelineCreateInfo.config.depthWriteEnable = LV_FALSE;
    //pointLightGraphicsPipelineCreateInfo.config.depthOp = LV_COMPARE_OP_NOT_EQUAL;
    //TODO: configure the blending

	lv::GraphicsPipeline pointLightGraphicsPipeline(pointLightGraphicsPipelineCreateInfo);
#pragma endregion POINT_LIGHT_SHADER
    */

    // *************** Downsample shader ***************
#pragma region DOWNSAMPLE_SHADER
    //Fragment
    lv::ShaderModuleCreateInfo fragDownsampleCreateInfo{};
    fragDownsampleCreateInfo.shaderStage = LV_SHADER_STAGE_FRAGMENT_BIT;
    fragDownsampleCreateInfo.source = lv::readFile(GET_SHADER_FILENAME("fragment/downsample"));

    lv::ShaderModule fragDownsampleModule;
    fragDownsampleModule.init(fragDownsampleCreateInfo);
    
    //Shader
	lv::GraphicsPipeline downsampleGraphicsPipeline;
	
	downsampleGraphicsPipeline.vertexShaderModule = &vertTriangleModule;
	downsampleGraphicsPipeline.fragmentShaderModule = &fragDownsampleModule;
	downsampleGraphicsPipeline.pipelineLayout = &bloomLayout;
	downsampleGraphicsPipeline.renderPass = &bloomRenderPass.renderPasses[0];

    downsampleGraphicsPipeline.addColorBlendAttachment({0});

    downsampleGraphicsPipeline.init();
#pragma endregion DOWNSAMPLE_SHADER

    // *************** Upsample shader ***************
#pragma region UPSAMPLE_SHADER
    //Fragment
    lv::ShaderModuleCreateInfo fragUpsampleCreateInfo{};
    fragUpsampleCreateInfo.shaderStage = LV_SHADER_STAGE_FRAGMENT_BIT;
    fragUpsampleCreateInfo.source = lv::readFile(GET_SHADER_FILENAME("fragment/upsample"));

    lv::ShaderModule fragUpsampleModule;
    fragUpsampleModule.init(fragUpsampleCreateInfo);
    
    //Shader
	lv::GraphicsPipeline upsampleGraphicsPipeline;
	upsampleGraphicsPipeline.vertexShaderModule = &vertTriangleModule;
	upsampleGraphicsPipeline.fragmentShaderModule = &fragUpsampleModule;
	upsampleGraphicsPipeline.pipelineLayout = &bloomLayout;
	upsampleGraphicsPipeline.renderPass = &bloomRenderPass.renderPasses[1];

    upsampleGraphicsPipeline.addColorBlendAttachment({
        .index = 0,
        .blendEnable = LV_TRUE,
        .srcRgbBlendFactor = LV_BLEND_FACTOR_ONE,
        .dstRgbBlendFactor = LV_BLEND_FACTOR_ONE,
        .srcAlphaBlendFactor = LV_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = LV_BLEND_FACTOR_ONE
    });

    upsampleGraphicsPipeline.init();
#pragma endregion UPSAMPLE_SHADER

    // *************** HDR shader ***************
#pragma region HDR_SHADER
    //Fragment
    lv::ShaderModuleCreateInfo fragHdrCreateInfo{};
    fragHdrCreateInfo.shaderStage = LV_SHADER_STAGE_FRAGMENT_BIT;
    fragHdrCreateInfo.source = lv::readFile(GET_SHADER_FILENAME("fragment/hdr"));

    lv::ShaderModule fragHdrModule;
    fragHdrModule.init(fragHdrCreateInfo);
    
    //Shader
	lv::GraphicsPipeline hdrGraphicsPipeline;
	hdrGraphicsPipeline.vertexShaderModule = &vertTriangleModule;
	hdrGraphicsPipeline.fragmentShaderModule = &fragHdrModule;
	hdrGraphicsPipeline.pipelineLayout = &hdrLayout;
	hdrGraphicsPipeline.renderPass = &hdrRenderPass.renderPass;

    hdrGraphicsPipeline.addColorBlendAttachment({0});

    hdrGraphicsPipeline.init();
#pragma endregion HDR_SHADER

    //Compute pipelines

    // *************** Disassemble depth shader ***************
    /*
#pragma region DISASSEMBLE_DEPTH_SHADER
    //Compute
    lv::ShaderModuleCreateInfo compDisasmCreateInfo{};
    compDisasmCreateInfo.shaderStage = LV_SHADER_STAGE_COMPUTE_BIT;
    compDisasmCreateInfo.source = lv::readFile(GET_SHADER_FILENAME("compute/disassemble_depth"));

    lv::ShaderModule compDisasmModule(compDisasmCreateInfo);

    //Shader
    lv::ComputePipelineCreateInfo disasmComputePipelineCreateInfo{};

    disasmComputePipelineCreateInfo.computeShaderModule = &compDisasmModule;
    disasmComputePipelineCreateInfo.pipelineLayout = &disasmLayout;

    lv::ComputePipeline disasmComputePipeline(disasmComputePipelineCreateInfo);

#pragma endregion DISASSEMBLE_DEPTH_SHADER
    */

    //Command buffers
    lv::CommandBuffer depthBlitCommandBuffer;
    depthBlitCommandBuffer.init();

    //Semaphores
    lv::Semaphore deferredRenderSemaphore;
    deferredRenderSemaphore.init();

    lv::Semaphore depthBlitSemaphore;
    depthBlitSemaphore.init();

    //Light
    lv::DirectLight directLight;
    directLight.light.direction = glm::normalize(glm::vec3(2.0f, -4.0f, 1.0f));

    //Uniform buffers
    lv::Buffer deferredVPUniformBuffer;
    deferredVPUniformBuffer.usage = LV_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    deferredVPUniformBuffer.memoryType = LV_MEMORY_TYPE_SHARED;
    deferredVPUniformBuffer.init(sizeof(glm::mat4));
    lv::Buffer shadowVPUniformBuffers[CASCADE_COUNT];
    for (uint8_t i = 0; i < CASCADE_COUNT; i++) {
        shadowVPUniformBuffers[i].usage = LV_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        shadowVPUniformBuffers[i].memoryType = LV_MEMORY_TYPE_SHARED;
        shadowVPUniformBuffers[i].init(sizeof(glm::mat4));
    }
    lv::Buffer mainShadowUniformBuffer;
    mainShadowUniformBuffer.usage = LV_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    mainShadowUniformBuffer.memoryType = LV_MEMORY_TYPE_SHARED;
    mainShadowUniformBuffer.init(sizeof(glm::mat4) * CASCADE_COUNT);
    lv::Buffer mainVPUniformBuffer;
    mainVPUniformBuffer.usage = LV_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    mainVPUniformBuffer.memoryType = LV_MEMORY_TYPE_SHARED;
    mainVPUniformBuffer.init(sizeof(UBOMainVP));

    //Textures
    std::default_random_engine rndEngine((unsigned)time(0));
	std::uniform_real_distribution<int8_t> rndInt8Dist(-128, 127);

    std::vector<glm::i8vec4> ssaoNoise(AO_NOISE_TEX_SIZE * AO_NOISE_TEX_SIZE);
    for (uint32_t i = 0; i < ssaoNoise.size(); i++) {
        ssaoNoise[i] = glm::i8vec4(rndInt8Dist(rndEngine), rndInt8Dist(rndEngine), 0, 0);
    }

	std::uniform_real_distribution<float> rndFloatDist(-128, 127);

    std::vector<glm::i8vec4> hbaoNoise(AO_NOISE_TEX_SIZE * AO_NOISE_TEX_SIZE);
    for (uint32_t i = 0; i < hbaoNoise.size(); i++) {
        glm::vec4 rndVec(rndFloatDist(rndEngine), rndFloatDist(rndEngine), 0, 0);
        while (glm::length2(rndVec) > 1.0f) {
            rndVec = glm::vec4(rndFloatDist(rndEngine), rndFloatDist(rndEngine), 0, 0);
        }
        uint8_t index = rand() % 2;
        rndVec[(index + 1) % 2] /= fabs(rndVec[index]);
        rndVec[index] = rndVec[index] > 0.0f ? 1.0f : -1.0f;
        hbaoNoise[i] = glm::i8vec4(rndVec[0] * 127, rndVec[1] * 127, 0, 0);
    }

    lv::Texture aoNoiseTex;
    aoNoiseTex.image.format = LV_FORMAT_R8G8B8A8_SNORM;
    aoNoiseTex.image.usage = LV_IMAGE_USAGE_SAMPLED_BIT | LV_IMAGE_USAGE_TRANSFER_DST_BIT;
    aoNoiseTex.image.init(AO_NOISE_TEX_SIZE, AO_NOISE_TEX_SIZE);
    aoNoiseTex.image.copyDataTo(0, hbaoNoise.data());
    lv::Sampler aoNoiseSampler;
    aoNoiseSampler.addressMode = LV_SAMPLER_ADDRESS_MODE_REPEAT;
    aoNoiseSampler.init();

    //Skylight
    lv::Skylight skylight(0);
    skylight.load(0, "assets/skylight/canyon.hdr", equiToCubeGraphicsPipeline);
    skylight.createIrradianceMap(0, irradianceGraphicsPipeline);
    skylight.createPrefilteredMap(0, prefilteredGraphicsPipeline);

    lv::Texture brdfLutTexture;
    brdfLutTexture.init("assets/textures/brdf_lut.png");
    lv::Sampler brdfLutSampler;
    brdfLutSampler.filter = LV_FILTER_LINEAR;
    brdfLutSampler.init();

    //Descriptor sets

    //Deferred descriptor set
    lv::DescriptorSet deferredDecriptorSet;
    deferredDecriptorSet.pipelineLayout = &deferredLayout;
    deferredDecriptorSet.layoutIndex = 0;
    deferredDecriptorSet.addBinding(deferredVPUniformBuffer.descriptorInfo(), 0);
    deferredDecriptorSet.init();

    //Shadow descriptor set
    lv::DescriptorSet shadowDecriptorSets[CASCADE_COUNT];
    for (uint8_t i = 0; i < CASCADE_COUNT; i++) {
        shadowDecriptorSets[i].pipelineLayout = &shadowLayout;
        shadowDecriptorSets[i].layoutIndex = 0;
        shadowDecriptorSets[i].addBinding(shadowVPUniformBuffers[i].descriptorInfo(), 0);
        shadowDecriptorSets[i].init();
    }

    //Skylight descriptor set
    lv::DescriptorSet skylightDescriptorSet;
    skylightDescriptorSet.pipelineLayout = &skylightLayout;
    skylightDescriptorSet.layoutIndex = 0;
    skylightDescriptorSet.addBinding(skylight.sampler.descriptorInfo(skylight.environmentMapImage), 0);
    skylightDescriptorSet.init();

    lv::DescriptorSet ssaoDescriptorSet;
    ssaoDescriptorSet.pipelineLayout = &ssaoLayout;
    ssaoDescriptorSet.layoutIndex = 0;
    SETUP_SSAO_DESCRIPTORS
    ssaoDescriptorSet.init();

    lv::DescriptorSet ssaoBlurDescriptorSet;
    ssaoBlurDescriptorSet.pipelineLayout = &ssaoBlurLayout;
    ssaoBlurDescriptorSet.layoutIndex = 0;
    SETUP_SSAO_BLUR_DESCRIPTORS
    ssaoBlurDescriptorSet.init();

    lv::DescriptorSet mainDescriptorSet0;
    mainDescriptorSet0.pipelineLayout = &mainLayout;
    mainDescriptorSet0.layoutIndex = 0;
    SETUP_MAIN_0_DESCRIPTORS
    mainDescriptorSet0.init();

    lv::DescriptorSet mainDescriptorSet1;
    mainDescriptorSet1.pipelineLayout = &mainLayout;
    mainDescriptorSet1.layoutIndex = 1;
    SETUP_MAIN_1_DESCRIPTORS
    mainDescriptorSet1.init();

    lv::DescriptorSet mainDescriptorSet2;
    mainDescriptorSet2.pipelineLayout = &mainLayout;
    mainDescriptorSet2.layoutIndex = 2;
    #ifdef LV_BACKEND_VULKAN
    mainDescriptorSet2.addBinding(mainRenderPass.depthImage.descriptorInfo(LV_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, LV_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL), 0);
    #elif defined(LV_BACKEND_METAL)
    mainDescriptorSet2.addBinding(mainRenderPass.depthAsColorImage.descriptorInfo(LV_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, LV_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL), 0);
    #endif
    mainDescriptorSet2.addBinding(mainRenderPass.normalRoughnessImage.descriptorInfo(LV_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, LV_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL), 1);
    mainDescriptorSet2.addBinding(mainRenderPass.albedoMetallicImage.descriptorInfo(LV_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, LV_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL), 2);
    mainDescriptorSet2.init();

    lv::DescriptorSet downsampleDescriptorSets[BLOOM_MIP_COUNT];
    downsampleDescriptorSets[0].pipelineLayout = &bloomLayout;
    downsampleDescriptorSets[0].layoutIndex = 0;
    downsampleDescriptorSets[0].addBinding(bloomRenderPass.downsampleSampler.descriptorInfo(mainRenderPass.colorImage), 0);
    downsampleDescriptorSets[0].init();
    for (uint8_t i = 1; i < BLOOM_MIP_COUNT; i++) {
        downsampleDescriptorSets[i].pipelineLayout = &bloomLayout;
        downsampleDescriptorSets[i].layoutIndex = 0;
        downsampleDescriptorSets[i].addBinding(bloomRenderPass.downsampleSampler.descriptorInfo(bloomRenderPass.colorImages[i - 1]), 0);
        downsampleDescriptorSets[i].init();
    }

    lv::DescriptorSet upsampleDescriptorSets[BLOOM_MIP_COUNT - 1];
    for (uint8_t i = 0; i < BLOOM_MIP_COUNT - 1; i++) {
        upsampleDescriptorSets[i].pipelineLayout = &bloomLayout;
        upsampleDescriptorSets[i].layoutIndex = 0;
        upsampleDescriptorSets[i].addBinding(bloomRenderPass.upsampleSampler.descriptorInfo(bloomRenderPass.colorImages[i + 1]), 0);
        upsampleDescriptorSets[i].init();
    }

    lv::DescriptorSet hdrDescriptorSet;
    hdrDescriptorSet.pipelineLayout = &hdrLayout;
    hdrDescriptorSet.layoutIndex = 0;
    SETUP_HDR_DESCRIPTORS
    hdrDescriptorSet.init();

    /*
    lv::DescriptorSet disasmDescriptorSet(disasmLayout, 0);
    disasmDescriptorSet.addBinding(deferredRenderPass.depthAttachmentView.descriptorInfo(), 0);
    disasmDescriptorSet.addBinding(disasmComputePass.outputColorAttachmentView.descriptorInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_IMAGE_LAYOUT_GENERAL), 1);
    disasmDescriptorSet.init();
    */

    //Viewports
	lv::Viewport mainViewport(0, 0, SRC_WIDTH/* * 2*/, SRC_HEIGHT/* * 2*/);
    lv::Viewport halfMainViewport(0, 0, SRC_WIDTH / 2, SRC_HEIGHT / 2);
    lv::Viewport shadowViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);

    //camera.aspectRatio = (float)SRC_WIDTH / (float)SRC_HEIGHT;
    g_game->scene().editorCamera.yScroll = g_game->scene().editorCamera.farPlane * 0.05f;

    //SSAO Kernel
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between [0.0, 1.0]
    std::default_random_engine generator;
    std::vector<glm::vec3> ssaoKernel(SSAO_KERNEL_SIZE);
    for (unsigned int i = 0; i < SSAO_KERNEL_SIZE; ++i) {
        glm::vec3 sample(
            randomFloats(generator) * 2.0 - 1.0, 
            randomFloats(generator) * 2.0 - 1.0, 
            randomFloats(generator)
        );
        sample  = glm::normalize(sample);
        sample *= randomFloats(generator);
        
        float scale = (float)i / (float)SSAO_KERNEL_SIZE; 
        scale = lerp(0.1f, 1.0f, scale * scale);
        ssaoKernel[i] = sample * scale;
    }

    //Printing kernel
    std::cout << "const vec3 SSAO_KERNEL[SSAO_KERNEL_SIZE] = vec3[](" << std::endl;
    for (uint8_t i = 0; i < ssaoKernel.size(); i++) {
        std::cout << "    vec3(" << ssaoKernel[i].x << ", " << ssaoKernel[i].y << ", " << ssaoKernel[i].z << ")" << (i < ssaoKernel.size() - 1 ? "," : "") << std::endl;
    }
    std::cout << ");" << std::endl;

    //Editor
    Editor editor(window, deferredLayout);

    editor.init();

    editor.createViewportSet(
#ifdef LV_BACKEND_VULKAN
        hdrRenderPass.colorImage.imageViews, hdrRenderPass.colorSampler.sampler
#elif defined(LV_BACKEND_METAL)
        hdrRenderPass.colorImage.images
#endif
    );

    //return 0;

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

        shadowFrameCounter = (shadowFrameCounter + 1) % SHADOW_FRAME_COUNT;

        lvndPollEvents(window);
        //std::cout << "Shift: " << lvndGetModifier(window.window, LVND_MODIFIER_SHIFT) << " : " << "Command: " << lvndGetModifier(window.window, LVND_MODIFIER_SUPER) << std::endl;

        //lvndSetWindowFullscreenMode(window, true);

        if (windowResized) {
            //Wait for device
            device.waitIdle();
            
            swapChain.resize();

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

            windowResized = false;
        }

        //Mouse
		int32_t mouseX, mouseY;
		lvndGetCursorPosition(window, &mouseX, &mouseY);

        //Window size
        uint16_t width, height;
        lvndGetWindowSize(window, &width, &height);

        //Camera
        game.scene().updateCameraTransforms();

        game.scene().camera->aspectRatio = float(editor.viewportWidth) / float(editor.viewportHeight);
        if (editor.activeViewport == VIEWPORT_TYPE_SCENE) {
            if (editor.cameraActive) {
                game.scene().editorCamera.inputs(window, dt, mouseX, mouseY, width, height);
            } else {
                game.scene().editorCamera.firstClick = true;
            }
        }
        game.scene().camera->loadViewProj();

        game.scene().calculateModels();

        game.scene().update(dt);

        swapChain.acquireNextImage();
        //std::cout << "New frame" << std::endl;

        //Rendering

        //Shadow matrices
        getLightMatrices(directLight.light.direction);

        //Shadow render pass
        shadowRenderPass.commandBuffer.bind();

        for (uint8_t i = 0; i < CASCADE_COUNT; i++) {
            if (updateShadowFrames[i]) {
                //std::cout << "Cascade: " << (int)i << std::endl;
                shadowRenderPass.framebuffers[i].bind();

                shadowGraphicsPipeline.bind();
                shadowViewport.bind();
                
                shadowDecriptorSets[i].bind();

                //PCShadowVP pcShadowVP{shadowVPs[i], (int)i};
                shadowVPUniformBuffers[i].copyDataTo(0, &shadowVPs[i]);

                game.scene().renderShadows(shadowGraphicsPipeline);

                shadowRenderPass.framebuffers[i].unbind();
            }
        }

        shadowRenderPass.commandBuffer.unbind();

        shadowRenderPass.commandBuffer.submit();

        //Main render pass
        mainRenderPass.commandBuffer.bind();

        mainRenderPass.framebuffer.bind();

        deferredGraphicsPipeline.bind();
        mainViewport.bind();

        deferredDecriptorSet.bind();

        glm::mat4 viewProj = g_game->scene().camera->projection * g_game->scene().camera->view;

        deferredVPUniformBuffer.copyDataTo(0, &viewProj);

        game.scene().render(deferredGraphicsPipeline);

        //Next subpass
        mainRenderPass.framebuffer.nextSubpass();

        //Skylight
        skylightGraphicsPipeline.bind();
        mainViewport.bind();

        skylightDescriptorSet.bind();

        glm::mat4 skylightViewProj = g_game->scene().camera->projection * glm::mat4(glm::mat3(g_game->scene().camera->view));

        skylightGraphicsPipeline.uploadPushConstants(&skylightViewProj, 0);

        skylight.vertexBuffer.bindVertexBuffer();
        skylight.indexBuffer.bindIndexBuffer(LV_INDEX_TYPE_UINT32);
        skylight.indexBuffer.drawIndexed(sizeof(uint32_t));

        //Main
        mainGraphicsPipeline.bind();
        mainViewport.bind();

        mainDescriptorSet0.bind();
        mainDescriptorSet1.bind();
        mainDescriptorSet2.bind();

        UBOMainVP uboMainVP{glm::inverse(viewProj), g_game->scene().camera->position};
        mainVPUniformBuffer.copyDataTo(0, &uboMainVP);

        directLight.lightUniformBuffer.copyDataTo(0, &directLight.light);
        mainShadowUniformBuffer.copyDataTo(0, shadowVPs);

        swapChain.renderFullscreenTriangle();

        mainRenderPass.framebuffer.unbind();

        mainRenderPass.commandBuffer.unbind();

		mainRenderPass.commandBuffer.submit(nullptr, &deferredRenderSemaphore);

        //Disassemble depth compute pass
        /*
        disasmComputePipeline.bind();

        disasmDescriptorSet.bind();

        disasmComputePipeline.dispatch(64, 64, 1,
                                       (SRC_WIDTH + 64 - 1) / 64, (SRC_HEIGHT + 64 - 1) / 64, 1);
        */

        //Creating depth image half the original size
        depthBlitCommandBuffer.bind();

        mainRenderPass.halfDepthImage.blitToFromImage(0, mainRenderPass.depthImage);

        depthBlitCommandBuffer.unbind();

        depthBlitCommandBuffer.submit(&deferredRenderSemaphore, &depthBlitSemaphore);

        //SSAO render pass
        ssaoRenderPass.commandBuffer.bind();

        ssaoRenderPass.framebuffer.bind();

        ssaoGraphicsPipeline.bind();
        halfMainViewport.bind();

        ssaoDescriptorSet.bind();

        PCSsaoVP pcSsaoVP{g_game->scene().camera->projection, g_game->scene().camera->view, glm::inverse(viewProj)};
        ssaoGraphicsPipeline.uploadPushConstants(&pcSsaoVP, 0);

        swapChain.renderFullscreenTriangle();

        ssaoRenderPass.framebuffer.unbind();

        ssaoRenderPass.commandBuffer.unbind();

        ssaoRenderPass.commandBuffer.submit(&depthBlitSemaphore);

        //SSAO blur render pass
        ssaoBlurRenderPass.commandBuffer.bind();

        ssaoBlurRenderPass.framebuffer.bind();

        blurGraphicsPipeline.bind();
        mainViewport.bind();

        ssaoBlurDescriptorSet.bind();

        swapChain.renderFullscreenTriangle();

        ssaoBlurRenderPass.framebuffer.unbind();

        ssaoBlurRenderPass.commandBuffer.unbind();

        ssaoBlurRenderPass.commandBuffer.submit();

        //Downsample render pass
        bloomRenderPass.downsampleCommandBuffer.bind();

        for (uint8_t i = 0; i < BLOOM_MIP_COUNT; i++) {
            bloomRenderPass.downsampleFramebuffers[i].bind();

            downsampleGraphicsPipeline.bind();
            bloomRenderPass.viewports[i].bind();

            downsampleDescriptorSets[i].bind();

            swapChain.renderFullscreenTriangle();

            bloomRenderPass.downsampleFramebuffers[i].unbind();
        }

        bloomRenderPass.downsampleCommandBuffer.unbind();

        bloomRenderPass.downsampleCommandBuffer.submit();

        //Upsample render pass
        bloomRenderPass.upsampleCommandBuffer.bind();

        for (int8_t i = BLOOM_MIP_COUNT - 2; i >= 0; i--) {
            bloomRenderPass.upsampleFramebuffers[i].bind();

            upsampleGraphicsPipeline.bind();
            bloomRenderPass.viewports[i].bind();

            upsampleDescriptorSets[i].bind();

            swapChain.renderFullscreenTriangle();

            bloomRenderPass.upsampleFramebuffers[i].unbind();
        }

        bloomRenderPass.upsampleCommandBuffer.unbind();

        bloomRenderPass.upsampleCommandBuffer.submit();

        //HDR render pass
        hdrRenderPass.commandBuffer.bind();

        hdrRenderPass.framebuffer.bind();

        hdrGraphicsPipeline.bind();
        mainViewport.bind();

        hdrDescriptorSet.bind();

        swapChain.renderFullscreenTriangle();

        hdrRenderPass.framebuffer.unbind();

        hdrRenderPass.commandBuffer.unbind();

        hdrRenderPass.commandBuffer.submit();

        //Present
        swapChain.commandBuffer.bind();

        swapChain.framebuffer.bind();
        
        //GUI
        editor.newFrame();

        editor.beginDockspace();

        editor.createSceneViewport();
        editor.createGameViewport();
        editor.createSceneHierarchy();
        editor.createPropertiesPanel();
        editor.createAssetsPanel();
        editor.createGraphicsPanel();
        //ImGui::ShowDemoWindow();

        if (game.scene().graphicsSettings.aoType != game.scene().graphicsSettings.prevAoType) {
            switch (game.scene().graphicsSettings.aoType) {
                case AMBIENT_OCCLUSION_TYPE_NONE:
                    break;
                case AMBIENT_OCCLUSION_TYPE_SSAO:
                    aoNoiseTex.image.copyDataTo(0, ssaoNoise.data());

                    break;
                case AMBIENT_OCCLUSION_TYPE_HBAO:
                    aoNoiseTex.image.copyDataTo(0, hbaoNoise.data());

                    break;
                default:
                    break;
            }
            fragSsaoModule.recompile();
            ssaoGraphicsPipeline.recompile();
        }

        editor.endDockspace();

        editor.render();
    
		swapChain.framebuffer.unbind();

        swapChain.commandBuffer.unbind();

        swapChain.renderAndPresent();
    }

    device.waitIdle();

    directLight.destroy();

    game.destroy();

    deferredVPUniformBuffer.destroy();
    for (auto& shadowVPUniformBuffer : shadowVPUniformBuffers)
        shadowVPUniformBuffer.destroy();
    mainShadowUniformBuffer.destroy();
    mainVPUniformBuffer.destroy();

    mainRenderPass.renderPass.destroy();
    mainRenderPass.framebuffer.destroy();

    shadowRenderPass.renderPass.destroy();
    for (uint8_t i = 0; i < CASCADE_COUNT; i++)
        shadowRenderPass.framebuffers[i].destroy();

    ssaoRenderPass.renderPass.destroy();
    ssaoRenderPass.framebuffer.destroy();

    ssaoBlurRenderPass.renderPass.destroy();
    ssaoBlurRenderPass.framebuffer.destroy();

    mainRenderPass.renderPass.destroy();
    mainRenderPass.framebuffer.destroy();

#ifndef LV_BACKEND_OPENGL
    vertCubemapModule.destroy();
    fragEquiToCubeModule.destroy();

    fragIrradianceModule.destroy();
#endif

    vertDeferredModule.destroy();
    fragDeferredModule.destroy();
    deferredGraphicsPipeline.destroy();

    vertShadowModule.destroy();
    fragShadowModule.destroy();
    shadowGraphicsPipeline.destroy();

    vertTriangleModule.destroy();
    fragSsaoModule.destroy();
    ssaoGraphicsPipeline.destroy();

    fragBlurModule.destroy();
    blurGraphicsPipeline.destroy();

    vertSkylightModule.destroy();
    fragSkylightModule.destroy();
    skylightGraphicsPipeline.destroy();

    fragMainModule.destroy();
    mainGraphicsPipeline.destroy();

    fragHdrModule.destroy();
    hdrGraphicsPipeline.destroy();

    //compDisasmModule.destroy();
    //disasmComputePipeline.destroy();

    mainRenderPass.normalRoughnessImage.destroy();
    mainRenderPass.normalRoughnessSampler.destroy();

    mainRenderPass.albedoMetallicImage.destroy();
    mainRenderPass.albedoMetallicSampler.destroy();

    mainRenderPass.depthImage.destroy();

    shadowRenderPass.depthImage.destroy();
    for (uint8_t i = 0; i < CASCADE_COUNT; i++)
        shadowRenderPass.depthImageViews[i].destroy();
    shadowRenderPass.depthSampler.destroy();

    ssaoRenderPass.colorImage.destroy();
    ssaoRenderPass.colorSampler.destroy();

    ssaoBlurRenderPass.colorImage.destroy();
    ssaoBlurRenderPass.colorSampler.destroy();

    mainRenderPass.colorImage.destroy();
    mainRenderPass.colorSampler.destroy();

    editor.destroy();

    swapChain.destroy();
	device.destroy();
    instance.destroy();

#ifdef LV_BACKEND_VULKAN
    lvndVulkanDestroyLayer(window);
#elif defined(LV_BACKEND_METAL)
    lvndMetalDestroyLayer(window);
#elif defined(LV_BACKEND_OPENGL)
    lvndOpenGLDestroyContext(window);
#endif

    lvndDestroyWindow(window);

    return 0;
}

//Callbacks
void windowResizeCallback(LvndWindow* window, uint16_t width, uint16_t height) {
    windowResized = true;
}

void scrollCallback(LvndWindow* window, double xoffset, double yoffset) {
    if (g_editor->activeViewport == VIEWPORT_TYPE_SCENE && g_editor->cameraActive) {
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

void getLightMatrix(glm::vec3 lightDir, float nearPlane, float farPlane, uint8_t index) {
    updateShadowFrames[index] = true;//(shadowFrameCounter + shadowStartingFrames[index]) % shadowRefreshFrames[index] == 0;
    if (updateShadowFrames[index]) {
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

        glm::mat4 projection = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);

        shadowVPs[index] = projection * view;
    }
}

void getLightMatrices(glm::vec3 lightDir) {
    for (int i = 0; i < CASCADE_COUNT; i++) {
        if (i == 0) {
            getLightMatrix(lightDir, g_game->scene().camera->nearPlane, cascadeLevels[0], 0);
        } else {
            getLightMatrix(lightDir, cascadeLevels[i - 1], cascadeLevels[i], i);
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
        g_game->scene().newEntityWithModel(filename.c_str());
    }
}

void newEntityPlane() {
    lv::Entity entity(g_game->scene().addEntity(), g_game->scene().registry);
    entity.getComponent<lv::TagComponent>().name = "plane";
    entity.addComponent<lv::TransformComponent>();
    lv::MeshComponent& meshComponent = entity.addComponent<lv::MeshComponent>(g_game->deferredLayout);
    meshComponent.createPlane(0);
    entity.addComponent<lv::MaterialComponent>(g_game->deferredLayout);
}

void duplicateEntity() {
    lv::Entity entity(g_game->scene().addEntity(), g_game->scene().registry);
    lv::Entity selectedEntity(g_editor->selectedEntityID, g_game->scene().registry);

    //Copying components
    entity.addComponent<lv::TagComponent>(selectedEntity.getComponent<lv::TagComponent>());
    lv::NodeComponent& nodeComponent = entity.addComponent<lv::NodeComponent>(selectedEntity.getComponent<lv::NodeComponent>());
    if (nodeComponent.parent != lv::Entity::nullEntity)
        g_game->scene().registry->get<lv::NodeComponent>(nodeComponent.parent).childs.push_back(entity.ID);
    nodeComponent.childs.clear();
    
    if (selectedEntity.hasComponent<lv::TransformComponent>())
        entity.addComponent<lv::TransformComponent>(selectedEntity.getComponent<lv::TransformComponent>());

    if (selectedEntity.hasComponent<lv::MeshComponent>())
        entity.addComponent<lv::MeshComponent>(selectedEntity.getComponent<lv::MeshComponent>());

    if (selectedEntity.hasComponent<lv::MaterialComponent>())
        entity.addComponent<lv::MaterialComponent>(selectedEntity.getComponent<lv::MaterialComponent>());

    if (selectedEntity.hasComponent<lv::ScriptComponent>()) {
        lv::ScriptComponent& scriptComponent = entity.addComponent<lv::ScriptComponent>(selectedEntity.getComponent<lv::ScriptComponent>());
        if (scriptComponent.isValid())
            scriptComponent.entity = scriptComponent.script->entityConstructor(scriptComponent.entity->ID, g_game->scene().registry);
    }

    if (selectedEntity.hasComponent<lv::CameraComponent>()) {
        lv::CameraComponent& cameraComponent = entity.addComponent<lv::CameraComponent>(selectedEntity.getComponent<lv::CameraComponent>());
        cameraComponent.active = false;
    }

    if (selectedEntity.hasComponent<lv::SphereColliderComponent>())
        entity.addComponent<lv::SphereColliderComponent>(selectedEntity.getComponent<lv::SphereColliderComponent>());

    if (selectedEntity.hasComponent<lv::BoxColliderComponent>())
        entity.addComponent<lv::BoxColliderComponent>(selectedEntity.getComponent<lv::BoxColliderComponent>());

    if (selectedEntity.hasComponent<lv::RigidBodyComponent>())
        entity.addComponent<lv::RigidBodyComponent>(selectedEntity.getComponent<lv::RigidBodyComponent>());
    
    g_editor->selectedEntityID = entity.ID;
}
