#include "PoliceChaseManager.hpp"
#include "PolicePilot.hpp"
#include "TextureManager.hpp"
#include "Renderer.hpp"
#include "Heading.hpp"
#include "VehicleConfig.hpp"
#include "Player.hpp"
#include <iostream>
#include <algorithm>

PoliceChaseManager::PoliceChaseManager()
    : m_tileGrid(nullptr)
    , m_camera(nullptr)
    , m_player(nullptr)
    , m_trafficManager(nullptr)
    , m_collisionCallback(nullptr)
    , m_currentTime(0.0f)
    , m_killThreshold(3)
    , m_killWindowSeconds(60.0f)
    , m_viewMargin(10.0f)
    , m_enabled(true)
    , m_chaseActive(false)
    , m_fovRadians(1.57f)
    , m_aspectRatio(16.0f / 9.0f)
    , m_rng(std::random_device{}()) {
}

void PoliceChaseManager::initialize(TileGrid* tileGrid, Camera* camera, Player* player, TrafficManager* trafficManager) {
    m_tileGrid = tileGrid;
    m_camera = camera;
    m_player = player;
    m_trafficManager = trafficManager;
    
    std::cout << "PoliceChaseManager initialized" << std::endl;
}

void PoliceChaseManager::reset() {
    m_policeVehicles.clear();
    m_killTimestamps.clear();
    m_currentTime = 0.0f;
    m_chaseActive = false;
    
    std::cout << "PoliceChaseManager reset" << std::endl;
}

void PoliceChaseManager::setProjectionInfo(float fovRadians, float aspectRatio) {
    m_fovRadians = fovRadians;
    m_aspectRatio = aspectRatio;
}

void PoliceChaseManager::setCollisionCallback(ColliderCallback callback) {
    m_collisionCallback = callback;
    
    // Apply to all existing police vehicles
    for (auto& vehicle : m_policeVehicles) {
        if (vehicle) {
            vehicle->setCollisionCallback(m_collisionCallback);
        }
    }
}

void PoliceChaseManager::onPedestrianKilled() {
    if (!m_enabled) return;
    
    // Record the kill timestamp
    m_killTimestamps.push_back(m_currentTime);
    
    std::cout << "Pedestrian killed! Recent kills: " << getRecentKillCount() 
              << "/" << m_killThreshold << " (in last " << m_killWindowSeconds << "s)" << std::endl;
    
    // Check if we should trigger a chase
    checkChaseCondition();
}

void PoliceChaseManager::onPlayerCausedVehicleExplosion() {
    if (!m_enabled) return;
    if (m_chaseActive) return;
    if (!m_policeVehicles.empty()) return;

    m_chaseActive = true;
    std::cout << "WANTED! Police chase initiated after player-caused vehicle explosion!" << std::endl;
    spawnPoliceVehicle();
}

int PoliceChaseManager::getRecentKillCount() const {
    int count = 0;
    float cutoffTime = m_currentTime - m_killWindowSeconds;
    
    for (float timestamp : m_killTimestamps) {
        if (timestamp >= cutoffTime) {
            count++;
        }
    }
    
    return count;
}

void PoliceChaseManager::cleanupOldKills() {
    float cutoffTime = m_currentTime - m_killWindowSeconds;
    
    while (!m_killTimestamps.empty() && m_killTimestamps.front() < cutoffTime) {
        m_killTimestamps.pop_front();
    }
}

void PoliceChaseManager::checkChaseCondition() {
    if (m_chaseActive) return;  // Already in a chase
    if (!m_policeVehicles.empty()) return;  // Already have a police vehicle
    
    int recentKills = getRecentKillCount();
    
    if (recentKills >= m_killThreshold) {
        m_chaseActive = true;
        std::cout << "WANTED! Police chase initiated after " << recentKills << " pedestrian kills!" << std::endl;
        spawnPoliceVehicle();
    }
}

void PoliceChaseManager::update(float deltaTime) {
    if (!m_enabled) return;
    
    m_currentTime += deltaTime;
    
    // Clean up old kill timestamps
    cleanupOldKills();
    
    // Update police vehicles
    updatePoliceVehicles(deltaTime);
    
    // Note: We only spawn ONE police vehicle per chase
    // If the police vehicle is destroyed, the chase continues but no respawn
}

void PoliceChaseManager::render(Renderer* renderer) {
    if (!renderer) return;
    
    for (auto& vehicle : m_policeVehicles) {
        if (vehicle && vehicle->isActive()) {
            vehicle->render(renderer);
        }
    }
}

void PoliceChaseManager::updatePoliceVehicles(float deltaTime) {
    // Update all police vehicles
    for (auto& vehicle : m_policeVehicles) {
        if (vehicle && vehicle->isActive()) {
            vehicle->update(deltaTime);
        }
    }
    
    // Optionally despawn police vehicles that are too far from player
    // (for now, keep them active to maintain the chase)
}

glm::vec3 PoliceChaseManager::findValidSpawnPoint() {
    if (!m_camera || !m_tileGrid) {
        return glm::vec3(0.0f);
    }
    
    ViewBounds bounds = ViewBounds::calculate(m_camera, m_fovRadians, m_aspectRatio);
    
    // Use TrafficManager's road spawn points
    if (m_trafficManager) {
        const auto& roadSpawnPoints = m_trafficManager->getRoadSpawnPoints();
        
        // Filter to spawn points in the spawn zone (outside view, but not too far)
        std::vector<const RoadSpawnPoint*> validPoints;
        for (const auto& point : roadSpawnPoints) {
            if (bounds.isInSpawnZone(point.worldPos, m_viewMargin, 3.0f) && 
                !isTooCloseToOtherVehicles(point.worldPos)) {
                validPoints.push_back(&point);
            }
        }
        
        if (!validPoints.empty()) {
            std::uniform_int_distribution<size_t> dist(0, validPoints.size() - 1);
            const RoadSpawnPoint* selected = validPoints[dist(m_rng)];
            m_lastSpawnRotation = selected->rotationDegrees;
            return selected->worldPos;
        }
    }
    
    // Fallback: spawn at edge of grid if no valid road spawn points
    const float tileSize = m_tileGrid->getTileSize();
    const glm::ivec3& gridSize = m_tileGrid->getGridSize();
    
    m_lastSpawnRotation = 0.0f;
    glm::vec3 fallbackPos(
        (gridSize.x / 2) * tileSize,
        0.5f * tileSize,
        0.1f
    );
    return fallbackPos;
}

bool PoliceChaseManager::isTooCloseToOtherVehicles(const glm::vec3& position) const {
    const float minDistance = 8.0f;
    const float minDistSq = minDistance * minDistance;
    
    // Check against police vehicles
    for (const auto& vehicle : m_policeVehicles) {
        if (!vehicle) continue;
        glm::vec3 diff = vehicle->getPosition() - position;
        float distSq = diff.x * diff.x + diff.y * diff.y;
        if (distSq < minDistSq) return true;
    }
    
    // Check against player if available
    if (m_player && m_player->isActive()) {
        glm::vec3 diff = m_player->getPosition() - position;
        float distSq = diff.x * diff.x + diff.y * diff.y;
        if (distSq < minDistSq) return true;
    }
    
    return false;
}

void PoliceChaseManager::spawnPoliceVehicle() {
    if (!m_tileGrid) return;
    
    glm::vec3 spawnPos = findValidSpawnPoint();
    
    // Get police vehicle config
    const auto& config = VehicleConfig::getInstance();
    const auto* policeDef = config.getDefinition("police");
    
    // Create the police vehicle
    auto vehicle = std::make_unique<Vehicle>();
    vehicle->setVehicleType("police");
    
    // Get police texture
    std::string texturePath = policeDef ? policeDef->texturePath : "assets/textures/police.png";
    auto vehicleTexture = TextureManager::instance().getTextureFromPath(texturePath);
    if (vehicleTexture) {
        vehicle->initialize(vehicleTexture);
    } else {
        vehicle->initialize(texturePath);
    }
    
    // Load delta texture for damage
    std::string deltaTexturePath = policeDef ? policeDef->deltaTexturePath : "assets/textures/police-deltas.png";
    auto deltaTexture = TextureManager::instance().getTextureFromPath(deltaTexturePath);
    if (deltaTexture) {
        vehicle->setDeltaTexture(deltaTexture);
    }
    
    vehicle->setPosition(spawnPos);
    
    // Use the road-aligned rotation from the spawn point (set by findValidSpawnPoint)
    vehicle->setRotation(glm::vec3(0.0f, 0.0f, m_lastSpawnRotation));
    
    // Set sprite size from config
    glm::vec2 spriteSize = policeDef ? policeDef->size : glm::vec2(1.6f, 3.2f);
    vehicle->setSpriteSize(spriteSize);
    vehicle->setTileGrid(m_tileGrid);
    
    // Set collision callback
    if (m_collisionCallback) {
        vehicle->setCollisionCallback(m_collisionCallback);
    }
    
    // Create and assign a PolicePilot
    auto pilot = std::make_unique<PolicePilot>();
    
    // Set up the player position callback for the pilot
    if (m_playerPositionCallback) {
        pilot->setPlayerPositionCallback(m_playerPositionCallback);
    }
    
    // Set the police car's speed (faster than any other vehicle)
    if (policeDef) {
        pilot->setMaxSpeed(policeDef->maxSpeed);
    } else {
        pilot->setMaxSpeed(30.0f);  // Default: very fast
    }
    
    vehicle->setPilot(std::move(pilot));
    
    m_policeVehicles.push_back(std::move(vehicle));
    
    std::cout << "Police vehicle spawned at (" << spawnPos.x << ", " << spawnPos.y << ")" << std::endl;
}
