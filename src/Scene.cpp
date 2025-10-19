#include "Scene.hpp"
#include "Renderer.hpp"
#include <iostream>

Scene::Scene() {
}

bool Scene::initialize() {
    // Initialize player
    m_player = std::make_unique<Player>();
    if (!m_player->initialize()) {
        std::cerr << "Failed to initialize player" << std::endl;
        return false;
    }
    
    // Create test scene
    createTestScene();
    
    return true;
}

void Scene::update(float deltaTime) {
    // Update player
    if (m_player) {
        m_player->update(deltaTime);
    }
    
    // Update all game objects
    for (auto& obj : m_gameObjects) {
        if (obj && obj->isActive()) {
            obj->update(deltaTime);
        }
    }
    
    // Update vehicles
    for (auto& vehicle : m_vehicles) {
        if (vehicle && vehicle->isActive()) {
            vehicle->update(deltaTime);
        }
    }
    
    // Update buildings (they're static, but for consistency)
    for (auto& building : m_buildings) {
        if (building && building->isActive()) {
            building->update(deltaTime);
        }
    }
    
    // Update roads (they're static, but for consistency)
    for (auto& road : m_roads) {
        if (road && road->isActive()) {
            road->update(deltaTime);
        }
    }
}

void Scene::render(Renderer* renderer) {
    if (!renderer) return;
    
    // Update camera to follow player
    if (m_player && renderer->getCamera()) {
        renderer->getCamera()->followTarget(m_player->getPosition(), 5.0f, 0.016f);
    }
    
    // Render roads first (bottom layer)
    for (auto& road : m_roads) {
        if (road && road->isActive()) {
            road->render(renderer);
        }
    }
    
    // Render buildings
    for (auto& building : m_buildings) {
        if (building && building->isActive()) {
            building->render(renderer);
        }
    }
    
    // Render vehicles
    for (auto& vehicle : m_vehicles) {
        if (vehicle && vehicle->isActive()) {
            vehicle->render(renderer);
        }
    }
    
    // Render player (on top)
    if (m_player) {
        m_player->render(renderer);
    }
    
    // Render other game objects
    for (auto& obj : m_gameObjects) {
        if (obj && obj->isActive()) {
            obj->render(renderer);
        }
    }
}

void Scene::addGameObject(std::unique_ptr<GameObject> object) {
    m_gameObjects.push_back(std::move(object));
}

void Scene::addVehicle(std::unique_ptr<Vehicle> vehicle) {
    m_vehicles.push_back(std::move(vehicle));
}

void Scene::addBuilding(std::unique_ptr<Building> building) {
    m_buildings.push_back(std::move(building));
}

void Scene::addRoad(std::unique_ptr<Road> road) {
    m_roads.push_back(std::move(road));
}

void Scene::createTestScene() {
    // Create road grid
    for (int x = -4; x <= 4; x++) {
        for (int y = -4; y <= 4; y++) {
            auto road = std::make_unique<Road>();
            road->initialize(glm::vec2(3.0f, 3.0f));
            road->setPosition(glm::vec3(x * 3.0f, y * 3.0f, 0.0f));
            addRoad(std::move(road));
        }
    }
    
    // Create some test buildings along the edges
    for (int x = -3; x <= 3; x += 2) {
        for (int y = -3; y <= 3; y += 2) {
            if (x == 0 && y == 0) continue; // Don't place building at player start
            if (x == 1 && y == -1) continue; // Leave space for car
            
            auto building = std::make_unique<Building>();
            float height = 2.0f + static_cast<float>(rand() % 6); // Random height 2-8
            building->initialize(glm::vec3(1.5f, 1.5f, height));
            building->setPosition(glm::vec3(x * 3.0f, y * 3.0f, 0.0f));
            addBuilding(std::move(building));
        }
    }
    
    // Add a car nearby
    auto car = std::make_unique<Vehicle>();
    car->initialize("");
    car->setPosition(glm::vec3(3.0f, -3.0f, 0.0f));
    addVehicle(std::move(car));
    
    std::cout << "Created test scene with " << m_roads.size() << " road tiles, " 
              << m_buildings.size() << " buildings, and " << m_vehicles.size() << " vehicles" << std::endl;
}