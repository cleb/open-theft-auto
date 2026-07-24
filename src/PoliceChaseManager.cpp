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
#include <limits>
#include <ostream>

PoliceChaseManager::PoliceChaseManager()
    : m_tileGrid(nullptr)
    , m_camera(nullptr)
    , m_player(nullptr)
    , m_trafficManager(nullptr)
    , m_characterPhysics(nullptr)
    , m_viewMargin(10.0f)
    , m_enabled(true)
    , m_chaseActive(false)
    , m_fovRadians(1.57f)
    , m_aspectRatio(16.0f / 9.0f)
    , m_rng(std::random_device{}()) {
}

void PoliceChaseManager::initialize(TileGrid* tileGrid, Camera* camera, Player* player, TrafficManager* trafficManager,
                                    std::vector<std::unique_ptr<Vehicle>>* vehicles,
                                    CharacterPhysics* characterPhysics) {
    m_tileGrid = tileGrid;
    m_camera = camera;
    m_player = player;
    m_trafficManager = trafficManager;
    m_vehicles = vehicles;
    m_characterPhysics = characterPhysics;

    m_policeOfficerAnimation = std::make_unique<SpriteAnimation>();
    if (!m_policeOfficerAnimation->loadFromFile("assets/textures/policeman-animation.json")) {
        std::cerr << "Failed to load policeman animation" << std::endl;
        m_policeOfficerAnimation.reset();
    }
    
    std::cout << "PoliceChaseManager initialized" << std::endl;
}

void PoliceChaseManager::clearWantedState() {
    m_killTimestamps.clear();
    m_chaseActive = false;
    m_wantedLevel = 0;
    m_civilianKillOffensesAtWantedLevel = 0;
    m_directOffensesAtWantedLevel = 0;
}

void PoliceChaseManager::endChase() {
    clearWantedState();

    if (m_vehicles) {
        for (auto& vehicle : *m_vehicles) {
            if (!vehicle || vehicle->getOwner() != VehicleOwner::Police ||
                !vehicle->isActive() || vehicle->isWrecked() || vehicle->isExploding()) {
                continue;
            }
            auto* policePilot = dynamic_cast<PolicePilot*>(vehicle->getPilot());
            if (policePilot && policePilot->isChasing()) {
                vehicle->setSpeed(0.0f);
                policePilot->beginTrafficPatrol();
            }
        }
    }

    for (auto& officerUnit : m_onFootOfficers) {
        if (officerUnit.inVehicle || !officerUnit.officer) {
            continue;
        }

        Pedestrian* officer = officerUnit.officer->getPedestrian();
        if (!officer || !officer->isDead()) {
            continue;
        }

        Vehicle* vehicle = officerUnit.vehicle;
        if (vehicle && vehicle->getOwner() == VehicleOwner::Police &&
            vehicle->isActive() && !vehicle->isWrecked() && !vehicle->isExploding() &&
            !vehicle->hasPilot()) {
            assignPatrolPilot(vehicle);
        }
        officerUnit.vehicle = nullptr;
    }

    std::cout << "Police chase ended" << std::endl;
}

void PoliceChaseManager::resetForWorldRestart() {
    for (auto& officerUnit : m_onFootOfficers) {
        clearOfficer(officerUnit);
    }
    m_onFootOfficers.clear();

    if (m_vehicles) {
        m_vehicles->erase(
            std::remove_if(m_vehicles->begin(), m_vehicles->end(),
                [](const std::unique_ptr<Vehicle>& vehicle) {
                    return vehicle && vehicle->getOwner() == VehicleOwner::Police;
                }),
            m_vehicles->end());
    }

    clearWantedState();
    m_currentTime = 0.0f;
    std::cout << "PoliceChaseManager reset for world restart" << std::endl;
}

void PoliceChaseManager::setProjectionInfo(float fovRadians, float aspectRatio) {
    m_fovRadians = fovRadians;
    m_aspectRatio = aspectRatio;
}

std::vector<Pedestrian*> PoliceChaseManager::getShootableOfficers() const {
    std::vector<Pedestrian*> officers;
    for (const auto& officerUnit : m_onFootOfficers) {
        if (!officerUnit.inVehicle && officerUnit.officer && officerUnit.officer->isAlive()) {
            officers.push_back(officerUnit.officer->getPedestrian());
        }
    }
    return officers;
}

void PoliceChaseManager::onPlayerFiredWeapon() {
    if (!m_enabled) return;

    if (isAnyPoliceVehicleOnScreen()) {
        recordDirectWantedOffense("player fired weapon with police on screen");
    }
}

void PoliceChaseManager::onPedestrianRunDown() {
    if (!m_enabled) return;

    if (m_chaseActive) {
        recordCivilianKillOffense("player ran down pedestrians");
        return;
    }

    // Immediate trigger if police can see it
    if (isAnyPoliceVehicleOnScreen()) {
        increaseWantedLevel("player ran down pedestrian with police on screen");
        return;
    }

    // Record the kill timestamp
    m_killTimestamps.push_back(m_currentTime);

    // Fallback: check kill threshold (3 kills triggers chase even without police on screen)
    checkChaseCondition();
}

void PoliceChaseManager::onPedestrianKilledByGunfire() {
    if (!m_enabled) return;

    if (m_chaseActive) {
        recordCivilianKillOffense("player killed pedestrians with gunfire");
        return;
    }

    // Immediate trigger if police can see it
    if (isAnyPoliceVehicleOnScreen()) {
        increaseWantedLevel("player killed pedestrian with police on screen");
        return;
    }

    // Record the kill timestamp
    m_killTimestamps.push_back(m_currentTime);

    // Fallback: check kill threshold (3 kills triggers chase even without police on screen)
    checkChaseCondition();
}

void PoliceChaseManager::onPlayerCausedVehicleExplosion() {
    if (!m_enabled) return;

    recordDirectWantedOffense("player-caused vehicle explosion");
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
    m_wantedLevel = std::max(m_wantedLevel, 1);
    
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
    ensureWantedPoliceUnits();
}

void PoliceChaseManager::increaseWantedLevel(const std::string& reason) {
    ++m_wantedLevel;
    m_chaseActive = true;
    m_civilianKillOffensesAtWantedLevel = 0;
    m_directOffensesAtWantedLevel = 0;

    std::cout << "WANTED LEVEL " << m_wantedLevel << "! Police response increased after "
              << reason << "." << std::endl;
    triggerChase();
}

void PoliceChaseManager::recordCivilianKillOffense(const std::string& reason) {
    if (!m_chaseActive) {
        increaseWantedLevel(reason);
        return;
    }

    ++m_civilianKillOffensesAtWantedLevel;
    if (m_civilianKillOffensesAtWantedLevel >= getCivilianKillThresholdForCurrentLevel()) {
        increaseWantedLevel(reason);
    }
}

void PoliceChaseManager::recordDirectWantedOffense(const std::string& reason) {
    if (!m_chaseActive) {
        increaseWantedLevel(reason);
        return;
    }

    ++m_directOffensesAtWantedLevel;
    if (m_directOffensesAtWantedLevel >= getDirectOffenseThresholdForCurrentLevel()) {
        increaseWantedLevel(reason);
    }
}

int PoliceChaseManager::getCivilianKillThresholdForCurrentLevel() const {
    const int shift = std::max(0, m_wantedLevel);
    if (shift >= 30 || m_killThreshold > std::numeric_limits<int>::max() / (1 << shift)) {
        return std::numeric_limits<int>::max();
    }
    return m_killThreshold * (1 << shift);
}

int PoliceChaseManager::getDirectOffenseThresholdForCurrentLevel() const {
    const int shift = std::max(0, m_wantedLevel - 1);
    if (shift >= 30) {
        return std::numeric_limits<int>::max();
    }
    return 1 << shift;
}

void PoliceChaseManager::activatePoliceVehicles() {
    if (!m_vehicles) return;
    
    for (auto& v : *m_vehicles) {
        if (!v || v->getOwner() != VehicleOwner::Police) continue;
        if (!v->isActive() || v->isWrecked() || v->isExploding()) continue;
        if (isVehicleAssignedToOfficer(v.get())) continue;
        
        // A chase-mode PolicePilot is already active. Patrol-mode police
        // vehicles need a fresh chase pilot.
        auto* policePilot = dynamic_cast<PolicePilot*>(v->getPilot());
        if (policePilot && policePilot->isChasing()) continue;
        
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
    
    // Update police vehicles
    updatePoliceVehicles(deltaTime);
    updateOfficer(deltaTime);
}

void PoliceChaseManager::render(Renderer* renderer) {
    if (!renderer) return;

    // Vehicles are rendered by Scene from the shared list.
    // Only render the on-foot officers here.
    for (const auto& officerUnit : m_onFootOfficers) {
        if (officerUnit.officer) {
            officerUnit.officer->render(renderer);
        }
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

void PoliceChaseManager::dumpDebugState(std::ostream& out) const {
    int activeOfficerIndex = 0;
    for (const auto& officerUnit : m_onFootOfficers) {
        if (!officerUnit.officer || !officerUnit.officer->isAlive()) {
            continue;
        }

        const glm::vec3 officerPos = officerUnit.officer->getPosition();
        out << "officer." << activeOfficerIndex << ".position=["
            << officerPos.x << "," << officerPos.y << "," << officerPos.z << "]\n";
        if (officerUnit.vehicle) {
            const glm::vec3 vehiclePos = officerUnit.vehicle->getPosition();
            out << "officer." << activeOfficerIndex << ".vehicle_position=["
                << vehiclePos.x << "," << vehiclePos.y << "," << vehiclePos.z << "]\n";
        }
        ++activeOfficerIndex;
    }
    out << "officers.active=" << activeOfficerIndex << "\n";
}

bool PoliceChaseManager::debugSpawnOfficerAt(const glm::vec3& position, float headingDeg) {
    if (!m_policeOfficerAnimation || m_policeOfficerAnimation->getTexture() == nullptr ||
        !m_characterPhysics) {
        return false;
    }

    auto officer = std::make_unique<CombatPedestrian>();
    officer->spawn(m_policeOfficerAnimation.get(), m_tileGrid, *m_characterPhysics, position, headingDeg);
    officer->setShootCallback(m_officerShootCallback);
    officer->setSpeed(m_officerSpeed);
    officer->setFireDistance(m_officerFireDistance);
    officer->setChaseDistance(5.0f);
    officer->setShootCooldown(0.55f);
    officer->setMovementTargetAdjustCallback([this](const glm::vec3& from, const glm::vec3& target,
                                                    float officerRadius) {
        return adjustOfficerMovementTargetAroundVehicles(from, target, officerRadius);
    });
    officer->setLineOfSightBlockCallback([this](const glm::vec3& from, const glm::vec3& target,
                                                float clearanceRadius) {
        return isOfficerLineOfSightBlockedByVehicle(from, target, clearanceRadius);
    });

    OfficerUnit unit;
    unit.officer = std::move(officer);
    unit.vehicle = nullptr;
    m_onFootOfficers.push_back(std::move(unit));
    m_chaseActive = true;
    m_wantedLevel = std::max(m_wantedLevel, 1);
    std::cout << "Spawned debug officer at (" << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;
    return true;
}

void PoliceChaseManager::setWantedLevel(int wantedLevel) {
    wantedLevel = std::max(0, wantedLevel);
    if (wantedLevel == 0) {
        resetForWorldRestart();
        return;
    }

    m_wantedLevel = wantedLevel;
    m_chaseActive = true;
    m_civilianKillOffensesAtWantedLevel = 0;
    m_directOffensesAtWantedLevel = 0;
    activatePoliceVehicles();
    ensureWantedPoliceUnits();
    std::cout << "Debug wanted level set to " << m_wantedLevel << std::endl;
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
        increaseWantedLevel(std::to_string(recentKills) + " pedestrian kills");
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

void PoliceChaseManager::assignPatrolPilot(Vehicle* vehicle) {
    if (!vehicle) {
        return;
    }
    auto pilot = std::make_unique<PolicePilot>();
    PolicePilot* patrolPilot = pilot.get();
    vehicle->setPilot(std::move(pilot));
    vehicle->setSpeed(0.0f);
    patrolPilot->beginTrafficPatrol();
}

void PoliceChaseManager::updateOfficer(float deltaTime) {
    for (auto& officerUnit : m_onFootOfficers) {
        if (!officerUnit.inVehicle || !officerUnit.officer) {
            continue;
        }

        Vehicle* vehicle = officerUnit.vehicle;
        if (vehicle && vehicle->getOwner() == VehicleOwner::Police && vehicle->isActive() &&
            !vehicle->isWrecked() && !vehicle->isExploding()) {
            continue;
        }

        glm::vec3 exitPosition = officerUnit.officer->getPosition();
        float exitHeading = 0.0f;
        const bool vehicleDestroyed =
            !vehicle || !vehicle->isActive() || vehicle->isWrecked() || vehicle->isExploding();
        if (vehicle) {
            exitPosition = getOfficerVehicleEntryPoint(officerUnit, exitPosition);
            exitHeading = vehicle->getRotation().z;
        }

        officerUnit.officer->exitVehicle(exitPosition, exitHeading);
        officerUnit.inVehicle = false;
        officerUnit.vehicle = nullptr;
        if (vehicleDestroyed) {
            Pedestrian* ped = officerUnit.officer->getPedestrian();
            if (ped) {
                ped->kill();
            }
        }
    }

    if (!m_chaseActive || !m_playerPositionCallback) {
        for (auto& officerUnit : m_onFootOfficers) {
            if (!officerUnit.officer || officerUnit.inVehicle) {
                continue;
            }

            Pedestrian* ped = officerUnit.officer->getPedestrian();
            if (!ped || !ped->isActive()) {
                continue;
            }

            if (!m_chaseActive && officerUnit.officer->isAlive() && officerUnit.vehicle) {
                if (officerUnit.vehicle->getOwner() != VehicleOwner::Police ||
                    !officerUnit.vehicle->isActive() || officerUnit.vehicle->isWrecked() ||
                    officerUnit.vehicle->isExploding() || officerUnit.vehicle->hasPilot()) {
                    officerUnit.vehicle = nullptr;
                } else {
                    tryOfficerReenterVehicle(officerUnit, deltaTime, glm::vec3(0.0f));
                    continue;
                }
            }

            ped->update(deltaTime);
        }
        return;
    }

    ensureWantedPoliceUnits();
    const glm::vec3 playerPos = m_playerPositionCallback();

    if (m_vehicles) {
        for (auto& vehicle : *m_vehicles) {
            if (vehicle && vehicle->isActive() && vehicle->getOwner() == VehicleOwner::Police
                && !vehicle->isExploding() && !vehicle->isWrecked() && !isVehicleAssignedToOfficer(vehicle.get())) {
                maybeDeployOfficer(vehicle.get(), playerPos);
            }
        }
    }

    for (auto& officerUnit : m_onFootOfficers) {
        updateOfficerUnit(officerUnit, deltaTime, playerPos);
    }
    ensureWantedPoliceUnits();
}

void PoliceChaseManager::ensureWantedPoliceUnits() {
    if (!m_chaseActive || !m_vehicles) {
        return;
    }

    while (getActivePoliceUnitCount() < m_wantedLevel) {
        spawnPoliceVehicle();
    }
}

int PoliceChaseManager::getActivePoliceUnitCount() const {
    int count = 0;
    if (m_vehicles) {
        for (const auto& vehicle : *m_vehicles) {
            if (vehicle && vehicle->isActive() && vehicle->getOwner() == VehicleOwner::Police &&
                !vehicle->isExploding() && !vehicle->isWrecked() && vehicle->hasPilot()) {
                ++count;
            }
        }
    }

    for (const auto& officerUnit : m_onFootOfficers) {
        if (!officerUnit.inVehicle && officerUnit.officer && officerUnit.officer->isAlive()) {
            ++count;
        }
    }
    return count;
}

bool PoliceChaseManager::isVehicleAssignedToOfficer(const Vehicle* vehicle) const {
    if (!vehicle) {
        return false;
    }

    for (const auto& officerUnit : m_onFootOfficers) {
        if (!officerUnit.inVehicle && officerUnit.vehicle == vehicle) {
            return true;
        }
    }
    return false;
}

PoliceChaseManager::OfficerUnit* PoliceChaseManager::findOfficerUnitForVehicle(Vehicle* vehicle) {
    if (!vehicle) {
        return nullptr;
    }

    for (auto& officerUnit : m_onFootOfficers) {
        if (officerUnit.vehicle == vehicle) {
            return &officerUnit;
        }
    }
    return nullptr;
}

void PoliceChaseManager::handleOfficerKilled(OfficerUnit& unit) {
    if (unit.deathHandled) {
        return;
    }

    unit.deathHandled = true;
    if (unit.vehicle && unit.vehicle->isActive()) {
        unit.vehicle->clearPilot();
        unit.vehicle->setSpeed(0.0f);
    }
    recordDirectWantedOffense("police officer killed");
}

void PoliceChaseManager::updateOfficerUnit(OfficerUnit& unit, float deltaTime, const glm::vec3& playerPos) {
    if (!unit.officer || unit.inVehicle) {
        return;
    }

    if (unit.vehicle &&
        (unit.vehicle->getOwner() != VehicleOwner::Police || !unit.vehicle->isActive() ||
         unit.vehicle->isWrecked() || unit.vehicle->isExploding())) {
        unit.vehicle = nullptr;
    }

    if (!unit.officer->isAlive()) {
        handleOfficerKilled(unit);
        Pedestrian* ped = unit.officer->getPedestrian();
        if (ped && ped->isActive()) ped->update(deltaTime);
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
            unit.officer->checkVehicleCollision(vehicle->getPosition(), vehicle->getSpriteSize(),
                                                vehicle->getRotation().z, vehicle->getSpeed());
        }
    }

    if (!unit.officer->isAlive()) {
        handleOfficerKilled(unit);
        return;
    }

    const float dx = playerPos.x - unit.officer->getPosition().x;
    const float dy = playerPos.y - unit.officer->getPosition().y;
    const float distToPlayer = std::sqrt(dx * dx + dy * dy);
    if (distToPlayer >= m_officerReturnDistance && unit.vehicle) {
        tryOfficerReenterVehicle(unit, deltaTime, playerPos);
        return;
    }

    // CombatPedestrian::update handles Pedestrian::update + pathfinding chase + shoot
    unit.officer->update(deltaTime, playerPos);

    if (unit.officer->isDead()) {
        handleOfficerKilled(unit);
    }
}

void PoliceChaseManager::maybeDeployOfficer(Vehicle* vehicle, const glm::vec3& playerPos) {
    if (!vehicle || !m_characterPhysics ||
        !m_policeOfficerAnimation || m_policeOfficerAnimation->getTexture() == nullptr) {
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

    OfficerUnit* existingUnit = findOfficerUnitForVehicle(vehicle);
    if (existingUnit) {
        if (!existingUnit->inVehicle || !existingUnit->officer) {
            return;
        }
        existingUnit->officer->exitVehicle(
            glm::vec3(spawnPos.x, spawnPos.y, vehiclePos.z), headingDeg);
        existingUnit->inVehicle = false;
        existingUnit->deathHandled = false;
    } else {
        auto officer = std::make_unique<CombatPedestrian>();
        officer->spawn(m_policeOfficerAnimation.get(), m_tileGrid, *m_characterPhysics,
                       glm::vec3(spawnPos.x, spawnPos.y, vehiclePos.z), headingDeg);
        officer->setShootCallback(m_officerShootCallback);
        officer->setSpeed(m_officerSpeed);
        officer->setFireDistance(m_officerFireDistance);
        officer->setChaseDistance(5.0f);
        officer->setShootCooldown(0.55f);
        officer->setMovementTargetAdjustCallback([this](const glm::vec3& from, const glm::vec3& target,
                                                        float officerRadius) {
            return adjustOfficerMovementTargetAroundVehicles(from, target, officerRadius);
        });
        officer->setLineOfSightBlockCallback([this](const glm::vec3& from, const glm::vec3& target,
                                                    float clearanceRadius) {
            return isOfficerLineOfSightBlockedByVehicle(from, target, clearanceRadius);
        });

        OfficerUnit unit;
        unit.officer = std::move(officer);
        unit.vehicle = vehicle;
        m_onFootOfficers.push_back(std::move(unit));
    }

    vehicle->clearPilot();
    vehicle->setSpeed(0.0f);
}

void PoliceChaseManager::tryOfficerReenterVehicle(OfficerUnit& unit, float deltaTime, const glm::vec3& /*playerPos*/) {
    if (!unit.officer || unit.officer->isDead() || !unit.vehicle || !unit.vehicle->isActive() ||
        unit.vehicle->isWrecked() || unit.vehicle->isExploding()) {
        return;
    }

    Pedestrian* ped = unit.officer->getPedestrian();
    if (!ped) return;

    ped->update(deltaTime);

    const glm::vec3 entryPos = getOfficerVehicleEntryPoint(unit, ped->getPosition());
    unit.officer->moveToward(deltaTime, entryPos, 0.35f);

    const glm::vec3 updatedOfficerPos = ped->getPosition();
    const float dx = entryPos.x - updatedOfficerPos.x;
    const float dy = entryPos.y - updatedOfficerPos.y;
    if (std::sqrt(dx * dx + dy * dy) <= 0.45f) {
        if (!unit.vehicle->hasPilot()) {
            if (m_chaseActive) {
                assignPolicePilot(unit.vehicle);
            } else {
                assignPatrolPilot(unit.vehicle);
            }
        }
        unit.officer->enterVehicle();
        unit.inVehicle = true;
    }
}

void PoliceChaseManager::clearOfficer(OfficerUnit& unit) {
    if (!unit.officer) {
        unit.vehicle = nullptr;
        unit.inVehicle = false;
        return;
    }

    Pedestrian* ped = unit.officer->getPedestrian();
    if (ped) ped->setActive(false);

    unit.officer.reset();
    unit.vehicle = nullptr;
    unit.inVehicle = false;
    m_officerDetourVehicle = nullptr;
    m_officerDetourSide = 0;
}

glm::vec3 PoliceChaseManager::getOfficerVehicleEntryPoint(const OfficerUnit& unit, const glm::vec3& officerPos) const {
    if (!unit.vehicle) {
        return officerPos;
    }

    const glm::vec3 vehiclePos = unit.vehicle->getPosition();
    const glm::vec2 vehicleSize = unit.vehicle->getSpriteSize();
    const glm::vec2 forward = Heading::forwardFromHeadingDeg(unit.vehicle->getRotation().z);
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
