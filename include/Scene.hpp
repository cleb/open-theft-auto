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
#include "TrafficManager.hpp"
#include "PedestrianManager.hpp"
#include "PoliceChaseManager.hpp"
#include "Pickup.hpp"
#include "ProjectileManager.hpp"
#include "Collider.hpp"
#include "MissionSystem.hpp"
#include "Texture.hpp"

#include <string>

class InputManager;

class Scene {
private:
    std::vector<std::unique_ptr<GameObject>> m_gameObjects;
    std::unique_ptr<Player> m_player;
    std::vector<std::unique_ptr<Vehicle>> m_vehicles;
    std::vector<std::unique_ptr<Pickup>> m_pickups;

    // New tile grid system
    std::unique_ptr<TileGrid> m_tileGrid;
    std::unique_ptr<TileGridEditor> m_tileGridEditor;
    std::unique_ptr<TrafficManager> m_trafficManager;
    std::unique_ptr<PedestrianManager> m_pedestrianManager;
    std::unique_ptr<PoliceChaseManager> m_policeChaseManager;
    LevelData m_levelData;
    std::string m_levelPath;
    ProjectileManager m_projectileManager;

    // Game logic handler (owned by Engine)
    GameLogic* m_gameLogic;
    
    // Input manager reference for pilot assignment
    InputManager* m_inputManager;

    // Mission system
    MissionSystem m_missionSystem;

    // Phone booth rendering data
    struct PhoneBoothRuntime {
        glm::vec3 worldPos{0.0f};
        std::string id;
        std::string currentJobId;  // Advances through the mission chain after each completion
        std::shared_ptr<Texture> texInactive;
        std::shared_ptr<Texture> texActive;
    };
    std::vector<PhoneBoothRuntime> m_phoneBooths;

    // Mission HUD state
    bool m_showMissionPrompt = false;
    const Job* m_promptJob = nullptr;
    std::string m_promptBoothId;
    glm::vec3 m_promptBoothWorldPos{0.0f};
    float m_missionCompletedTimer = 0.0f;
    std::string m_completedBoothId;       // Which booth just finished a mission
    static constexpr float kMissionBannerTime = 4.0f;   // How long the completion banner shows
    static constexpr float kMissionCooldownTime = 8.0f;  // Total cooldown before next mission

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
    TrafficManager* getTrafficManager() const { return m_trafficManager.get(); }
    PedestrianManager* getPedestrianManager() const { return m_pedestrianManager.get(); }
    PoliceChaseManager* getPoliceChaseManager() const { return m_policeChaseManager.get(); }
    bool isEditModeActive() const { return m_tileGridEditor && m_tileGridEditor->isEnabled(); }
    
    // Get all collidable objects for collision detection
    std::vector<const Collider*> getAllColliders() const;
    
private:
    void createTestScene();
    void toggleEditMode();
    void rebuildVehiclesFromSpawns();
    void onLevelChanged();
    void setupCollisionCallbacks();
    void rebuildPickupsFromSpawns();
    void rebuildPhoneBoothsFromSpawns();
    void handlePickupCollection();
    void handlePhoneBoothInteraction(float deltaTime);
    void advanceBoothJob(const std::string& boothId);
    void fireWeaponShot();
    void handleVehicleExploded(Vehicle* vehicle);
    void restartLevel();
    void drawMissionGui();
};
