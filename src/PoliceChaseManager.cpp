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
    if (m_onFootOfficer && m_onFootOfficer->isAlive()) {
        officers.push_back(m_onFootOfficer->getPedestrian());
    }
    return officers;
}

void PoliceChaseManager::onPlayerFiredWeapon() {
    if (!m_enabled) return;
    if (m_chaseActive) return;
    
    if (isAnyPoliceVehicleOnScreen()) {
        std::cout << "WANTED! Police chase initiated - player fired weapon with police on screen!" << std::endl;
        triggerChase();
    }
}

void PoliceChaseManager::onPedestrianRunDown() {
    if (!m_enabled) return;
    
    // Record the kill timestamp
    m_killTimestamps.push_back(m_currentTime);
    
    if (m_chaseActive) return;
    
    // Immediate trigger if police can see it
    if (isAnyPoliceVehicleOnScreen()) {
        std::cout << "WANTED! Police chase initiated - player ran down pedestrian with police on screen!" << std::endl;
        triggerChase();
        return;
    }
    
    // Fallback: check kill threshold (3 kills triggers chase even without police on screen)
    checkChaseCondition();
}

void PoliceChaseManager::onPedestrianKilledByGunfire() {
    if (!m_enabled) return;
    
    // Record the kill timestamp
    m_killTimestamps.push_back(m_currentTime);
    
    if (m_chaseActive) return;
    
    // Immediate trigger if police can see it
    if (isAnyPoliceVehicleOnScreen()) {
        std::cout << "WANTED! Police chase initiated - player killed pedestrian with police on screen!" << std::endl;
        triggerChase();
        return;
    }
    
    // Fallback: check kill threshold (3 kills triggers chase even without police on screen)
    checkChaseCondition();
}

void PoliceChaseManager::onPlayerCausedVehicleExplosion() {
    if (!m_enabled) return;
    if (m_chaseActive) return;
    
    std::cout << "WANTED! Police chase initiated after player-caused vehicle explosion!" << std::endl;
    triggerChase();
}

bool PoliceChaseManager::isAnyPoliceVehicleOnScreen() const {
    if (!m_vehicles || !m_camera) return false;
    
    ViewBounds bounds = ViewBounds::calculate(m_camera, m_fovRadians, m_aspectRatio);
    
    for (const auto& v : *m_vehicles) {
        if (!v || v->getOwner() != VehicleOwner::Police) continue;
        if (!v->isActive() || v->isWrecked() || v->isExploding()) continue;
        
        if (bounds.contains(v->getPosition())) {
            return true;
        }
    }
    
    return false;
}

void PoliceChaseManager::triggerChase() {
    m_chaseActive = true;
    
    // Check if any police vehicles already exist
    bool hasPoliceVehicles = false;
    if (m_vehicles) {
        for (const auto& v : *m_vehicles) {
            if (v && v->getOwner() == VehicleOwner::Police && 
                v->isActive() && !v->isWrecked() && !v->isExploding()) {
                hasPoliceVehicles = true;
                break;
            }
        }
    }
    
    if (hasPoliceVehicles) {
        // Activate existing police vehicles - switch from AutoPilot to PolicePilot
        activatePoliceVehicles();
    } else {
        // No police vehicles exist at all, spawn one
        spawnPoliceVehicle();
    }
}

void PoliceChaseManager::activatePoliceVehicles() {
    if (!m_vehicles) return;
    
    for (auto& v : *m_vehicles) {
        if (!v || v->getOwner() != VehicleOwner::Police) continue;
        if (!v->isActive() || v->isWrecked() || v->isExploding()) continue;
        
        // Check if the vehicle already has a PolicePilot (already activated)
        if (dynamic_cast<PolicePilot*>(v->getPilot()) != nullptr) continue;
        
        // Replace AutoPilot with PolicePilot to start chasing
        assignPolicePilot(v.get());
        std::cout << "Police vehicle activated for chase at (" 
                  << v->getPosition().x << ", " << v->getPosition().y << ")" << std::endl;
    }
}

void PoliceChaseManager::update(float deltaTime) {
    if (!m_enabled) return;
    
    m_currentTime += deltaTime;
    
    // Clean up old kill timestamps
    cleanupOldKills();
    
    // Despawn police vehicles that have drifted too far from view (only when not chasing)
    if (!m_chaseActive) {
        despawnOutOfViewPoliceVehicles();
    }
    
    // Update police vehicles
    updatePoliceVehicles(deltaTime);
    updateOfficer(deltaTime);
}

void PoliceChaseManager::render(Renderer* renderer) {
    if (!renderer) return;

    // Vehicles are rendered by Scene from the shared list.
    // Only render the on-foot officer here.
    if (m_onFootOfficer) {
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
}

void PoliceChaseManager::despawnOutOfViewPoliceVehicles() {
    if (!m_vehicles || !m_camera) return;
    
    ViewBounds bounds = ViewBounds::calculate(m_camera, m_fovRadians, m_aspectRatio);
    ViewBounds despawnBounds = bounds.expanded(m_viewMargin * 2.0f);
    
    m_vehicles->erase(
        std::remove_if(m_vehicles->begin(), m_vehicles->end(),
            [&despawnBounds](const std::unique_ptr<Vehicle>& vehicle) {
                if (!vehicle) return true;
                if (vehicle->getOwner() != VehicleOwner::Police) return false;
                return !despawnBounds.contains(vehicle->getPosition());
            }),
        m_vehicles->end());
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
    if (m_chaseActive) return;
    
    int recentKills = getRecentKillCount();
    
    if (recentKills >= m_killThreshold) {
        std::cout << "WANTED! Police chase initiated after " << recentKills << " pedestrian kills!" << std::endl;
        triggerChase();
    }
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
        if (m_onFootOfficer) {
            Pedestrian* ped = m_onFootOfficer->getPedestrian();
            if (ped && ped->isActive()) ped->update(deltaTime);
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

    if (!m_onFootOfficer || !m_onFootOfficer->isAlive()) {
        // Still update for death animation
        if (m_onFootOfficer) {
            Pedestrian* ped = m_onFootOfficer->getPedestrian();
            if (ped && ped->isActive()) ped->update(deltaTime);
        }
        return;
    }

    // Check vehicle collisions via collider callback
    if (m_officerColliderCallback) {
        const auto colliders = m_officerColliderCallback();
        for (const Collider* collider : colliders) {
            const auto* vehicle = dynamic_cast<const Vehicle*>(collider);
            if (!vehicle || !vehicle->isActive() || vehicle->isWrecked() || vehicle->isExploding()) {
                continue;
            }
            m_onFootOfficer->checkVehicleCollision(vehicle->getPosition(), vehicle->getSpriteSize(),
                                                   vehicle->getRotation().z, vehicle->getSpeed());
        }
    }

    const float dx = playerPos.x - m_onFootOfficer->getPosition().x;
    const float dy = playerPos.y - m_onFootOfficer->getPosition().y;
    const float distToPlayer = std::sqrt(dx * dx + dy * dy);
    if (distToPlayer >= m_officerReturnDistance) {
        tryOfficerReenterVehicle(deltaTime, playerPos);
        return;
    }

    // CombatPedestrian::update handles Pedestrian::update + pathfinding chase + shoot
    m_onFootOfficer->update(deltaTime, playerPos);

    if (m_onFootOfficer->isDead()) {
        return;
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

    const float vehicleHeading = vehicle->getRotation().z;
    const glm::vec2 forward = Heading::forwardFromHeadingDeg(vehicleHeading);
    const glm::vec2 left(-forward.y, forward.x);
    const glm::vec2 vehicleSize = vehicle->getSpriteSize();
    const float exitOffset = (vehicleSize.x * 0.5f) + 0.8f;
    const glm::vec3 spawnPos = vehiclePos + glm::vec3(left.x * exitOffset, left.y * exitOffset, 0.0f);
    const float headingDeg = Heading::headingDegFromForward(left);

    auto officer = std::make_unique<CombatPedestrian>();
    officer->spawn(m_policeOfficerAnimation.get(), m_tileGrid,
                   glm::vec3(spawnPos.x, spawnPos.y, vehiclePos.z), headingDeg);
    officer->setShootCallback(m_officerShootCallback);
    officer->setSpeed(m_officerSpeed);
    officer->setFireDistance(m_officerFireDistance);
    officer->setChaseDistance(5.0f);
    officer->setShootCooldown(0.55f);
    officer->setVehicleBlockCheck([this](const glm::vec3& position, float officerRadius) {
        return isOfficerPositionBlockedByVehicle(position, officerRadius);
    });
    officer->setMovementTargetAdjustCallback([this](const glm::vec3& from, const glm::vec3& target,
                                                    float officerRadius) {
        return adjustOfficerMovementTargetAroundVehicles(from, target, officerRadius);
    });
    officer->setLineOfSightBlockCallback([this](const glm::vec3& from, const glm::vec3& target,
                                                float clearanceRadius) {
        return isOfficerLineOfSightBlockedByVehicle(from, target, clearanceRadius);
    });

    vehicle->clearPilot();
    vehicle->setSpeed(0.0f);

    m_onFootOfficer = std::move(officer);
    m_officerVehicle = vehicle;
}

void PoliceChaseManager::tryOfficerReenterVehicle(float deltaTime, const glm::vec3& /*playerPos*/) {
    if (!m_onFootOfficer || m_onFootOfficer->isDead() || !m_officerVehicle || !m_officerVehicle->isActive() ||
        m_officerVehicle->isWrecked() || m_officerVehicle->isExploding()) {
        return;
    }

    Pedestrian* ped = m_onFootOfficer->getPedestrian();
    if (!ped) return;

    ped->update(deltaTime);

    const glm::vec3 entryPos = getOfficerVehicleEntryPoint(ped->getPosition());
    m_onFootOfficer->moveToward(deltaTime, entryPos, 0.35f);

    const glm::vec3 updatedOfficerPos = ped->getPosition();
    const float dx = entryPos.x - updatedOfficerPos.x;
    const float dy = entryPos.y - updatedOfficerPos.y;
    if (std::sqrt(dx * dx + dy * dy) <= 0.45f) {
        if (!m_officerVehicle->hasPilot()) {
            assignPolicePilot(m_officerVehicle);
        }
        clearOfficer(false);
    }
}

void PoliceChaseManager::clearOfficer(bool keepCorpse) {
    if (!m_onFootOfficer) {
        m_officerVehicle = nullptr;
        return;
    }

    if (!keepCorpse) {
        Pedestrian* ped = m_onFootOfficer->getPedestrian();
        if (ped) ped->setActive(false);
    }

    m_onFootOfficer.reset();
    m_officerVehicle = nullptr;
    m_officerDetourVehicle = nullptr;
    m_officerDetourSide = 0;
}

glm::vec3 PoliceChaseManager::getOfficerVehicleEntryPoint(const glm::vec3& officerPos) const {
    if (!m_officerVehicle) {
        return officerPos;
    }

    const glm::vec3 vehiclePos = m_officerVehicle->getPosition();
    const glm::vec2 vehicleSize = m_officerVehicle->getSpriteSize();
    const glm::vec2 forward = Heading::forwardFromHeadingDeg(m_officerVehicle->getRotation().z);
    const glm::vec2 right(forward.y, -forward.x);
    const glm::vec2 toOfficer(officerPos.x - vehiclePos.x, officerPos.y - vehiclePos.y);

    const float halfWidth = vehicleSize.x * 0.5f;
    const float halfLength = vehicleSize.y * 0.5f;
    const float localX = glm::dot(toOfficer, right);
    const float localY = glm::dot(toOfficer, forward);
    constexpr float kEntryClearance = 0.85f;
    constexpr float kCornerInset = 0.2f;

    const bool useSideDoor = std::abs(localX) / std::max(halfWidth, 0.001f) >=
                             std::abs(localY) / std::max(halfLength, 0.001f);

    float entryLocalX = std::clamp(localX, -halfWidth + kCornerInset, halfWidth - kCornerInset);
    float entryLocalY = std::clamp(localY, -halfLength + kCornerInset, halfLength - kCornerInset);
    if (useSideDoor) {
        entryLocalX = (localX < 0.0f ? -1.0f : 1.0f) * (halfWidth + kEntryClearance);
    } else {
        entryLocalY = (localY < 0.0f ? -1.0f : 1.0f) * (halfLength + kEntryClearance);
    }

    glm::vec3 entryPos = vehiclePos +
                         glm::vec3(right.x * entryLocalX + forward.x * entryLocalY,
                                   right.y * entryLocalX + forward.y * entryLocalY,
                                   0.0f);
    entryPos.z = officerPos.z;
    return entryPos;
}

glm::vec3 PoliceChaseManager::adjustOfficerMovementTargetAroundVehicles(const glm::vec3& from,
                                                                        const glm::vec3& target,
                                                                        float officerRadius) {
    if (!m_officerColliderCallback) {
        return target;
    }

    const auto colliders = m_officerColliderCallback();
    for (const Collider* collider : colliders) {
        const auto* vehicle = dynamic_cast<const Vehicle*>(collider);
        if (!vehicle || !vehicle->isActive() || vehicle->isWrecked() || vehicle->isExploding()) {
            continue;
        }
        if (!segmentIntersectsVehicle(from, target, vehicle, officerRadius, false)) {
            continue;
        }

        const glm::vec3 vehiclePos = vehicle->getPosition();
        const glm::vec2 vehicleSize = vehicle->getSpriteSize();
        const glm::vec2 forward = Heading::forwardFromHeadingDeg(vehicle->getRotation().z);
        const glm::vec2 right(forward.y, -forward.x);
        const glm::vec2 toFrom(from.x - vehiclePos.x, from.y - vehiclePos.y);
        const glm::vec2 toTarget(target.x - vehiclePos.x, target.y - vehiclePos.y);
        const float localX = glm::dot(toFrom, right);
        const float localY = glm::dot(toFrom, forward);
        const float targetLocalY = glm::dot(toTarget, forward);
        const float halfWidth = vehicleSize.x * 0.5f;
        const float halfLength = vehicleSize.y * 0.5f;
        const float detourWidth = halfWidth + officerRadius + 1.2f;
        const float detourLength = halfLength + officerRadius + 1.2f;

        if (m_officerDetourVehicle != vehicle || m_officerDetourSide == 0) {
            std::uniform_int_distribution<int> sideDist(0, 1);
            m_officerDetourVehicle = vehicle;
            m_officerDetourSide = sideDist(m_rng) == 0 ? -1 : 1;
        }

        glm::vec2 localDetour(
            static_cast<float>(m_officerDetourSide) * detourWidth,
            std::clamp(localY, -detourLength, detourLength)
        );

        if (std::abs(localX) >= detourWidth * 0.75f) {
            localDetour.y = std::clamp(targetLocalY, -detourLength, detourLength);
        }

        glm::vec3 detourTarget = vehiclePos +
                                 glm::vec3(right.x * localDetour.x + forward.x * localDetour.y,
                                           right.y * localDetour.x + forward.y * localDetour.y,
                                           0.0f);
        detourTarget.z = from.z;
        if (isOfficerPositionBlockedByVehicle(detourTarget, officerRadius)) {
            m_officerDetourSide *= -1;
            localDetour.x = static_cast<float>(m_officerDetourSide) * detourWidth;
            detourTarget = vehiclePos +
                           glm::vec3(right.x * localDetour.x + forward.x * localDetour.y,
                                     right.y * localDetour.x + forward.y * localDetour.y,
                                     0.0f);
            detourTarget.z = from.z;
        }

        return detourTarget;
    }

    m_officerDetourVehicle = nullptr;
    m_officerDetourSide = 0;
    return target;
}

bool PoliceChaseManager::isOfficerPositionBlockedByVehicle(const glm::vec3& position, float officerRadius) const {
    if (!m_officerColliderCallback) {
        return false;
    }

    const auto colliders = m_officerColliderCallback();
    for (const Collider* collider : colliders) {
        const auto* vehicle = dynamic_cast<const Vehicle*>(collider);
        if (!vehicle || !vehicle->isActive() || vehicle->isWrecked() || vehicle->isExploding()) {
            continue;
        }

        const glm::vec3 vehiclePos = vehicle->getPosition();
        const glm::vec2 vehicleSize = vehicle->getSpriteSize();
        const glm::vec2 forward = Heading::forwardFromHeadingDeg(vehicle->getRotation().z);
        const glm::vec2 right(forward.y, -forward.x);
        const glm::vec2 toP(position.x - vehiclePos.x, position.y - vehiclePos.y);
        const float localX = glm::dot(toP, right);
        const float localY = glm::dot(toP, forward);
        const float halfWidth = vehicleSize.x * 0.5f;
        const float halfLength = vehicleSize.y * 0.5f;

        if (std::abs(localX) < halfWidth + officerRadius &&
            std::abs(localY) < halfLength + officerRadius) {
            return true;
        }
    }

    return false;
}

bool PoliceChaseManager::isOfficerLineOfSightBlockedByVehicle(const glm::vec3& from, const glm::vec3& target,
                                                              float clearanceRadius) const {
    if (!m_officerColliderCallback) {
        return false;
    }

    const auto colliders = m_officerColliderCallback();
    for (const Collider* collider : colliders) {
        const auto* vehicle = dynamic_cast<const Vehicle*>(collider);
        if (!vehicle || !vehicle->isActive() || vehicle->isWrecked() || vehicle->isExploding()) {
            continue;
        }
        if (segmentIntersectsVehicle(from, target, vehicle, clearanceRadius, true)) {
            return true;
        }
    }

    return false;
}

bool PoliceChaseManager::segmentIntersectsVehicle(const glm::vec3& from, const glm::vec3& target,
                                                  const Vehicle* vehicle, float clearanceRadius,
                                                  bool ignoreIfTargetInside) const {
    if (!vehicle) {
        return false;
    }

    const glm::vec3 vehiclePos = vehicle->getPosition();
    const glm::vec2 vehicleSize = vehicle->getSpriteSize();
    const glm::vec2 forward = Heading::forwardFromHeadingDeg(vehicle->getRotation().z);
    const glm::vec2 right(forward.y, -forward.x);
    const float halfWidth = vehicleSize.x * 0.5f + clearanceRadius;
    const float halfLength = vehicleSize.y * 0.5f + clearanceRadius;

    auto toLocal = [&](const glm::vec3& worldPos) {
        const glm::vec2 toP(worldPos.x - vehiclePos.x, worldPos.y - vehiclePos.y);
        return glm::vec2(glm::dot(toP, right), glm::dot(toP, forward));
    };

    const glm::vec2 start = toLocal(from);
    const glm::vec2 end = toLocal(target);
    const bool targetInside = std::abs(end.x) <= halfWidth && std::abs(end.y) <= halfLength;
    if (ignoreIfTargetInside && targetInside) {
        return false;
    }

    const glm::vec2 delta = end - start;
    float tMin = 0.0f;
    float tMax = 1.0f;

    auto clipAxis = [&](float startCoord, float deltaCoord, float minCoord, float maxCoord) {
        constexpr float kEpsilon = 0.0001f;
        if (std::abs(deltaCoord) < kEpsilon) {
            return startCoord >= minCoord && startCoord <= maxCoord;
        }

        float t1 = (minCoord - startCoord) / deltaCoord;
        float t2 = (maxCoord - startCoord) / deltaCoord;
        if (t1 > t2) {
            std::swap(t1, t2);
        }
        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        return tMin <= tMax;
    };

    return clipAxis(start.x, delta.x, -halfWidth, halfWidth) &&
           clipAxis(start.y, delta.y, -halfLength, halfLength) &&
           tMax >= 0.0f && tMin <= 1.0f;
}
