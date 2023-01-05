#ifndef GAME_H
#define GAME_H

#include "lvutils/scripting/script_manager.hpp"

#include "scene.hpp"

class Game;

//Global
extern Game* g_game;

class Game {
public:
    std::vector<Scene> scenes;

    uint16_t activeScene = 0;

    lv::ScriptManager scriptManager;

    std::string filename;
    std::string assetPath;

#ifdef LV_BACKEND_VULKAN
    lv::PipelineLayout& deferredLayout;

    Game(lv::PipelineLayout& aDeferredLayout) : deferredLayout(aDeferredLayout) { g_game = this; }
#elif defined LV_BACKEND_METAL
    Game() { g_game = this; }
#endif

    void destroy();

    Scene& addScene();

    Scene& scene() { return scenes[activeScene]; }

    void changeScene(uint16_t newScene);

    void loadFromFile();

    void saveToFile();
};

#endif
