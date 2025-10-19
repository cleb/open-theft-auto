#pragma once

#include <vector>
#include <memory>
#include "GameObject.hpp"
#include "Player.hpp"
#include "Vehicle.hpp"
#include "Building.hpp"
#include "Road.hpp"

class Scene {
private:
    std::vector<std::unique_ptr<GameObject>> m_gameObjects;
    std::unique_ptr<Player> m_player;
    std::vector<std::unique_ptr<Vehicle>> m_vehicles;
    std::vector<std::unique_ptr<Building>> m_buildings;
    std::vector<std::unique_ptr<Road>> m_roads;

public:
    Scene();
    ~Scene() = default;
    
    bool initialize();
    void update(float deltaTime);
    void render(class Renderer* renderer);
    
    void addGameObject(std::unique_ptr<GameObject> object);
    void addVehicle(std::unique_ptr<Vehicle> vehicle);
    void addBuilding(std::unique_ptr<Building> building);
    void addRoad(std::unique_ptr<Road> road);
    
    Player* getPlayer() const { return m_player.get(); }
    
private:
    void createTestScene();
};