#include "Scene.hpp"
#include "Renderer.hpp"
#include "InputManager.hpp"
#include "LevelSerialization.hpp"
#include "GameLogic.hpp"
#include "TrafficManager.hpp"
#include "PedestrianManager.hpp"
#include "PoliceChaseManager.hpp"
#include "VehicleConfig.hpp"
#include "Window.hpp"
#include "Heading.hpp"
#include "PistolWeapon.hpp"
#include <iostream>
#include <string>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/geometric.hpp>
#include <GLFW/glfw3.h>
#include <imgui.h>

Scene::Scene() : m_gameLogic(nullptr), m_inputManager(nullptr) {
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
    m_tileGridEditor->setLevelChangedCallback([this]() { onLevelChanged(); });
    
    // Initialize player
    m_player = std::make_unique<Player>();
    if (!m_player->initialize()) {
        std::cerr << "Failed to initialize player" << std::endl;
        return false;
    }
    m_player->setTileGrid(m_tileGrid.get());
    
    // Initialize traffic manager
    m_trafficManager = std::make_unique<TrafficManager>();
    m_trafficManager->initialize(m_tileGrid.get(), renderer->getCamera(), &m_vehicles);
    // Set projection info for accurate view bounds calculation (FOV matches Renderer::initialize)
    m_trafficManager->setProjectionInfo(1.57f, window->getAspectRatio());
    m_trafficManager->setVehicleExplodeCallback([this](Vehicle* vehicle) {
        handleVehicleExploded(vehicle);
    });
    
    // Initialize pedestrian manager
    m_pedestrianManager = std::make_unique<PedestrianManager>();
    m_pedestrianManager->initialize(m_tileGrid.get(), renderer->getCamera());
    m_pedestrianManager->setProjectionInfo(1.57f, window->getAspectRatio());
    
    // Connect traffic manager to pedestrian manager for carjack callbacks
    m_trafficManager->setPedestrianManager(m_pedestrianManager.get());
    
    // Set vehicle callback for pedestrian collision detection
    m_pedestrianManager->setVehicleCallback([this]() -> std::vector<Vehicle*> {
        std::vector<Vehicle*> allVehicles;
        // Add player vehicles
        for (const auto& vehicle : m_vehicles) {
            if (vehicle && vehicle->isActive()) {
                allVehicles.push_back(vehicle.get());
            }
        }
        // Add traffic vehicles
        if (m_trafficManager) {
            for (const auto& vehicle : m_trafficManager->getTrafficVehicles()) {
                if (vehicle && vehicle->isActive()) {
                    allVehicles.push_back(vehicle.get());
                }
            }
        }
        return allVehicles;
    });
    
    // Initialize police chase manager
    m_policeChaseManager = std::make_unique<PoliceChaseManager>();
    m_policeChaseManager->initialize(m_tileGrid.get(), renderer->getCamera(), m_player.get(), m_trafficManager.get());
    m_policeChaseManager->setProjectionInfo(1.57f, window->getAspectRatio());
    
    // Set up player position callback for police to chase
    m_policeChaseManager->setPlayerPositionCallback([this]() -> glm::vec3 {
        if (m_gameLogic && m_gameLogic->isPlayerInVehicle()) {
            Vehicle* activeVehicle = m_gameLogic->getActiveVehicle();
            if (activeVehicle) {
                return activeVehicle->getPosition();
            }
        }
        if (m_player) {
            return m_player->getPosition();
        }
        return glm::vec3(0.0f);
    });
    
    // Set up pedestrian kill callback to notify police chase manager
    m_pedestrianManager->setPedestrianKillCallback([this](Vehicle* killerVehicle) {
        // Only count kills by the player's vehicle
        if (m_gameLogic && m_gameLogic->isPlayerInVehicle()) {
            Vehicle* playerVehicle = m_gameLogic->getActiveVehicle();
            if (playerVehicle == killerVehicle) {
                if (m_policeChaseManager) {
                    m_policeChaseManager->onPedestrianKilled();
                }
            }
        }
    });
    
    // Initialize game logic
    m_gameLogic->setPlayer(m_player.get());
    m_gameLogic->setVehicles(&m_vehicles);
    m_gameLogic->setTrafficManager(m_trafficManager.get());
    
    // Create test scene
    createTestScene();

    m_projectileManager.initialize();
    
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
    
    // Update traffic manager (AI vehicles)
    if (m_trafficManager && !isEditModeActive()) {
        m_trafficManager->update(deltaTime);
    }
    
    // Update pedestrian manager (AI pedestrians)
    if (m_pedestrianManager && !isEditModeActive()) {
        m_pedestrianManager->update(deltaTime);
    }

    if (!isEditModeActive()) {
        handlePickupCollection();
    m_projectileManager.update(deltaTime, m_pedestrianManager.get(), &m_vehicles, m_trafficManager.get());
        for (auto& pickup : m_pickups) {
            if (pickup && pickup->isActive()) {
                pickup->update(deltaTime);
            }
        }
    }
    
    // Update police chase manager
    if (m_policeChaseManager && !isEditModeActive()) {
        m_policeChaseManager->update(deltaTime);
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
    
    // Update camera target (skip in editor mode - edge scrolling controls camera)
    if (renderer->getCamera()) {
        const bool editorMode = m_tileGridEditor && m_tileGridEditor->isEnabled();
        if (!editorMode) {
            glm::vec3 target(0.0f);
            if (m_gameLogic && m_gameLogic->isPlayerInVehicle()) {
                Vehicle* activeVehicle = m_gameLogic->getActiveVehicle();
                if (activeVehicle) {
                    target = activeVehicle->getPosition();
                }
            } else if (m_player) {
                target = m_player->getPosition();
            }
            renderer->getCamera()->followTarget(target);
        }
    }
    
    // Render tile grid (replaces roads and buildings)
    if (m_tileGrid) {
        m_tileGrid->render(renderer);
    }

    if (m_tileGridEditor) {
        m_tileGridEditor->render(renderer);
    }

    if (!isEditModeActive()) {
        for (auto& pickup : m_pickups) {
            if (pickup && pickup->isActive()) {
                pickup->render(renderer);
            }
        }

        m_projectileManager.render(renderer);
    }
    
    // Render vehicles
    for (auto& vehicle : m_vehicles) {
        if (vehicle && vehicle->isActive()) {
            vehicle->render(renderer);
        }
    }
    
    // Render traffic vehicles
    if (m_trafficManager) {
        m_trafficManager->render(renderer);
        // Render debug spawn points if enabled
        m_trafficManager->renderDebugSpawnPoints(renderer);
    }
    
    // Render police vehicles
    if (m_policeChaseManager) {
        m_policeChaseManager->render(renderer);
    }
    
    // Render pedestrians
    if (m_pedestrianManager) {
        m_pedestrianManager->render(renderer);
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
    // Store reference to InputManager for pilot assignment
    if (input && !m_inputManager) {
        m_inputManager = input;
        if (m_gameLogic) {
            m_gameLogic->setInputManager(input);
        }
    }
    if (!input) {
        return;
    }

    if (input->isKeyPressed(GLFW_KEY_F1)) {
        toggleEditMode();
    }

    // Toggle traffic spawn point debug visualization with F2
    if (input->isKeyPressed(GLFW_KEY_F2)) {
        if (m_trafficManager) {
            bool enabled = !m_trafficManager->isDebugRenderSpawnPointsEnabled();
            m_trafficManager->setDebugRenderSpawnPoints(enabled);
            std::cout << "Traffic spawn point debug: " << (enabled ? "ON" : "OFF") << std::endl;
        }
    }

    const bool editActive = m_tileGridEditor && m_tileGridEditor->isEnabled();
    ImGuiIO& io = ImGui::GetIO();
    const bool captureKeyboard = io.WantCaptureKeyboard;

    if (editActive) {
        m_tileGridEditor->processInput(input, deltaTime);
        return;
    }

    if (captureKeyboard) {
        return;
    }

    // Delegate all game input to GameLogic
    if (m_gameLogic) {
        m_gameLogic->processInput(input, deltaTime);
    }

    const bool ctrlDown = input->isKeyDown(GLFW_KEY_LEFT_CONTROL) || input->isKeyDown(GLFW_KEY_RIGHT_CONTROL);
    if (!editActive && !captureKeyboard && ctrlDown && m_player && m_player->canShoot()
        && (!m_gameLogic || !m_gameLogic->isPlayerInVehicle())) {
        firePistolShot();
        m_player->recordShot();
    }
}

void Scene::addGameObject(std::unique_ptr<GameObject> object) {
    m_gameObjects.push_back(std::move(object));
}

void Scene::addVehicle(std::unique_ptr<Vehicle> vehicle) {
    if (vehicle && m_tileGrid) {
        vehicle->setTileGrid(m_tileGrid.get());
    }
    // Set up collision callback for the new vehicle
    if (vehicle) {
        vehicle->setCollisionCallback([this]() { return getAllColliders(); });
        vehicle->setExplodeCallback([this](Vehicle* exploded) { handleVehicleExploded(exploded); });
    }
    m_vehicles.push_back(std::move(vehicle));
}

void Scene::createTestScene() {
    // Configure the tile grid with test data
    if (m_tileGrid) {
        const std::string levelPath = "assets/levels/test_level2.tg";
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
    
    // Reset traffic manager
    if (m_trafficManager) {
        m_trafficManager->reset();
    }
    
    // Reset pedestrian manager
    if (m_pedestrianManager) {
        m_pedestrianManager->reset();
    }
    
    // Reset police chase manager
    if (m_policeChaseManager) {
        m_policeChaseManager->reset();
    }

    if (!m_tileGrid) {
        return;
    }

    rebuildPickupsFromSpawns();

    for (const auto& spawn : m_levelData.vehicleSpawns) {
        auto vehicle = std::make_unique<Vehicle>();
        
        // Set vehicle type first (this configures speed, acceleration, etc.)
        vehicle->setVehicleType(spawn.vehicleTypeId);
        
        // Get the type definition for texture/size defaults
        const auto& config = VehicleConfig::getInstance();
        const auto* typeDef = config.getDefinition(spawn.vehicleTypeId);
        
        // Use spawn texture if provided, otherwise use default for the vehicle type
        std::string texture = spawn.texturePath;
        if (texture.empty() && typeDef) {
            texture = typeDef->texturePath;
        }
        if (texture.empty()) {
            texture = "textures/car.png";
        }
        vehicle->initialize(texture);
        vehicle->setSpriteSize(spawn.size);
        glm::vec3 position(
            spawn.gridPosition.x * m_tileGrid->getTileSize(),
            spawn.gridPosition.y * m_tileGrid->getTileSize(),
            spawn.gridPosition.z * m_tileGrid->getTileSize());
        position.z += 0.1f;
        vehicle->setPosition(position);
    // rotationDegrees is a heading in degrees where 0°=+X (East), 90°=+Y (North), CCW positive.
    vehicle->setRotation(glm::vec3(0.0f, 0.0f, spawn.rotationDegrees));
        addVehicle(std::move(vehicle));
    }

    // Set player position from spawn point
    if (m_player) {
        if (m_levelData.playerSpawn.isSet) {
            const float tileSize = m_tileGrid->getTileSize();
            glm::vec3 playerPos(
                m_levelData.playerSpawn.gridPosition.x * tileSize,
                m_levelData.playerSpawn.gridPosition.y * tileSize,
                m_levelData.playerSpawn.gridPosition.z * tileSize);
            playerPos.z += 0.1f;  // Slightly above the tile surface
            m_player->setPosition(playerPos);
            // rotationDegrees is a heading in degrees where 0°=+X (East), 90°=+Y (North), CCW positive.
            m_player->setRotation(glm::vec3(0.0f, 0.0f, m_levelData.playerSpawn.rotationDegrees));
            std::cout << "Set player position to spawn: (" << playerPos.x << ", " << playerPos.y << ", " << playerPos.z 
                      << ") rotation=" << m_levelData.playerSpawn.rotationDegrees << std::endl;
        } else {
            // Default spawn at center of bottom layer
            const glm::ivec3& gridSize = m_tileGrid->getGridSize();
            const float tileSize = m_tileGrid->getTileSize();
            glm::vec3 defaultPos(
                (gridSize.x / 2) * tileSize,
                (gridSize.y / 2) * tileSize,
                0.1f);
            m_player->setPosition(defaultPos);
            m_player->setRotation(glm::vec3(0.0f));
        }
        m_player->setActive(true);
    }

    // Set up collision detection for all vehicles
    setupCollisionCallbacks();
    
    // Set up collision callback for traffic manager
    if (m_trafficManager) {
        m_trafficManager->setCollisionCallback([this]() { return getAllColliders(); });
    }
    
    // Set up collision callback for police chase manager
    if (m_policeChaseManager) {
        m_policeChaseManager->setCollisionCallback([this]() { return getAllColliders(); });
    }

    std::cout << "Rebuilt vehicles from grid: " << m_vehicles.size() << std::endl;
}

void Scene::onLevelChanged() {
    // Update the level path from the editor
    if (m_tileGridEditor) {
        m_levelPath = m_tileGridEditor->getLevelPath();
    }
    
    // Rebuild vehicles for the new level
    rebuildVehiclesFromSpawns();
    
    std::cout << "Level changed, rebuilt scene with " << m_vehicles.size() << " vehicles" << std::endl;
}

void Scene::rebuildPickupsFromSpawns() {
    m_pickups.clear();
    if (!m_tileGrid) {
        return;
    }

    for (const auto& spawn : m_levelData.pickups) {
        auto pickup = std::make_unique<Pickup>(spawn.type);
        if (!pickup->initialize()) {
            std::cerr << "Failed to initialize pickup type " << pickupTypeToString(spawn.type) << std::endl;
            continue;
        }
        glm::vec3 position = m_tileGrid->gridToWorld(spawn.gridPosition);
        position.z += 0.1f;
        pickup->setPosition(position);
        pickup->setActive(true);
        m_pickups.push_back(std::move(pickup));
    }
}

void Scene::handlePickupCollection() {
    if (!m_player || m_pickups.empty()) {
        return;
    }

    if (m_gameLogic && m_gameLogic->isPlayerInVehicle()) {
        return;
    }

    const glm::vec3 playerPos = m_player->getPosition();
    const glm::vec2 playerSize = m_player->getColliderSize();
    const float playerRadius = std::max(playerSize.x, playerSize.y) * 0.35f;

    auto it = std::remove_if(m_pickups.begin(), m_pickups.end(), [&](const std::unique_ptr<Pickup>& pickup) {
        if (!pickup || !pickup->isActive()) {
            return true;
        }
        const glm::vec3 pickupPos = pickup->getPosition();
        const glm::vec2 delta(pickupPos.x - playerPos.x, pickupPos.y - playerPos.y);
        const float radius = playerRadius + pickup->getRadius();
        const float distanceSq = delta.x * delta.x + delta.y * delta.y;
        if (distanceSq <= radius * radius) {
            if (pickup->getType() == PickupType::Pistol) {
                m_player->equipWeapon(std::make_unique<PistolWeapon>());
                std::cout << "Picked up pistol" << std::endl;
            }
            return true;
        }
        return false;
    });

    if (it != m_pickups.end()) {
        m_pickups.erase(it, m_pickups.end());
    }
}

void Scene::firePistolShot() {
    if (!m_player) {
        return;
    }

    const glm::vec3 playerPos = m_player->getPosition();
    glm::vec2 origin(playerPos.x, playerPos.y);
    glm::vec2 dir = Heading::forwardFromHeadingDeg(m_player->getRotation().z);
    if ((dir.x * dir.x + dir.y * dir.y) < 0.0001f) {
        return;
    }
    dir = glm::normalize(dir);

    constexpr float kMaxRange = 25.0f;
    constexpr float kProjectileSpeed = 35.0f;

    if (m_pedestrianManager) {
        m_pedestrianManager->notifyGunshot(playerPos);
    }
    m_projectileManager.spawnProjectile(glm::vec3(origin.x, origin.y, playerPos.z + 0.15f), dir,
                                        kProjectileSpeed, kMaxRange);
}

std::vector<const Collider*> Scene::getAllColliders() const {
    std::vector<const Collider*> allColliders;
    
    // Add player if active and not in vehicle
    if (m_player && m_player->isActive()) {
        // Only add player as collider if not in a vehicle
        if (!m_gameLogic || !m_gameLogic->isPlayerInVehicle()) {
            allColliders.push_back(m_player.get());
        }
    }
    
    // Add player vehicles
    for (const auto& vehicle : m_vehicles) {
        if (vehicle && vehicle->isActive()) {
            allColliders.push_back(vehicle.get());
        }
    }
    
    // Add traffic vehicles
    if (m_trafficManager) {
        for (const auto& vehicle : m_trafficManager->getTrafficVehicles()) {
            if (vehicle && vehicle->isActive()) {
                allColliders.push_back(vehicle.get());
            }
        }
    }
    
    // Add police vehicles
    if (m_policeChaseManager) {
        for (const auto& vehicle : m_policeChaseManager->getPoliceVehicles()) {
            if (vehicle && vehicle->isActive()) {
                allColliders.push_back(vehicle.get());
            }
        }
    }
    
    return allColliders;
}

void Scene::setupCollisionCallbacks() {
    // Set up collision callback for player
    if (m_player) {
        m_player->setCollisionCallback([this]() { return getAllColliders(); });
    }
    
    // Set up collision callbacks for all player vehicles
    for (auto& vehicle : m_vehicles) {
        if (vehicle) {
            vehicle->setCollisionCallback([this]() { return getAllColliders(); });
        }
    }
}

void Scene::handleVehicleExploded(Vehicle* vehicle) {
    if (!vehicle) {
        return;
    }

    // Apply explosion damage to nearby vehicles
    const glm::vec3 explosionPos = vehicle->getPosition();
    const float tileSize = m_tileGrid ? m_tileGrid->getTileSize() : 3.0f;
    const float explosionRadius = tileSize * 3.0f;  // 4 tile radius
    const float maxDamage = 8.0f;

    auto applyExplosionDamage = [&](Vehicle* target) {
        if (!target || target == vehicle || target->isWrecked() || target->isExploding()) {
            return;
        }
        const glm::vec3 targetPos = target->getPosition();
        const float dx = targetPos.x - explosionPos.x;
        const float dy = targetPos.y - explosionPos.y;
        const float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < explosionRadius) {
            // Linear falloff: full damage at center, zero at edge
            const float factor = 1.0f - (dist / explosionRadius);
            const int damage = static_cast<int>(std::ceil(maxDamage * factor));
            if (damage > 0) {
                target->applyHit(damage);
            }
        }
    };

    for (auto& v : m_vehicles) {
        applyExplosionDamage(v.get());
    }
    if (m_trafficManager) {
        for (auto& v : m_trafficManager->getTrafficVehicles()) {
            applyExplosionDamage(v.get());
        }
    }

    const bool playerInside = m_gameLogic && m_gameLogic->getActiveVehicle() == vehicle;
    if (playerInside) {
        restartLevel();
    }
}

void Scene::restartLevel() {
    rebuildVehiclesFromSpawns();
    std::cout << "Restarted level after vehicle explosion" << std::endl;
}
