#pragma once

#include <vector>
#include <memory>
#include "GameObject.hpp"
#include "Player.hpp"
#include "Vehicle.hpp"
#include "TileGrid.hpp"
#include "TileGridEditor.hpp"
#include "LevelData.hpp"
#include "GameLogic.hpp"

#include <string>

class InputManager;

class Scene {
private:
    std::vector<std::unique_ptr<GameObject>> m_gameObjects;
    std::unique_ptr<Player> m_player;
    std::vector<std::unique_ptr<Vehicle>> m_vehicles;
    
    // New tile grid system
    std::unique_ptr<TileGrid> m_tileGrid;
    std::unique_ptr<TileGridEditor> m_tileGridEditor;
    LevelData m_levelData;
    std::string m_levelPath;

    // Game logic handler (owned by Engine)
    GameLogic* m_gameLogic;

public:
    Scene();
    ~Scene() = default;
    
    bool initialize(GameLogic* gameLogic, class Window* window, class Renderer* renderer);
    void update(float deltaTime);
    void render(class Renderer* renderer);
    void processInput(InputManager* input, float deltaTime);
    void drawGui();
    
    void addGameObject(std::unique_ptr<GameObject> object);
    void addVehicle(std::unique_ptr<Vehicle> vehicle);

    Player* getPlayer() const { return m_player.get(); }
    TileGrid* getTileGrid() const { return m_tileGrid.get(); }
    GameLogic* getGameLogic() const { return m_gameLogic; }
    bool isEditModeActive() const { return m_tileGridEditor && m_tileGridEditor->isEnabled(); }
    
private:
    void createTestScene();
    void toggleEditMode();
    void rebuildVehiclesFromSpawns();
};
