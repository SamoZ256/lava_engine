#pragma once
#include "imgui.h"      // IMGUI_IMPL_API

#define LVND_DEBUG
#include "lvnd/lvnd.h"

/*
struct LvndWindow;
enum LvndKey;
enum LvndMouseButton;
enum LvndState;
*/

//Might be implemented in the future
//IMGUI_IMPL_API bool     ImGui_ImplLvnd_InitForOpenGL(GLFWwindow* window, bool install_callbacks);
IMGUI_IMPL_API bool     ImGui_ImplLvnd_InitForVulkan(LvndWindow* window, bool install_callbacks);
IMGUI_IMPL_API bool     ImGui_ImplLvnd_InitForOther(LvndWindow* window, bool install_callbacks);
IMGUI_IMPL_API void     ImGui_ImplLvnd_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplLvnd_NewFrame();

// GLFW callbacks (installer)
// - When calling Init with 'install_callbacks=true': ImGui_ImplLvnd_InstallCallbacks() is called. GLFW callbacks will be installed for you. They will chain-call user's previously installed callbacks, if any.
// - When calling Init with 'install_callbacks=false': GLFW callbacks won't be installed. You will need to call individual function yourself from your own GLFW callbacks.
IMGUI_IMPL_API void     ImGui_ImplLvnd_InstallCallbacks(LvndWindow* window);
IMGUI_IMPL_API void     ImGui_ImplLvnd_RestoreCallbacks(LvndWindow* window);

// GLFW callbacks (individual callbacks to call if you didn't install callbacks)
IMGUI_IMPL_API void     ImGui_ImplLvnd_WindowFocusCallback(LvndWindow* window, bool focused);        // Since 1.84
IMGUI_IMPL_API void     ImGui_ImplLvnd_CursorEnterCallback(LvndWindow* window, bool entered);        // Since 1.84
IMGUI_IMPL_API void     ImGui_ImplLvnd_CursorPosCallback(LvndWindow* window, int32_t xpos, int32_t ypos);   // Since 1.87
IMGUI_IMPL_API void     ImGui_ImplLvnd_MouseButtonCallback(LvndWindow* window, LvndMouseButton mouseButton, LvndState state);
IMGUI_IMPL_API void     ImGui_ImplLvnd_ScrollCallback(LvndWindow* window, double xoffset, double yoffset);
IMGUI_IMPL_API void     ImGui_ImplLvnd_KeyCallback(LvndWindow* window, LvndKey key, LvndState state);
IMGUI_IMPL_API void     ImGui_ImplLvnd_CharCallback(LvndWindow* window, uint16_t c);
