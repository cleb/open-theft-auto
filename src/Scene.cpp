#include "Scene.hpp"
#include "Renderer.hpp"
#include "InputManager.hpp"
#include "LevelSerialization.hpp"
#include "GameLogic.hpp"
#include <iostream>
#include <string>
#include <glm/glm.hpp>
#include <glm/geometric.hpp>
#include <GLFW/glfw3.h>
#include <imgui.h>

Scene::Scene() : m_gameLogic(nullptr) {
}

bool Scene::initialize(GameLogic* gameLogic, Window* window, Renderer* renderer) {
    m_gameLogic = gameLogic;
    
    // Initialize tile grid
    m_tileGrid = std::make_unique<TileGrid>(glm::ivec3(16, 16, 4), 3.0f);
    if (!m_tileGrid->initialize()) {
        std::cerr << "Failed to initialize tile grid" << std::endl;
        return false;
    }

    m_tileGridEditor = std::make_unique<TileGridEditor>();
    m_tileGridEditor->initialize(m_tileGrid.get(), &m_levelData);
    m_tileGridEditor->setWindow(window);
    m_tileGridEditor->setRenderer(renderer);
    
    // Initialize player
    m_player = std::make_unique<Player>();
    if (!m_player->initialize()) {
        std::cerr << "Failed to initialize player" << std::endl;
        return false;
    }
    m_player->setTileGrid(m_tileGrid.get());
    
    // Initialize game logic
    m_gameLogic->setPlayer(m_player.get());
    m_gameLogic->setVehicles(&m_vehicles);
    
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

    // Update game logic (handles player/vehicle synchronization)
    if (m_gameLogic) {
        m_gameLogic->update(deltaTime);
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
        } else if (m_gameLogic && m_gameLogic->isPlayerInVehicle()) {
            Vehicle* activeVehicle = m_gameLogic->getActiveVehicle();
            if (activeVehicle) {
                target = activeVehicle->getPosition();
            }
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
    
    // Render vehicles
    for (auto& vehicle : m_vehicles) {
        if (vehicle && vehicle->isActive()) {
            vehicle->render(renderer);
        }
    }
    
    // Render player (on top)
    if (m_player && (!m_gameLogic || !m_gameLogic->isPlayerInVehicle())) {
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

    // Delegate all game input to GameLogic
    if (m_gameLogic) {
        m_gameLogic->processInput(input, deltaTime);
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

void Scene::toggleEditMode() {
    if (!m_tileGridEditor || !m_tileGrid) {
        return;
    }

    if (m_gameLogic && m_gameLogic->isPlayerInVehicle()) {
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

void Scene::rebuildVehiclesFromSpawns() {
    // Reset game logic
    if (m_gameLogic) {
        m_gameLogic->reset();
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