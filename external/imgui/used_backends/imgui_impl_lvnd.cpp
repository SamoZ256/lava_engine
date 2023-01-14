#include "imgui.h"
#include "imgui_impl_lvnd.h"

// Clang warnings with -Weverything
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"     // warning: use of old-style cast
#pragma clang diagnostic ignored "-Wsign-conversion"    // warning: implicit conversion changes signedness
#endif

// Lvnd
#include <lvnd/lvnd.h>

// We gather version tests as define in order to easily see which features are version-dependent.
#define LVND_VERSION_COMBINED           (LVND_VERSION_MAJOR * 1000 + LVND_VERSION_MINOR * 100 + LVND_VERSION_REVISION)

// Lvnd data
enum LvndClientApi
{
    LvndClientApi_Unknown,
    LvndClientApi_OpenGL,
    LvndClientApi_Vulkan
};

struct ImGui_ImplLvnd_Data
{
    LvndWindow*             Window;
    LvndClientApi           ClientApi;
    double                  Time;
    LvndWindow*             MouseWindow;
    ImVec2                  LastValidMousePos;
    bool                    InstalledCallbacks;

    // Chain Lvnd callbacks: our callbacks will call the user's previously installed callbacks, if any.
    LvndWindowFocusCallbackFun        PrevUserCallbackWindowFocus;
    LvndCursorMovedCallbackFun        PrevUserCallbackCursorPos;
    LvndCursorEnteredCallbackFun      PrevUserCallbackCursorEnter;
    LvndMouseButtonPressedCallbackFun PrevUserCallbackMousebutton;
    LvndScrollCallbackFun             PrevUserCallbackScroll;
    LvndKeyPressedCallbackFun         PrevUserCallbackKey;
    LvndCharCallbackFun               PrevUserCallbackChar;

    ImGui_ImplLvnd_Data()   { memset((void*)this, 0, sizeof(*this)); }
};

// Backend data stored in io.BackendPlatformUserData to allow support for multiple Dear ImGui contexts
// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple windows) instead of multiple Dear ImGui contexts.
// FIXME: multi-context support is not well tested and probably dysfunctional in this backend.
// - Because lvndPollEvents() process all windows and some events may be called outside of it, you will need to register your own callbacks
//   (passing install_callbacks=false in ImGui_ImplLvnd_InitXXX functions), set the current dear imgui context and then call our callbacks.
// - Otherwise we may need to store a LvndWindow* -> ImGuiContext* map and handle this in the backend, adding a little bit of extra complexity to it.
// FIXME: some shared resources (mouse cursor shape, gamepad) are mishandled when using multi-context.
static ImGui_ImplLvnd_Data* ImGui_ImplLvnd_GetBackendData() {
    return ImGui::GetCurrentContext() ? (ImGui_ImplLvnd_Data*)ImGui::GetIO().BackendPlatformUserData : nullptr;
}

// Functions
static const char* ImGui_ImplLvnd_GetClipboardText(void* user_data) {
    //TODO: implement this
    //return lvndGetClipboardString((LvndWindow*)user_data);
    return "";
}

static void ImGui_ImplLvnd_SetClipboardText(void* user_data, const char* text) {
    //TODO: implement this
    //lvndSetClipboardString((LvndWindow*)user_data, text);
}

static ImGuiKey ImGui_ImplLvnd_KeyToImGuiKey(int key) {
    switch (key) {
        case LVND_KEY_TAB: return ImGuiKey_Tab;
        case LVND_KEY_LEFT: return ImGuiKey_LeftArrow;
        case LVND_KEY_RIGHT: return ImGuiKey_RightArrow;
        case LVND_KEY_UP: return ImGuiKey_UpArrow;
        case LVND_KEY_DOWN: return ImGuiKey_DownArrow;
        case LVND_KEY_PAGE_UP: return ImGuiKey_PageUp;
        case LVND_KEY_PAGE_DOWN: return ImGuiKey_PageDown;
        case LVND_KEY_HOME: return ImGuiKey_Home;
        case LVND_KEY_END: return ImGuiKey_End;
        case LVND_KEY_INSERT: return ImGuiKey_Insert;
        case LVND_KEY_DELETE: return ImGuiKey_Delete;
        case LVND_KEY_BACKSPACE: return ImGuiKey_Backspace;
        case LVND_KEY_SPACE: return ImGuiKey_Space;
        case LVND_KEY_ENTER: return ImGuiKey_Enter;
        case LVND_KEY_ESCAPE: return ImGuiKey_Escape;
        case LVND_KEY_APOSTROPHE: return ImGuiKey_Apostrophe;
        case LVND_KEY_COMMA: return ImGuiKey_Comma;
        case LVND_KEY_MINUS: return ImGuiKey_Minus;
        case LVND_KEY_PERIOD: return ImGuiKey_Period;
        case LVND_KEY_SLASH: return ImGuiKey_Slash;
        case LVND_KEY_SEMICOLON: return ImGuiKey_Semicolon;
        case LVND_KEY_EQUAL: return ImGuiKey_Equal;
        case LVND_KEY_LEFT_BRACKET: return ImGuiKey_LeftBracket;
        case LVND_KEY_BACKSLASH: return ImGuiKey_Backslash;
        case LVND_KEY_RIGHT_BRACKET: return ImGuiKey_RightBracket;
        case LVND_KEY_GRAVE_ACCENT: return ImGuiKey_GraveAccent;
        case LVND_KEY_CAPS_LOCK: return ImGuiKey_CapsLock;
        case LVND_KEY_SCROLL_LOCK: return ImGuiKey_ScrollLock;
        case LVND_KEY_NUM_LOCK: return ImGuiKey_NumLock;
        case LVND_KEY_PRINT_SCREEN: return ImGuiKey_PrintScreen;
        case LVND_KEY_PAUSE: return ImGuiKey_Pause;
        case LVND_KEY_KP_0: return ImGuiKey_Keypad0;
        case LVND_KEY_KP_1: return ImGuiKey_Keypad1;
        case LVND_KEY_KP_2: return ImGuiKey_Keypad2;
        case LVND_KEY_KP_3: return ImGuiKey_Keypad3;
        case LVND_KEY_KP_4: return ImGuiKey_Keypad4;
        case LVND_KEY_KP_5: return ImGuiKey_Keypad5;
        case LVND_KEY_KP_6: return ImGuiKey_Keypad6;
        case LVND_KEY_KP_7: return ImGuiKey_Keypad7;
        case LVND_KEY_KP_8: return ImGuiKey_Keypad8;
        case LVND_KEY_KP_9: return ImGuiKey_Keypad9;
        case LVND_KEY_KP_DECIMAL: return ImGuiKey_KeypadDecimal;
        case LVND_KEY_KP_DIVIDE: return ImGuiKey_KeypadDivide;
        case LVND_KEY_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
        case LVND_KEY_KP_SUBTRACT: return ImGuiKey_KeypadSubtract;
        case LVND_KEY_KP_ADD: return ImGuiKey_KeypadAdd;
        case LVND_KEY_KP_ENTER: return ImGuiKey_KeypadEnter;
        case LVND_KEY_KP_EQUAL: return ImGuiKey_KeypadEqual;
        case LVND_KEY_LEFT_SHIFT: return ImGuiKey_LeftShift;
        case LVND_KEY_LEFT_CONTROL: return ImGuiKey_LeftCtrl;
        case LVND_KEY_LEFT_ALT: return ImGuiKey_LeftAlt;
        case LVND_KEY_LEFT_SUPER: return ImGuiKey_LeftSuper;
        case LVND_KEY_RIGHT_SHIFT: return ImGuiKey_RightShift;
        case LVND_KEY_RIGHT_CONTROL: return ImGuiKey_RightCtrl;
        case LVND_KEY_RIGHT_ALT: return ImGuiKey_RightAlt;
        case LVND_KEY_RIGHT_SUPER: return ImGuiKey_RightSuper;
        case LVND_KEY_MENU: return ImGuiKey_Menu;
        case LVND_KEY_0: return ImGuiKey_0;
        case LVND_KEY_1: return ImGuiKey_1;
        case LVND_KEY_2: return ImGuiKey_2;
        case LVND_KEY_3: return ImGuiKey_3;
        case LVND_KEY_4: return ImGuiKey_4;
        case LVND_KEY_5: return ImGuiKey_5;
        case LVND_KEY_6: return ImGuiKey_6;
        case LVND_KEY_7: return ImGuiKey_7;
        case LVND_KEY_8: return ImGuiKey_8;
        case LVND_KEY_9: return ImGuiKey_9;
        case LVND_KEY_A: return ImGuiKey_A;
        case LVND_KEY_B: return ImGuiKey_B;
        case LVND_KEY_C: return ImGuiKey_C;
        case LVND_KEY_D: return ImGuiKey_D;
        case LVND_KEY_E: return ImGuiKey_E;
        case LVND_KEY_F: return ImGuiKey_F;
        case LVND_KEY_G: return ImGuiKey_G;
        case LVND_KEY_H: return ImGuiKey_H;
        case LVND_KEY_I: return ImGuiKey_I;
        case LVND_KEY_J: return ImGuiKey_J;
        case LVND_KEY_K: return ImGuiKey_K;
        case LVND_KEY_L: return ImGuiKey_L;
        case LVND_KEY_M: return ImGuiKey_M;
        case LVND_KEY_N: return ImGuiKey_N;
        case LVND_KEY_O: return ImGuiKey_O;
        case LVND_KEY_P: return ImGuiKey_P;
        case LVND_KEY_Q: return ImGuiKey_Q;
        case LVND_KEY_R: return ImGuiKey_R;
        case LVND_KEY_S: return ImGuiKey_S;
        case LVND_KEY_T: return ImGuiKey_T;
        case LVND_KEY_U: return ImGuiKey_U;
        case LVND_KEY_V: return ImGuiKey_V;
        case LVND_KEY_W: return ImGuiKey_W;
        case LVND_KEY_X: return ImGuiKey_X;
        case LVND_KEY_Y: return ImGuiKey_Y;
        case LVND_KEY_Z: return ImGuiKey_Z;
        case LVND_KEY_F1: return ImGuiKey_F1;
        case LVND_KEY_F2: return ImGuiKey_F2;
        case LVND_KEY_F3: return ImGuiKey_F3;
        case LVND_KEY_F4: return ImGuiKey_F4;
        case LVND_KEY_F5: return ImGuiKey_F5;
        case LVND_KEY_F6: return ImGuiKey_F6;
        case LVND_KEY_F7: return ImGuiKey_F7;
        case LVND_KEY_F8: return ImGuiKey_F8;
        case LVND_KEY_F9: return ImGuiKey_F9;
        case LVND_KEY_F10: return ImGuiKey_F10;
        case LVND_KEY_F11: return ImGuiKey_F11;
        case LVND_KEY_F12: return ImGuiKey_F12;
        default: return ImGuiKey_None;
    }
}

static int ImGui_ImplLvnd_KeyToModifier(int key) {
    if (key == LVND_KEY_LEFT_CONTROL || key == LVND_KEY_RIGHT_CONTROL)
        return LVND_MODIFIER_CONTROL;
    if (key == LVND_KEY_LEFT_SHIFT || key == LVND_KEY_RIGHT_SHIFT)
        return LVND_MODIFIER_SHIFT;
    if (key == LVND_KEY_LEFT_ALT || key == LVND_KEY_RIGHT_ALT)
        return LVND_MODIFIER_ALT;
    if (key == LVND_KEY_LEFT_SUPER || key == LVND_KEY_RIGHT_SUPER)
        return LVND_MODIFIER_SUPER;
    
    return 0;
}

static void ImGui_ImplLvnd_UpdateKeyModifiers(uint16_t mods)
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiMod_Ctrl, (mods & LVND_MODIFIER_CONTROL) != 0);
    io.AddKeyEvent(ImGuiMod_Shift, (mods & LVND_MODIFIER_SHIFT) != 0);
    io.AddKeyEvent(ImGuiMod_Alt, (mods & LVND_MODIFIER_ALT) != 0);
    io.AddKeyEvent(ImGuiMod_Super, (mods & LVND_MODIFIER_SUPER) != 0);
}

void ImGui_ImplLvnd_MouseButtonCallback(LvndWindow* window, LvndMouseButton mouseButton, LvndState state) {
    ImGui_ImplLvnd_Data* bd = ImGui_ImplLvnd_GetBackendData();
    if (bd->PrevUserCallbackMousebutton != nullptr && window == bd->Window)
        bd->PrevUserCallbackMousebutton(window, mouseButton, state);

    ImGui_ImplLvnd_UpdateKeyModifiers(window->modifiers);

    ImGuiIO& io = ImGui::GetIO();
    if ((int)mouseButton >= 0 && (int)mouseButton < ImGuiMouseButton_COUNT)
        io.AddMouseButtonEvent(mouseButton, state == LVND_STATE_PRESSED);
}

void ImGui_ImplLvnd_ScrollCallback(LvndWindow* window, double xoffset, double yoffset) {
    ImGui_ImplLvnd_Data* bd = ImGui_ImplLvnd_GetBackendData();
    if (bd->PrevUserCallbackScroll != nullptr && window == bd->Window)
        bd->PrevUserCallbackScroll(window, xoffset, yoffset);

    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseWheelEvent((float)xoffset, (float)yoffset);
}

static int ImGui_ImplLvnd_TranslateUntranslatedKey(int key, int scancode) {
    IM_UNUSED(scancode);

    return key;
}

void ImGui_ImplLvnd_KeyCallback(LvndWindow* window, LvndKey key, LvndState state) {
    ImGui_ImplLvnd_Data* bd = ImGui_ImplLvnd_GetBackendData();
    if (bd->PrevUserCallbackKey != nullptr && window == bd->Window)
        bd->PrevUserCallbackKey(window, key, state);

    if (state != LVND_STATE_PRESSED && state != LVND_STATE_RELEASED)
        return;

    // Workaround: X11 does not include current pressed/released modifier key in 'mods' flags.
    uint16_t mods = window->modifiers;
    if (int keycode_to_mod = ImGui_ImplLvnd_KeyToModifier(key))
        mods = (state == LVND_STATE_PRESSED) ? (mods | keycode_to_mod) : (mods & ~keycode_to_mod);
    ImGui_ImplLvnd_UpdateKeyModifiers(mods);

    //TODO: implement this
    //keycode = ImGui_ImplLvnd_TranslateUntranslatedKey(keycode, scancode);

    ImGuiIO& io = ImGui::GetIO();
    ImGuiKey imgui_key = ImGui_ImplLvnd_KeyToImGuiKey(key);
    io.AddKeyEvent(imgui_key, (state == LVND_STATE_PRESSED));
    //TODO: implement this
    //io.SetKeyEventNativeData(imgui_key, keycode, scancode); // To support legacy indexing (<1.87 user code)
}

void ImGui_ImplLvnd_WindowFocusCallback(LvndWindow* window, bool focused) {
    ImGui_ImplLvnd_Data* bd = ImGui_ImplLvnd_GetBackendData();
    if (bd->PrevUserCallbackWindowFocus != nullptr && window == bd->Window)
        bd->PrevUserCallbackWindowFocus(window, focused);

    ImGuiIO& io = ImGui::GetIO();
    io.AddFocusEvent(focused != false);
}

void ImGui_ImplLvnd_CursorPosCallback(LvndWindow* window, int32_t xpos, int32_t ypos) {
    ImGui_ImplLvnd_Data* bd = ImGui_ImplLvnd_GetBackendData();
    if (bd->PrevUserCallbackCursorPos != nullptr && window == bd->Window)
        bd->PrevUserCallbackCursorPos(window, xpos, ypos);

    uint16_t w, h;
    lvndGetWindowSize(window, &w, &h);

    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent((float)xpos, (float)(h - ypos));
    bd->LastValidMousePos = ImVec2((float)xpos, (float)(h - ypos));
}

// Workaround: X11 seems to send spurious Leave/Enter events which would make us lose our position,
// so we back it up and restore on Leave/Enter (see https://github.com/ocornut/imgui/issues/4984)
void ImGui_ImplLvnd_CursorEnterCallback(LvndWindow* window, bool entered) {
    ImGui_ImplLvnd_Data* bd = ImGui_ImplLvnd_GetBackendData();
    if (bd->PrevUserCallbackCursorEnter != nullptr && window == bd->Window)
        bd->PrevUserCallbackCursorEnter(window, entered);

    ImGuiIO& io = ImGui::GetIO();
    if (entered)
    {
        bd->MouseWindow = window;
        io.AddMousePosEvent(bd->LastValidMousePos.x, bd->LastValidMousePos.y);
    }
    else if (!entered && bd->MouseWindow == window)
    {
        bd->LastValidMousePos = io.MousePos;
        bd->MouseWindow = nullptr;
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    }
}

void ImGui_ImplLvnd_CharCallback(LvndWindow* window, uint16_t c) {
    ImGui_ImplLvnd_Data* bd = ImGui_ImplLvnd_GetBackendData();
    if (bd->PrevUserCallbackChar != nullptr && window == bd->Window)
        bd->PrevUserCallbackChar(window, c);

    ImGuiIO& io = ImGui::GetIO();
    io.AddInputCharacter(c);
}

void ImGui_ImplLvnd_InstallCallbacks(LvndWindow* window) {
    ImGui_ImplLvnd_Data* bd = ImGui_ImplLvnd_GetBackendData();
    IM_ASSERT(bd->InstalledCallbacks == false && "Callbacks already installed!");
    IM_ASSERT(bd->Window == window);

    bd->PrevUserCallbackWindowFocus = lvndSetWindowFocusCallback(window, ImGui_ImplLvnd_WindowFocusCallback);
    bd->PrevUserCallbackCursorEnter = lvndSetCursorEnteredCallback(window, ImGui_ImplLvnd_CursorEnterCallback);
    bd->PrevUserCallbackCursorPos = lvndSetCursorMovedCallback(window, ImGui_ImplLvnd_CursorPosCallback);
    bd->PrevUserCallbackMousebutton = lvndSetMouseButtonPressedCallback(window, ImGui_ImplLvnd_MouseButtonCallback);
    bd->PrevUserCallbackScroll = lvndSetScrollCallback(window, ImGui_ImplLvnd_ScrollCallback);
    bd->PrevUserCallbackKey = lvndSetKeyPressedCallback(window, ImGui_ImplLvnd_KeyCallback);
    bd->PrevUserCallbackChar = lvndSetCharCallback(window, ImGui_ImplLvnd_CharCallback);
    bd->InstalledCallbacks = true;
}

void ImGui_ImplLvnd_RestoreCallbacks(LvndWindow* window)
{
    ImGui_ImplLvnd_Data* bd = ImGui_ImplLvnd_GetBackendData();
    IM_ASSERT(bd->InstalledCallbacks == true && "Callbacks not installed!");
    IM_ASSERT(bd->Window == window);

    lvndSetWindowFocusCallback(window, bd->PrevUserCallbackWindowFocus);
    lvndSetCursorEnteredCallback(window, bd->PrevUserCallbackCursorEnter);
    lvndSetCursorMovedCallback(window, bd->PrevUserCallbackCursorPos);
    lvndSetMouseButtonPressedCallback(window, bd->PrevUserCallbackMousebutton);
    lvndSetScrollCallback(window, bd->PrevUserCallbackScroll);
    lvndSetKeyPressedCallback(window, bd->PrevUserCallbackKey);
    lvndSetCharCallback(window, bd->PrevUserCallbackChar);
    
    bd->InstalledCallbacks = false;
    bd->PrevUserCallbackWindowFocus = nullptr;
    bd->PrevUserCallbackCursorEnter = nullptr;
    bd->PrevUserCallbackCursorPos = nullptr;
    bd->PrevUserCallbackMousebutton = nullptr;
    bd->PrevUserCallbackScroll = nullptr;
    bd->PrevUserCallbackKey = nullptr;
    bd->PrevUserCallbackChar = nullptr;
}

static bool ImGui_ImplLvnd_Init(LvndWindow* window, bool install_callbacks, LvndClientApi client_api)
{
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.BackendPlatformUserData == nullptr && "Already initialized a platform backend!");
    //printf("LVND_VERSION: %d.%d.%d (%d)", LVND_VERSION_MAJOR, LVND_VERSION_MINOR, LVND_VERSION_REVISION, LVND_VERSION_COMBINED);

    // Setup backend capabilities flags
    ImGui_ImplLvnd_Data* bd = IM_NEW(ImGui_ImplLvnd_Data)();
    io.BackendPlatformUserData = (void*)bd;
    io.BackendPlatformName = "imgui_impl_lvnd";
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // We can honor GetMouseCursor() values (optional)
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // We can honor io.WantSetMousePos requests (optional, rarely used)

    bd->Window = window;
    bd->Time = 0.0;

    io.SetClipboardTextFn = ImGui_ImplLvnd_SetClipboardText;
    io.GetClipboardTextFn = ImGui_ImplLvnd_GetClipboardText;
    io.ClipboardUserData = bd->Window;

    // Set platform dependent data in viewport
    //TODO: implement this for the windows version
#if defined(_WIN32)
    //ImGui::GetMainViewport()->PlatformHandleRaw = (void*)lvndGetWin32Window(bd->Window);
#endif

    // Create mouse cursors
    // (By design, on X11 cursors are user configurable and some cursors may be missing. When a cursor doesn't exist,
    // Lvnd will emit an error which will often be printed by the app, so we temporarily disable error reporting.
    // Missing cursors will return nullptr and our _UpdateMouseCursor() function will use the Arrow cursor instead.)

    // Chain Lvnd callbacks: our callbacks will call the user's previously installed callbacks, if any.
    if (install_callbacks)
        ImGui_ImplLvnd_InstallCallbacks(window);

    bd->ClientApi = client_api;
    return true;
}

bool ImGui_ImplLvnd_InitForOpenGL(LvndWindow* window, bool install_callbacks)
{
    return ImGui_ImplLvnd_Init(window, install_callbacks, LvndClientApi_OpenGL);
}

bool ImGui_ImplLvnd_InitForVulkan(LvndWindow* window, bool install_callbacks)
{
    return ImGui_ImplLvnd_Init(window, install_callbacks, LvndClientApi_Vulkan);
}

bool ImGui_ImplLvnd_InitForOther(LvndWindow* window, bool install_callbacks)
{
    return ImGui_ImplLvnd_Init(window, install_callbacks, LvndClientApi_Unknown);
}

void ImGui_ImplLvnd_Shutdown()
{
    ImGui_ImplLvnd_Data* bd = ImGui_ImplLvnd_GetBackendData();
    IM_ASSERT(bd != nullptr && "No platform backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();

    if (bd->InstalledCallbacks)
        ImGui_ImplLvnd_RestoreCallbacks(bd->Window);

    io.BackendPlatformName = nullptr;
    io.BackendPlatformUserData = nullptr;
    IM_DELETE(bd);
}

static void ImGui_ImplLvnd_UpdateMouseData()
{
    ImGui_ImplLvnd_Data* bd = ImGui_ImplLvnd_GetBackendData();
    ImGuiIO& io = ImGui::GetIO();

#ifdef __EMSCRIPTEN__
    const bool is_app_focused = true;
#else
    //TODO: implement this
    const bool is_app_focused = true;//lvndGetWindowAttrib(bd->Window, LVND_FOCUSED) != 0;
#endif
    if (is_app_focused)
    {
        // (Optional) Set OS mouse position from Dear ImGui if requested (rarely used, only when ImGuiConfigFlags_NavEnableSetMousePos is enabled by user)
        if (io.WantSetMousePos)
            lvndSetCursorPosition(bd->Window, (double)io.MousePos.x, (double)io.MousePos.y);

        // (Optional) Fallback to provide mouse position when focused (ImGui_ImplLvnd_CursorPosCallback already provides this when hovered or captured)
        if (is_app_focused && bd->MouseWindow == nullptr)
        {
            int32_t mouse_x, mouse_y;
            lvndGetCursorPosition(bd->Window, &mouse_x, &mouse_y);
            io.AddMousePosEvent((float)mouse_x, (float)mouse_y);
            bd->LastValidMousePos = ImVec2((float)mouse_x, (float)mouse_y);
        }
    }
}

static void ImGui_ImplLvnd_UpdateMouseCursor()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplLvnd_Data* bd = ImGui_ImplLvnd_GetBackendData();
    if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange))
        return;

    ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
    if (imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor) {
        // Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
        lvndSetCursorState(bd->Window, LVND_CURSOR_STATE_HIDDEN);
    }
    else {
        // Show OS mouse cursor
        // FIXME-PLATFORM: Unfocused windows seems to fail changing the mouse cursor with Lvnd 3.2, but 3.3 works here.
        lvndSetCursorState(bd->Window, LVND_CURSOR_STATE_NORMAL);
    }
}

// Update gamepad inputs
static inline float Saturate(float v) { return v < 0.0f ? 0.0f : v  > 1.0f ? 1.0f : v; }

void ImGui_ImplLvnd_NewFrame()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplLvnd_Data* bd = ImGui_ImplLvnd_GetBackendData();
    IM_ASSERT(bd != nullptr && "Did you call ImGui_ImplLvnd_InitForXXX()?");

    // Setup display size (every frame to accommodate for window resizing)
    uint16_t w, h;
    uint16_t display_w, display_h;
    lvndGetWindowSize(bd->Window, &w, &h);
    lvndWindowGetFramebufferSize(bd->Window, &display_w, &display_h);
    io.DisplaySize = ImVec2((float)w, (float)h);
    if (w > 0 && h > 0)
        io.DisplayFramebufferScale = ImVec2((float)display_w / (float)w, (float)display_h / (float)h);

    // Setup time step
    double current_time = lvndGetTime();
    io.DeltaTime = bd->Time > 0.0 ? (float)(current_time - bd->Time) : (float)(1.0f / 60.0f);
    bd->Time = current_time;

    ImGui_ImplLvnd_UpdateMouseData();
    ImGui_ImplLvnd_UpdateMouseCursor();
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
