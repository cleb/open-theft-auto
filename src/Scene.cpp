#include "Scene.hpp"
#include "Renderer.hpp"
#include "InputManager.hpp"
#include "LevelSerialization.hpp"
#include <iostream>
#include <string>
#include <glm/glm.hpp>
#include <glm/geometric.hpp>
#include <GLFW/glfw3.h>
#include <imgui.h>

Scene::Scene() {
}

bool Scene::initialize() {
    // Initialize tile grid
    m_tileGrid = std::make_unique<TileGrid>(glm::ivec3(16, 16, 4), 3.0f);
    if (!m_tileGrid->initialize()) {
        std::cerr << "Failed to initialize tile grid" << std::endl;
        return false;
    }

    m_tileGridEditor = std::make_unique<TileGridEditor>();
    m_tileGridEditor->initialize(m_tileGrid.get(), &m_levelData);
    
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

    if (m_tileGridEditor && m_tileGridEditor->isEnabled()) {
        m_tileGridEditor->update(deltaTime);
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
    
    // Update camera target
    if (renderer->getCamera()) {
        glm::vec3 target(0.0f);
        if (m_tileGridEditor && m_tileGridEditor->isEnabled() && m_tileGrid) {
            const glm::ivec3 cursor = m_tileGridEditor->getCursor();
            target = m_tileGrid->gridToWorld(cursor);
            target.z += m_tileGrid->getTileSize();
        } else if (m_playerInVehicle && m_activeVehicle) {
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

    if (m_tileGridEditor) {
        m_tileGridEditor->render(renderer);
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

void Scene::drawGui() {
    if (m_tileGridEditor) {
        m_tileGridEditor->drawGui();
    }
}

void Scene::processInput(InputManager* input, float deltaTime) {
    if (!input) {
        return;
    }

    if (input->isKeyPressed(GLFW_KEY_F1)) {
        toggleEditMode();
    }

    const bool editActive = m_tileGridEditor && m_tileGridEditor->isEnabled();
    ImGuiIO& io = ImGui::GetIO();
    const bool captureKeyboard = io.WantCaptureKeyboard;

    if (editActive) {
        m_tileGridEditor->processInput(input);
        return;
    }

    if (captureKeyboard) {
        return;
    }

    if (input->isKeyPressed(GLFW_KEY_ENTER) || input->isKeyPressed(GLFW_KEY_KP_ENTER)) {
        if (!m_playerInVehicle) {
            m_playerInVehicle = tryEnterNearestVehicle();
        }
    }

    if (input->isKeyPressed(GLFW_KEY_F) && m_playerInVehicle) {
        leaveVehicle();
    }

    if (m_playerInVehicle && m_activeVehicle) {
        if (input->isKeyDown(GLFW_KEY_W) || input->isKeyDown(GLFW_KEY_UP)) {
            m_activeVehicle->accelerate(deltaTime);
        }
        if (input->isKeyDown(GLFW_KEY_S) || input->isKeyDown(GLFW_KEY_DOWN)) {
            m_activeVehicle->brake(deltaTime);
        }
        if (input->isKeyDown(GLFW_KEY_A) || input->isKeyDown(GLFW_KEY_LEFT)) {
            m_activeVehicle->turnRight(deltaTime);
        }
        if (input->isKeyDown(GLFW_KEY_D) || input->isKeyDown(GLFW_KEY_RIGHT)) {
            m_activeVehicle->turnLeft(deltaTime);
        }
        return;
    }

    if (Player* player = m_player.get()) {
        if (input->isKeyDown(GLFW_KEY_W) || input->isKeyDown(GLFW_KEY_UP)) {
            player->moveForward(deltaTime);
        }
        if (input->isKeyDown(GLFW_KEY_S) || input->isKeyDown(GLFW_KEY_DOWN)) {
            player->moveBackward(deltaTime);
        }
        if (input->isKeyDown(GLFW_KEY_A) || input->isKeyDown(GLFW_KEY_LEFT)) {
            player->turnLeft(deltaTime);
        }
        if (input->isKeyDown(GLFW_KEY_D) || input->isKeyDown(GLFW_KEY_RIGHT)) {
            player->turnRight(deltaTime);
        }
    }
}

void Scene::addGameObject(std::unique_ptr<GameObject> object) {
    m_gameObjects.push_back(std::move(object));
}

void Scene::addVehicle(std::unique_ptr<Vehicle> vehicle) {
    if (vehicle && m_tileGrid) {
        vehicle->setTileGrid(m_tileGrid.get());
    }
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
        const std::string levelPath = "assets/levels/test_grid.tg";
        if (!LevelSerialization::loadLevel(levelPath, *m_tileGrid, m_levelData)) {
            std::cerr << "Failed to load level from " << levelPath << std::endl;
        }
        m_levelPath = levelPath;
        if (m_tileGridEditor) {
            m_tileGridEditor->setLevelPath(m_levelPath);
            m_tileGridEditor->initialize(m_tileGrid.get(), &m_levelData);
        }
        rebuildVehiclesFromSpawns();
    }

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

void Scene::toggleEditMode() {
    if (!m_tileGridEditor || !m_tileGrid) {
        return;
    }

    if (m_playerInVehicle) {
        std::cout << "Exit the vehicle before entering edit mode." << std::endl;
        return;
    }

    const bool enable = !m_tileGridEditor->isEnabled();
    if (enable) {
        glm::ivec3 cursor(0);
        if (m_player) {
            cursor = m_tileGrid->worldToGrid(m_player->getPosition());
        }
        m_tileGridEditor->setLevelPath(m_levelPath);
        m_tileGridEditor->setCursor(cursor);
        m_tileGridEditor->setEnabled(true);
        if (m_player) {
            m_player->setActive(false);
        }
        std::cout << "Edit mode enabled" << std::endl;
    } else {
        m_tileGridEditor->setEnabled(false);
        rebuildVehiclesFromSpawns();
        if (m_player) {
            m_player->setActive(true);
        }
        std::cout << "Edit mode disabled" << std::endl;
    }
}

void Scene::leaveVehicle() {
    if (!m_playerInVehicle || !m_activeVehicle || !m_player) {
        return;
    }

    m_playerInVehicle = false;
    m_player->setActive(true);
    m_player->setPosition(m_activeVehicle->getPosition());
    m_player->setRotation(m_activeVehicle->getRotation());
    m_activeVehicle->setPlayerControlled(false);
    m_activeVehicle = nullptr;
}

void Scene::rebuildVehiclesFromSpawns() {
    m_playerInVehicle = false;
    m_activeVehicle = nullptr;

    for (auto& vehicle : m_vehicles) {
        if (vehicle) {
            vehicle->setPlayerControlled(false);
        }
    }
    m_vehicles.clear();

    if (!m_tileGrid) {
        return;
    }

    for (const auto& spawn : m_levelData.vehicleSpawns) {
        auto vehicle = std::make_unique<Vehicle>();
        const std::string texture = spawn.texturePath.empty() ? "assets/textures/car.png" : spawn.texturePath;
        vehicle->initialize(texture);
        vehicle->setSpriteSize(spawn.size);
        glm::vec3 position(
            spawn.gridPosition.x * m_tileGrid->getTileSize(),
            spawn.gridPosition.y * m_tileGrid->getTileSize(),
            spawn.gridPosition.z * m_tileGrid->getTileSize());
        position.z += 0.1f;
        vehicle->setPosition(position);
        vehicle->setRotation(glm::vec3(0.0f, 0.0f, spawn.rotationDegrees));
        addVehicle(std::move(vehicle));
    }

    if (m_player) {
        m_player->setActive(true);
    }

    std::cout << "Rebuilt vehicles from grid: " << m_vehicles.size() << std::endl;
}