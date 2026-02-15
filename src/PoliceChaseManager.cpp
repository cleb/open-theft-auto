#include "PoliceChaseManager.hpp"
#include "PolicePilot.hpp"
#include "TextureManager.hpp"
#include "Renderer.hpp"
#include "Heading.hpp"
#include "VehicleConfig.hpp"
#include "Player.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>

PoliceChaseManager::PoliceChaseManager()
    : m_tileGrid(nullptr)
    , m_camera(nullptr)
    , m_player(nullptr)
    , m_trafficManager(nullptr)
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

void PoliceChaseManager::initialize(TileGrid* tileGrid, Camera* camera, Player* player, TrafficManager* trafficManager,
                                    std::vector<std::unique_ptr<Vehicle>>* vehicles) {
    m_tileGrid = tileGrid;
    m_camera = camera;
    m_player = player;
    m_trafficManager = trafficManager;
    m_vehicles = vehicles;

    m_policeOfficerAnimation = std::make_unique<SpriteAnimation>();
    if (!m_policeOfficerAnimation->loadFromFile("assets/textures/policeman-animation.json")) {
        std::cerr << "Failed to load policeman animation" << std::endl;
        m_policeOfficerAnimation.reset();
    }
    
    std::cout << "PoliceChaseManager initialized" << std::endl;
}

void PoliceChaseManager::reset() {
    if (m_vehicles) {
        m_vehicles->erase(
            std::remove_if(m_vehicles->begin(), m_vehicles->end(),
                [](const std::unique_ptr<Vehicle>& v) {
                    return v && v->getOwner() == VehicleOwner::Police;
                }),
            m_vehicles->end());
    }
    clearOfficer(false);
    m_killTimestamps.clear();
    m_currentTime = 0.0f;
    m_chaseActive = false;
    
    std::cout << "PoliceChaseManager reset" << std::endl;
}

void PoliceChaseManager::setProjectionInfo(float fovRadians, float aspectRatio) {
    m_fovRadians = fovRadians;
    m_aspectRatio = aspectRatio;
}

std::vector<Pedestrian*> PoliceChaseManager::getShootableOfficers() const {
    std::vector<Pedestrian*> officers;
    if (m_onFootOfficer && m_onFootOfficer->isActive() && !m_onFootOfficer->isDead()) {
        officers.push_back(m_onFootOfficer.get());
    }
    return officers;
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
    
    // Check if any police vehicles already exist
    if (m_vehicles) {
        for (const auto& v : *m_vehicles) {
            if (v && v->getOwner() == VehicleOwner::Police) return;
        }
    }

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
    
    // Check if any police vehicles already exist
    if (m_vehicles) {
        for (const auto& v : *m_vehicles) {
            if (v && v->getOwner() == VehicleOwner::Police) return;
        }
    }
    
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
    updateOfficer(deltaTime);
    
    // Note: We only spawn ONE police vehicle per chase
    // If the police vehicle is destroyed, the chase continues but no respawn
}

void PoliceChaseManager::render(Renderer* renderer) {
    if (!renderer) return;

    // Vehicles are rendered by Scene from the shared list.
    // Only render the on-foot officer here.
    if (m_onFootOfficer && m_onFootOfficer->isActive()) {
        m_onFootOfficer->render(renderer);
    }
}

void PoliceChaseManager::updatePoliceVehicles(float deltaTime) {
    if (!m_vehicles) return;

    // Update all police vehicles
    for (auto& vehicle : *m_vehicles) {
        if (vehicle && vehicle->isActive() && vehicle->getOwner() == VehicleOwner::Police) {
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
    
    // Check against all vehicles in the shared list
    if (m_vehicles) {
        for (const auto& vehicle : *m_vehicles) {
            if (!vehicle) continue;
            glm::vec3 diff = vehicle->getPosition() - position;
            float distSq = diff.x * diff.x + diff.y * diff.y;
            if (distSq < minDistSq) return true;
        }
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
    if (!m_tileGrid || !m_vehicles) return;
    
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
    
    assignPolicePilot(vehicle.get());

    vehicle->setOwner(VehicleOwner::Police);
    if (m_addVehicleCallback) {
        m_addVehicleCallback(std::move(vehicle));
    } else {
        m_vehicles->push_back(std::move(vehicle));
    }
    
    std::cout << "Police vehicle spawned at (" << spawnPos.x << ", " << spawnPos.y << ")" << std::endl;
}

void PoliceChaseManager::assignPolicePilot(Vehicle* vehicle) {
    if (!vehicle) {
        return;
    }

    auto pilot = std::make_unique<PolicePilot>();
    if (m_playerPositionCallback) {
        pilot->setPlayerPositionCallback(m_playerPositionCallback);
    }

    const auto* policeDef = VehicleConfig::getInstance().getDefinition("police");
    if (policeDef) {
        pilot->setMaxSpeed(policeDef->maxSpeed);
    } else {
        pilot->setMaxSpeed(30.0f);
    }

    vehicle->setPilot(std::move(pilot));
}

void PoliceChaseManager::updateOfficer(float deltaTime) {
    // Check if any police vehicles exist in the shared list
    bool hasPoliceVehicles = false;
    if (m_vehicles) {
        for (const auto& v : *m_vehicles) {
            if (v && v->getOwner() == VehicleOwner::Police) {
                hasPoliceVehicles = true;
                break;
            }
        }
    }

    if (!m_chaseActive || !hasPoliceVehicles || !m_playerPositionCallback) {
        if (m_onFootOfficer && m_onFootOfficer->isActive()) {
            m_onFootOfficer->update(deltaTime);
        }
        return;
    }

    Vehicle* activePoliceVehicle = nullptr;
    if (m_vehicles) {
        for (auto& vehicle : *m_vehicles) {
            if (vehicle && vehicle->isActive() && vehicle->getOwner() == VehicleOwner::Police
                && !vehicle->isExploding() && !vehicle->isWrecked()) {
                activePoliceVehicle = vehicle.get();
                break;
            }
        }
    }

    if (!activePoliceVehicle) {
        return;
    }

    const glm::vec3 playerPos = m_playerPositionCallback();
    maybeDeployOfficer(activePoliceVehicle, playerPos);

    if (!m_onFootOfficer || !m_onFootOfficer->isActive()) {
        return;
    }

    m_officerShootCooldown = std::max(0.0f, m_officerShootCooldown - deltaTime);

    checkOfficerVehicleCollision();
    m_onFootOfficer->update(deltaTime);

    if (m_onFootOfficer->isDead()) {
        // One police car gets one officer. If the officer is killed, no replacement driver appears.
        return;
    }

    const float dx = playerPos.x - m_onFootOfficer->getPosition().x;
    const float dy = playerPos.y - m_onFootOfficer->getPosition().y;
    const float distToPlayer = std::sqrt(dx * dx + dy * dy);
    if (distToPlayer >= m_officerReturnDistance) {
        tryOfficerReenterVehicle(deltaTime, playerPos);
    } else {
        updateOfficerCombat(deltaTime, playerPos);
    }
}

void PoliceChaseManager::maybeDeployOfficer(Vehicle* vehicle, const glm::vec3& playerPos) {
    if (!vehicle || m_onFootOfficer || !m_policeOfficerAnimation || m_policeOfficerAnimation->getTexture() == nullptr) {
        return;
    }
    if (!vehicle->hasPilot()) {
        return;
    }

    const glm::vec3 vehiclePos = vehicle->getPosition();
    const float dx = playerPos.x - vehiclePos.x;
    const float dy = playerPos.y - vehiclePos.y;
    const float distToPlayer = std::sqrt(dx * dx + dy * dy);
    if (distToPlayer > m_officerDeployDistance) {
        return;
    }

    auto officer = std::make_unique<Pedestrian>();
    officer->initialize(m_policeOfficerAnimation.get());
    officer->setTileGrid(m_tileGrid);
    officer->setSpeed(0.0f);
    officer->setActive(true);

    const float vehicleHeading = vehicle->getRotation().z;
    const glm::vec2 forward = Heading::forwardFromHeadingDeg(vehicleHeading);
    const glm::vec2 left(-forward.y, forward.x);
    const glm::vec2 vehicleSize = vehicle->getSpriteSize();
    const float exitOffset = (vehicleSize.x * 0.5f) + 0.8f;
    const glm::vec3 spawnPos = vehiclePos + glm::vec3(left.x * exitOffset, left.y * exitOffset, 0.0f);

    officer->setPosition(glm::vec3(spawnPos.x, spawnPos.y, vehiclePos.z));
    officer->setRotation(glm::vec3(0.0f, 0.0f, Heading::headingDegFromForward(left)));

    vehicle->clearPilot();
    vehicle->setSpeed(0.0f);

    m_onFootOfficer = std::move(officer);
    m_officerVehicle = vehicle;
    m_officerShootCooldown = 0.2f;
}

void PoliceChaseManager::updateOfficerCombat(float deltaTime, const glm::vec3& playerPos) {
    if (!m_onFootOfficer || m_onFootOfficer->isDead()) {
        return;
    }

    glm::vec3 officerPos = m_onFootOfficer->getPosition();
    glm::vec2 toPlayer(playerPos.x - officerPos.x, playerPos.y - officerPos.y);
    const float distToPlayer = glm::length(toPlayer);
    if (distToPlayer < 0.001f) {
        return;
    }

    const glm::vec2 dirToPlayer = glm::normalize(toPlayer);
    m_onFootOfficer->setRotation(glm::vec3(0.0f, 0.0f, Heading::headingDegFromForward(dirToPlayer)));

    // Close distance to keep pressure, but avoid standing on top of the player.
    if (distToPlayer > 5.0f) {
        glm::vec3 nextPos = officerPos + glm::vec3(dirToPlayer.x, dirToPlayer.y, 0.0f) * (m_officerSpeed * deltaTime);
        nextPos.z = officerPos.z;
        if (m_tileGrid && m_tileGrid->canOccupy(officerPos, nextPos)) {
            m_onFootOfficer->setPosition(nextPos);
        }
    }

    if (distToPlayer <= m_officerFireDistance && m_officerShootCooldown <= 0.0f && m_officerShootCallback) {
        m_officerShootCallback(glm::vec3(officerPos.x, officerPos.y, officerPos.z + 0.15f), dirToPlayer);
        m_officerShootCooldown = 0.55f;
    }
}

void PoliceChaseManager::tryOfficerReenterVehicle(float deltaTime, const glm::vec3& /*playerPos*/) {
    if (!m_onFootOfficer || m_onFootOfficer->isDead() || !m_officerVehicle || !m_officerVehicle->isActive() ||
        m_officerVehicle->isWrecked() || m_officerVehicle->isExploding()) {
        return;
    }

    const glm::vec3 officerPos = m_onFootOfficer->getPosition();
    const glm::vec3 vehiclePos = m_officerVehicle->getPosition();
    glm::vec2 toVehicle(vehiclePos.x - officerPos.x, vehiclePos.y - officerPos.y);
    const float distToVehicle = glm::length(toVehicle);

    if (distToVehicle > 0.05f) {
        const glm::vec2 dirToVehicle = glm::normalize(toVehicle);
        m_onFootOfficer->setRotation(glm::vec3(0.0f, 0.0f, Heading::headingDegFromForward(dirToVehicle)));

        glm::vec3 nextPos = officerPos + glm::vec3(dirToVehicle.x, dirToVehicle.y, 0.0f) * (m_officerSpeed * deltaTime);
        nextPos.z = officerPos.z;
        if (m_tileGrid && m_tileGrid->canOccupy(officerPos, nextPos)) {
            m_onFootOfficer->setPosition(nextPos);
        }
    }

    if (distToVehicle <= 1.1f) {
        if (!m_officerVehicle->hasPilot()) {
            assignPolicePilot(m_officerVehicle);
        }
        clearOfficer(false);
    }
}

void PoliceChaseManager::checkOfficerVehicleCollision() {
    if (!m_onFootOfficer || !m_onFootOfficer->isActive() || m_onFootOfficer->isDead() || !m_officerColliderCallback) {
        return;
    }

    const glm::vec3 pedPos = m_onFootOfficer->getPosition();
    const glm::vec2 pedSize = m_onFootOfficer->getSize();
    const float pedRadius = std::max(pedSize.x, pedSize.y) * 0.3f;

    const auto colliders = m_officerColliderCallback();
    for (const Collider* collider : colliders) {
        const auto* vehicle = dynamic_cast<const Vehicle*>(collider);
        if (!vehicle || !vehicle->isActive() || vehicle->isWrecked() || vehicle->isExploding()) {
            continue;
        }

        const glm::vec3 vehPos = vehicle->getPosition();
        const glm::vec2 vehSize = vehicle->getSpriteSize();
        const float vehRotation = vehicle->getRotation().z;

        const glm::vec2 forward = Heading::forwardFromHeadingDeg(vehRotation);
        const glm::vec2 right(forward.y, -forward.x);
        const glm::vec2 toP(pedPos.x - vehPos.x, pedPos.y - vehPos.y);

        const float localX = glm::dot(toP, right);
        const float localY = glm::dot(toP, forward);
        const float halfWidth = vehSize.x * 0.5f;
        const float halfLength = vehSize.y * 0.5f;

        if (std::abs(localX) < halfWidth + pedRadius && std::abs(localY) < halfLength + pedRadius) {
            m_onFootOfficer->kill();
            break;
        }
    }
}

void PoliceChaseManager::clearOfficer(bool keepCorpse) {
    if (!m_onFootOfficer) {
        m_officerVehicle = nullptr;
        m_officerShootCooldown = 0.0f;
        return;
    }

    if (!keepCorpse) {
        m_onFootOfficer->setActive(false);
    }

    m_onFootOfficer.reset();
    m_officerVehicle = nullptr;
    m_officerShootCooldown = 0.0f;
}
