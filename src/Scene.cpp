#include "Scene.hpp"
#include "Renderer.hpp"
#include <iostream>
#include <glm/glm.hpp>
#include <glm/geometric.hpp>

Scene::Scene() {
}

bool Scene::initialize() {
    // Initialize tile grid
    m_tileGrid = std::make_unique<TileGrid>(glm::ivec3(16, 16, 4), 3.0f);
    if (!m_tileGrid->initialize()) {
        std::cerr << "Failed to initialize tile grid" << std::endl;
        return false;
    }
    
    // Initialize player
    m_player = std::make_unique<Player>();
    if (!m_player->initialize()) {
        std::cerr << "Failed to initialize player" << std::endl;
        return false;
    }
    m_player->setTileGrid(m_tileGrid.get());
    
    // Create test scene
    createTestScene();
    
    return true;
}

void Scene::update(float deltaTime) {
    // Update player
    if (m_player) {
        m_player->update(deltaTime);
    }

    if (m_playerInVehicle && m_activeVehicle && m_player) {
        m_player->setPosition(m_activeVehicle->getPosition());
        m_player->setRotation(m_activeVehicle->getRotation());
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
    if (renderer->getCamera()) {
        glm::vec3 target = glm::vec3(0.0f);
        if (m_playerInVehicle && m_activeVehicle) {
            target = m_activeVehicle->getPosition();
        } else if (m_player) {
            target = m_player->getPosition();
        }
        renderer->getCamera()->followTarget(target);
    }
    
    // Render tile grid (replaces roads and buildings)
    if (m_tileGrid) {
        m_tileGrid->render(renderer);
    }
    
    // Render roads first (bottom layer) - legacy, can be removed
    for (auto& road : m_roads) {
        if (road && road->isActive()) {
            road->render(renderer);
        }
    }
    
    // Render buildings - legacy, can be removed
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
    if (m_player && !m_playerInVehicle) {
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
    // Configure the tile grid with test data
    if (m_tileGrid) {
        m_tileGrid->createTestGrid();
    }
    
    // Add a car nearby
    auto car = std::make_unique<Vehicle>();
    car->initialize("assets/textures/car.png");
    car->setSpriteSize(glm::vec2(1.5f, 3.0f));
    car->setPosition(glm::vec3(15.0f, 15.0f, 0.0f));  // Place on road
    addVehicle(std::move(car));
    
    std::cout << "Created test scene with tile grid and " 
              << m_vehicles.size() << " vehicles" << std::endl;
}

bool Scene::tryEnterNearestVehicle(float radius) {
    if (!m_player || m_playerInVehicle) {
        return false;
    }

    Vehicle* nearestVehicle = nullptr;
    float nearestDistance = radius;
    const glm::vec2 playerPos(m_player->getPosition().x, m_player->getPosition().y);

    for (auto& vehicle : m_vehicles) {
        if (!vehicle || !vehicle->isActive()) {
            continue;
        }

        const glm::vec3 vehiclePos3 = vehicle->getPosition();
        const glm::vec2 vehiclePos(vehiclePos3.x, vehiclePos3.y);
        const float distance = glm::length(vehiclePos - playerPos);

        if (distance <= nearestDistance) {
            nearestVehicle = vehicle.get();
            nearestDistance = distance;
        }
    }

    if (!nearestVehicle) {
        return false;
    }

    m_playerInVehicle = true;
    m_activeVehicle = nearestVehicle;
    m_activeVehicle->setPlayerControlled(true);
    m_player->setActive(false);
    m_player->setPosition(m_activeVehicle->getPosition());
    m_player->setRotation(m_activeVehicle->getRotation());
    return true;
}