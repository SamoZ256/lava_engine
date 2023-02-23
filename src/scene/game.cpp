#include "game.hpp"

Game* g_game = nullptr;

void Game::destroy() {
    for (auto& scene : scenes) {
        if (scene.loaded)
            scene.destroy();
    }
}

Scene& Game::addScene() {
    scenes.emplace_back(deferredLayout);

    return scenes[scenes.size() - 1];
}

void Game::changeScene(uint16_t newScene) {
    activeScene = newScene;
    if (!scenes[activeScene].loaded) {
        scenes[activeScene].loadFromFile();
    }
}

void Game::loadFromFile() {
    nh::json openJSON;
    std::string openText = lv::readFile("data/open_data.json");
    openJSON = nh::json::parse(openText);

    filename = openJSON["openFilename"];
    std::string directory = filename.substr(0, filename.find_last_of("/"));
    assetPath = directory + "/assets";
    lv::g_scriptManager->scriptDir = assetPath + "/scripts";

    Scene::meshDataDir = directory + "/assets/scenes/meshData";

    nh::json JSON;
    std::string text = lv::readFile(filename.c_str());
    JSON = nh::json::parse(text);

    uint8_t sceneCount = (uint8_t)JSON["sceneCount"];

    for (uint8_t i = 0; i < sceneCount; i++) {
        addScene();
        scenes[scenes.size() - 1].filename = directory + "/assets/scenes/scene" + std::to_string(scenes.size() - 1) + ".json";
    }
    changeScene((uint16_t)JSON["activeScene"]);
}

void Game::saveToFile() {
    for (uint16_t i = 0; i < scenes.size(); i++) {
        if (scenes[i].loaded) {
            //scenes[i].filename = directory + "/Assets/Scenes/scene" + std::to_string(i) + ".json";
            scenes[i].saveToFile();
        }
    }

    nh::json JSON;

    JSON["sceneCount"] = scenes.size();
    JSON["activeScene"] = activeScene;

    std::ofstream out(filename);
    out << std::setw(4) << JSON;
}
