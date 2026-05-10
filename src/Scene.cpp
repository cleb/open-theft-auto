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
#include "MachineGunWeapon.hpp"
#include "PickupTypes.hpp"
#include "TextureManager.hpp"
#include "MissionSystem.hpp"
#include <iostream>
#include <ostream>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/geometric.hpp>
#include <GLFW/glfw3.h>
#include <imgui.h>

namespace {
const char* missionStateName(MissionState state) {
    switch (state) {
        case MissionState::Idle: return "Idle";
        case MissionState::Prompted: return "Prompted";
        case MissionState::Active: return "Active";
        case MissionState::Completed: return "Completed";
        case MissionState::Failed: return "Failed";
    }
    return "Unknown";
}

const char* vehicleOwnerName(VehicleOwner owner) {
    switch (owner) {
        case VehicleOwner::World: return "World";
        case VehicleOwner::Traffic: return "Traffic";
        case VehicleOwner::Police: return "Police";
    }
    return "Unknown";
}

void dumpVec3(std::ostream& out, const glm::vec3& value) {
    out << "[" << value.x << "," << value.y << "," << value.z << "]";
}

void dumpVec2(std::ostream& out, const glm::vec2& value) {
    out << "[" << value.x << "," << value.y << "]";
}

std::string normalizeDebugId(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        if (c == '-') {
            return '_';
        }
        return static_cast<char>(std::tolower(c));
    });
    return value;
}
}

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

    m_characterPhysics.configure(
        [this](const glm::vec3& from, const glm::vec3& to) {
            return m_tileGrid && m_tileGrid->canOccupy(from, to);
        },
        [this](const glm::vec3& position, const glm::vec2& footprintSize,
               const glm::vec2& movement, float referenceZ) {
            if (!m_tileGrid) {
                return referenceZ;
            }
            return m_tileGrid->getSurfaceHeightForFootprint(
                position, footprintSize, movement, referenceZ);
        },
        [this]() { return getCharacterPhysicsObstacles(); });

    m_tileGridEditor = std::make_unique<TileGridEditor>();
    m_tileGridEditor->initialize(m_tileGrid.get(), &m_levelData);
    m_tileGridEditor->setWindow(window);
    m_tileGridEditor->setRenderer(renderer);
    m_tileGridEditor->setLevelChangedCallback([this]() { onLevelChanged(); });
    
    // Initialize player
    m_player = std::make_unique<Player>(m_characterPhysics);
    if (!m_player->initialize()) {
        std::cerr << "Failed to initialize player" << std::endl;
        return false;
    }
    // Initialize traffic manager
    m_trafficManager = std::make_unique<TrafficManager>();
    m_trafficManager->initialize(m_tileGrid.get(), renderer->getCamera(), &m_vehicles);
    m_trafficManager->setProjectionInfo(renderer->getFovRadians(), window->getAspectRatio());
    m_trafficManager->setAddVehicleCallback([this](std::unique_ptr<Vehicle> v) {
        addVehicle(std::move(v));
    });
    
    // Initialize pedestrian manager
    m_pedestrianManager = std::make_unique<PedestrianManager>();
    m_pedestrianManager->initialize(m_tileGrid.get(), renderer->getCamera(), &m_characterPhysics);
    m_pedestrianManager->setProjectionInfo(renderer->getFovRadians(), window->getAspectRatio());
    
    // Connect traffic manager to pedestrian manager for carjack callbacks
    m_trafficManager->setPedestrianManager(m_pedestrianManager.get());
    
    // Set vehicle callback for pedestrian collision detection
    m_pedestrianManager->setVehicleCallback([this]() -> std::vector<Vehicle*> {
        std::vector<Vehicle*> allVehicles;
        for (const auto& vehicle : m_vehicles) {
            if (vehicle && vehicle->isActive()) {
                allVehicles.push_back(vehicle.get());
            }
        }
        return allVehicles;
    });
    
    // Initialize police chase manager
    m_policeChaseManager = std::make_unique<PoliceChaseManager>();
    m_policeChaseManager->initialize(m_tileGrid.get(), renderer->getCamera(), m_player.get(),
                                     m_trafficManager.get(), &m_vehicles, &m_characterPhysics);
    m_policeChaseManager->setProjectionInfo(renderer->getFovRadians(), window->getAspectRatio());
    m_policeChaseManager->setAddVehicleCallback([this](std::unique_ptr<Vehicle> v) {
        addVehicle(std::move(v));
    });
    m_policeChaseManager->setOfficerColliderCallback([this]() { return getAllColliders(); });
    
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
    m_policeChaseManager->setOfficerShootCallback([this](const glm::vec3& origin, const glm::vec2& direction) {
        constexpr float kOfficerProjectileSpeed = 30.0f;
        constexpr float kOfficerProjectileRange = 22.0f;
        m_projectileManager.spawnProjectile(origin, direction, kOfficerProjectileSpeed, kOfficerProjectileRange,
                                            ProjectileManager::ProjectileOwner::Police);
    });
    
    // Set up pedestrian kill callback to notify police chase manager
    m_pedestrianManager->setPedestrianKillCallback([this](Vehicle* killerVehicle) {
        // Only count kills by the player's vehicle
        if (m_gameLogic && m_gameLogic->isPlayerInVehicle()) {
            Vehicle* playerVehicle = m_gameLogic->getActiveVehicle();
            if (playerVehicle == killerVehicle) {
                if (m_policeChaseManager) {
                    m_policeChaseManager->onPedestrianRunDown();
                }
            }
        }
    });
    
    // Initialize game logic
    m_gameLogic->setPlayer(m_player.get());
    m_gameLogic->setVehicles(&m_vehicles);
    
    // Create test scene
    createTestScene();

    m_projectileManager.initialize();
    m_projectileManager.setPedestrianHitCallback([this]() {
        if (m_policeChaseManager) {
            m_policeChaseManager->onPedestrianKilledByGunfire();
        }
    });
    m_projectileManager.setExtraPedestrianTargetsCallback([this]() -> std::vector<Pedestrian*> {
        if (!m_policeChaseManager) {
            return {};
        }
        return m_policeChaseManager->getShootableOfficers();
    });
    m_projectileManager.setEnemyHitCallback([this](const glm::vec2& projPos, float projRadius) -> bool {
        if (m_gameLogic && m_gameLogic->isPlayerInVehicle()) {
            Vehicle* activeVehicle = m_gameLogic->getActiveVehicle();
            if (!activeVehicle || !activeVehicle->isActive() || activeVehicle->isExploding() || activeVehicle->isWrecked()) {
                return false;
            }

            const glm::vec3 vehPos3 = activeVehicle->getPosition();
            const glm::vec2 vehPos(vehPos3.x, vehPos3.y);
            const glm::vec2 diff = vehPos - projPos;
            const glm::vec2 vehSize = activeVehicle->getSpriteSize();
            const float vehRadius = std::max(vehSize.x, vehSize.y) * 0.5f;
            const float radius = projRadius + vehRadius;
            if ((diff.x * diff.x + diff.y * diff.y) <= radius * radius) {
                if (!m_debugInvincible) {
                    activeVehicle->applyHit(1);
                }
                return true;
            }
            return false;
        }

        if (!m_player || !m_player->isActive()) {
            return false;
        }

        const glm::vec3 playerPos3 = m_player->getPosition();
        const glm::vec2 playerPos(playerPos3.x, playerPos3.y);
        const glm::vec2 diff = playerPos - projPos;
        const glm::vec2 playerSize = m_player->getColliderSize();
        const float playerRadius = std::max(playerSize.x, playerSize.y) * 0.35f;
        const float radius = projRadius + playerRadius;
        if ((diff.x * diff.x + diff.y * diff.y) <= radius * radius) {
            if (!m_debugInvincible) {
                restartLevel();
            }
            return true;
        }

        return false;
    });

    // Set up enemy shoot callback for missions (enemies use Police owner → hits player)
    m_missionSystem.setEnemyShootCallback([this](const glm::vec3& origin, const glm::vec2& dir) {
        constexpr float kEnemyProjectileSpeed = 22.0f;
        constexpr float kEnemyProjectileRange = 20.0f;
        m_projectileManager.spawnProjectile(origin, dir, kEnemyProjectileSpeed, kEnemyProjectileRange,
                                            ProjectileManager::ProjectileOwner::Police);
    });
    m_missionSystem.setCharacterPhysics(&m_characterPhysics);

    // Set up player-projectile → mission-enemy hit callback
    m_projectileManager.setMissionEnemyHitCallback([this](const glm::vec2& projPos, float projRadius) -> bool {
        return m_missionSystem.checkProjectileHitEnemy(projPos, projRadius);
    });
    
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

    if (m_gameLogic) {
        if (Vehicle* activeVehicle = m_gameLogic->getActiveVehicle()) {
            activeVehicle->setInvincible(m_debugInvincible);
        }
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
        handlePhoneBoothInteraction(deltaTime);

        // Check vehicle-vs-mission-enemy collisions
        if (m_missionSystem.getState() == MissionState::Active) {
            for (const auto& vehicle : m_vehicles) {
                if (!vehicle || !vehicle->isActive() || vehicle->isWrecked() || vehicle->isExploding()) continue;
                const glm::vec3 vPos = vehicle->getPosition();
                const glm::vec2 vSize = vehicle->getSpriteSize();
                const float vRotation = vehicle->getRotation().z;
                m_missionSystem.checkVehicleHitEnemies(vPos, vSize, vRotation, vehicle->getSpeed());
            }
        }

    m_projectileManager.update(deltaTime, m_pedestrianManager.get(), &m_vehicles, m_tileGrid.get());
        for (auto& pickup : m_pickups) {
            if (pickup) {
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
    
    // Update vehicles not managed by TrafficManager or PoliceChaseManager
    for (auto& vehicle : m_vehicles) {
        if (vehicle && vehicle->isActive() && vehicle->getOwner() == VehicleOwner::World) {
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

        // Render phone booths
        for (const auto& booth : m_phoneBooths) {
            const std::string* jobId = booth.jobs.currentJobId();
            const bool active = jobId && m_missionSystem.isBoothActive(*jobId, *this);
            const auto& tex = active ? booth.texActive : booth.texInactive;
            if (tex) {
                renderer->renderSprite(*tex, glm::vec2(booth.worldPos.x, booth.worldPos.y), glm::vec2(1.5f, 1.5f));
            }
        }

        m_projectileManager.render(renderer);
    }
    
    // Render all vehicles (world, traffic, and police are all in m_vehicles)
    for (auto& vehicle : m_vehicles) {
        if (vehicle && vehicle->isActive()) {
            vehicle->render(renderer);
        }
    }

    if (!isEditModeActive()) {
        // Render mission-specific objects (escort char, enemies, markers) — after vehicles
        // so that vehicles appear on top of enemies (depth test: first writer wins)
        m_missionSystem.render(renderer);
    }
    
    // Render traffic debug spawn points if enabled
    if (m_trafficManager) {
        m_trafficManager->renderDebugSpawnPoints(renderer);
    }
    
    // Render police officer(s)
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

    drawMissionGui();

    if (isEditModeActive()) {
        return;
    }

    drawPoliceChaseGui();

    if (!m_player) {
        return;
    }

    const auto equippedType = m_player->getEquippedWeaponType();

    ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoBackground;
    if (ImGui::Begin("WeaponHUD", nullptr, flags)) {
        if (equippedType) {
            const auto texture = TextureManager::instance().getTextureFromPath(pickupTexturePath(*equippedType));
            if (texture) {
                constexpr float iconSize = 32.0f;
                constexpr float textSpacing = 6.0f;
                const ImVec2 startPos = ImGui::GetCursorPos();
                ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(texture->getID())),
                             ImVec2(iconSize, iconSize),
                             ImVec2(0.0f, 1.0f),
                             ImVec2(1.0f, 0.0f));
                ImGui::SameLine(0.0f, textSpacing);
                const float textOffset = (iconSize - ImGui::GetFontSize()) * 0.5f;
                ImGui::SetCursorPosY(startPos.y + textOffset);
                ImGui::Text("%d", m_player->getWeaponAmmo());
            }
        }
        ImGui::Text("$%d", m_player->getMoney());
    }
    ImGui::End();
}

void Scene::drawPoliceChaseGui() {
    if (!m_policeChaseManager || !m_policeChaseManager->isChaseActive()) {
        return;
    }

    const auto texture = TextureManager::instance().getTextureFromPath("assets/textures/police-chase-indicator.png");
    if (!texture) {
        return;
    }

    constexpr float defaultIconSize = 64.0f;
    constexpr float iconSpacing = 8.0f;
    constexpr float topMargin = 12.0f;
    constexpr float horizontalMargin = 12.0f;
    const int iconCount = std::max(1, m_policeChaseManager->getWantedPoliceUnitCount());
    const ImGuiIO& io = ImGui::GetIO();
    const float maxWidth = std::max(defaultIconSize, io.DisplaySize.x - horizontalMargin * 2.0f);
    const float maxIconSize = (maxWidth - iconSpacing * static_cast<float>(iconCount - 1)) /
                              static_cast<float>(iconCount);
    const float iconSize = std::clamp(maxIconSize, 24.0f, defaultIconSize);
    const float totalWidth = iconSize * static_cast<float>(iconCount) +
                             iconSpacing * static_cast<float>(iconCount - 1);
    const ImVec2 windowPos((io.DisplaySize.x - totalWidth) * 0.5f, topMargin);

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(totalWidth, iconSize), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(iconSpacing, 0.0f));
    if (ImGui::Begin("PoliceChaseHUD", nullptr, flags)) {
        for (int i = 0; i < iconCount; ++i) {
            if (i > 0) {
                ImGui::SameLine();
            }
            ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(texture->getID())),
                         ImVec2(iconSize, iconSize),
                         ImVec2(0.0f, 1.0f),
                         ImVec2(1.0f, 0.0f));
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void Scene::dumpDebugState(std::ostream& out, double gameTimeSeconds) const {
    int activeVehicles = 0;
    int trafficVehicles = 0;
    int policeVehicles = 0;
    int wreckedVehicles = 0;
    for (const auto& vehicle : m_vehicles) {
        if (!vehicle) {
            continue;
        }
        if (vehicle->isActive()) {
            ++activeVehicles;
        }
        if (vehicle->getOwner() == VehicleOwner::Traffic) {
            ++trafficVehicles;
        } else if (vehicle->getOwner() == VehicleOwner::Police) {
            ++policeVehicles;
        }
        if (vehicle->isWrecked()) {
            ++wreckedVehicles;
        }
    }

    int activePickups = 0;
    for (const auto& pickup : m_pickups) {
        if (pickup && pickup->isActive()) {
            ++activePickups;
        }
    }

    int activePedestrians = 0;
    if (m_pedestrianManager) {
        for (const auto& pedestrian : m_pedestrianManager->getPedestrians()) {
            if (pedestrian && pedestrian->isActive()) {
                ++activePedestrians;
            }
        }
    }

    out << "BEGIN_GAME_STATE\n";
    out << "time_seconds=" << gameTimeSeconds << "\n";
    out << "level_path=" << m_levelPath << "\n";
    out << "edit_mode=" << (isEditModeActive() ? "true" : "false") << "\n";
    out << "debug.invincible=" << (m_debugInvincible ? "true" : "false") << "\n";

    if (m_player) {
        out << "player.active=" << (m_player->isActive() ? "true" : "false") << "\n";
        out << "player.position=";
        dumpVec3(out, m_player->getPosition());
        out << "\nplayer.rotation=";
        dumpVec3(out, m_player->getRotation());
        out << "\nplayer.speed=" << m_player->getSpeed() << "\n";
        out << "player.money=" << m_player->getMoney() << "\n";
        out << "player.weapon=" << m_player->getWeaponDisplayName() << "\n";
        out << "player.ammo=" << m_player->getWeaponAmmo() << "\n";
    } else {
        out << "player=null\n";
    }

    out << "player.in_vehicle=" << (m_gameLogic && m_gameLogic->isPlayerInVehicle() ? "true" : "false") << "\n";
    Vehicle* activeVehicle = m_gameLogic ? m_gameLogic->getActiveVehicle() : nullptr;
    if (activeVehicle) {
        out << "active_vehicle.position=";
        dumpVec3(out, activeVehicle->getPosition());
        out << "\nactive_vehicle.rotation=";
        dumpVec3(out, activeVehicle->getRotation());
        out << "\nactive_vehicle.size=";
        dumpVec2(out, activeVehicle->getSpriteSize());
        out << "\nactive_vehicle.speed=" << activeVehicle->getSpeed() << "\n";
        out << "active_vehicle.health=" << activeVehicle->getHealth() << "/" << activeVehicle->getMaxHealth() << "\n";
        out << "active_vehicle.owner=" << vehicleOwnerName(activeVehicle->getOwner()) << "\n";
        out << "active_vehicle.type=" << activeVehicle->getVehicleTypeId() << "\n";
        out << "active_vehicle.burning=" << (activeVehicle->isBurning() ? "true" : "false") << "\n";
        out << "active_vehicle.exploding=" << (activeVehicle->isExploding() ? "true" : "false") << "\n";
        out << "active_vehicle.wrecked=" << (activeVehicle->isWrecked() ? "true" : "false") << "\n";
    }

    out << "vehicles.total=" << m_vehicles.size() << "\n";
    out << "vehicles.active=" << activeVehicles << "\n";
    out << "vehicles.traffic=" << trafficVehicles << "\n";
    out << "vehicles.police=" << policeVehicles << "\n";
    out << "vehicles.wrecked=" << wreckedVehicles << "\n";
    for (std::size_t i = 0; i < m_vehicles.size(); ++i) {
        const auto& vehicle = m_vehicles[i];
        if (!vehicle || !vehicle->isActive()) {
            continue;
        }
        out << "vehicle." << i << ".type=" << vehicle->getVehicleTypeId() << "\n";
        out << "vehicle." << i << ".owner=" << vehicleOwnerName(vehicle->getOwner()) << "\n";
        out << "vehicle." << i << ".position=";
        dumpVec3(out, vehicle->getPosition());
        out << "\nvehicle." << i << ".rotation=";
        dumpVec3(out, vehicle->getRotation());
        out << "\n";
    }
    out << "pickups.total=" << m_pickups.size() << "\n";
    out << "pickups.active=" << activePickups << "\n";
    out << "pedestrians.active=" << activePedestrians << "\n";
    if (m_policeChaseManager) {
        m_policeChaseManager->dumpDebugState(out);
    }
    out << "police.chase_active=" << (m_policeChaseManager && m_policeChaseManager->isChaseActive() ? "true" : "false") << "\n";
    out << "police.wanted_units=" << (m_policeChaseManager ? m_policeChaseManager->getWantedPoliceUnitCount() : 0) << "\n";
    out << "police.recent_kills=" << (m_policeChaseManager ? m_policeChaseManager->getRecentKillCount() : 0) << "\n";
    out << "mission.state=" << missionStateName(m_missionSystem.getState()) << "\n";
    out << "mission.active_job=" << (m_missionSystem.getActiveJob() ? m_missionSystem.getActiveJob()->id : "") << "\n";
    out << "mission.active_booth=" << m_missionSystem.getActiveBoothId() << "\n";
    out << "END_GAME_STATE" << std::endl;
}

void Scene::setDebugInvincible(bool enabled) {
    m_debugInvincible = enabled;
    for (auto& vehicle : m_vehicles) {
        if (vehicle) {
            vehicle->setInvincible(false);
        }
    }
    if (enabled && m_gameLogic) {
        if (Vehicle* activeVehicle = m_gameLogic->getActiveVehicle()) {
            activeVehicle->setInvincible(true);
        }
    }
    std::cout << "Debug invincibility: " << (enabled ? "ON" : "OFF") << std::endl;
}

bool Scene::setDebugWantedLevel(int wantedLevel) {
    if (!m_policeChaseManager || wantedLevel < 0) {
        return false;
    }
    m_policeChaseManager->setWantedLevel(wantedLevel);
    return true;
}

bool Scene::grantDebugWeapon(const std::string& weaponId, int ammo) {
    if (!m_player || ammo < 0) {
        return false;
    }

    PickupType type;
    if (!pickupTypeFromString(normalizeDebugId(weaponId), type)) {
        return false;
    }

    const int grantAmmo = ammo > 0 ? ammo : 999;
    if (!m_player->addAmmo(type, grantAmmo)) {
        switch (type) {
            case PickupType::Pistol:
                m_player->equipWeapon(type, std::make_unique<PistolWeapon>(grantAmmo));
                break;
            case PickupType::MachineGun:
                m_player->equipWeapon(type, std::make_unique<MachineGunWeapon>(grantAmmo));
                break;
        }
    }
    m_player->equipWeaponType(type);
    std::cout << "Granted weapon " << pickupTypeToString(type) << " ammo=" << grantAmmo << std::endl;
    return true;
}

bool Scene::spawnDebugVehicle(const std::string& vehicleTypeId) {
    if (!m_player || !m_tileGrid) {
        return false;
    }

    const std::string normalizedType = normalizeDebugId(vehicleTypeId);
    const auto& config = VehicleConfig::getInstance();
    const auto* typeDef = config.getDefinition(normalizedType);
    if (!typeDef) {
        return false;
    }

    glm::vec3 basePos = m_player->getPosition();
    float heading = m_player->getRotation().z;
    if (m_gameLogic && m_gameLogic->isPlayerInVehicle()) {
        if (Vehicle* activeVehicle = m_gameLogic->getActiveVehicle()) {
            basePos = activeVehicle->getPosition();
            heading = activeVehicle->getRotation().z;
        }
    }

    const glm::vec2 forward = Heading::forwardFromHeadingDeg(heading);
    glm::vec3 spawnPos = basePos + glm::vec3(forward.x, forward.y, 0.0f) * (typeDef->size.y + 2.0f);
    spawnPos.z = m_tileGrid->getSurfaceHeightForFootprint(spawnPos, typeDef->size, glm::vec2(0.0f), basePos.z) + 0.1f;

    return spawnDebugVehicleAt(vehicleTypeId, spawnPos, heading);
}

bool Scene::spawnDebugVehicleAt(const std::string& vehicleTypeId, const glm::vec3& position, float headingDeg) {
    if (!m_tileGrid) {
        return false;
    }

    const std::string normalizedType = normalizeDebugId(vehicleTypeId);
    const auto& config = VehicleConfig::getInstance();
    const auto* typeDef = config.getDefinition(normalizedType);
    if (!typeDef) {
        return false;
    }

    auto vehicle = std::make_unique<Vehicle>();
    vehicle->setVehicleType(normalizedType);
    vehicle->initialize(typeDef->texturePath);
    vehicle->setSpriteSize(typeDef->size);
    vehicle->setPosition(position);
    vehicle->setRotation(glm::vec3(0.0f, 0.0f, headingDeg));
    vehicle->setOwner(VehicleOwner::World);
    addVehicle(std::move(vehicle));

    std::cout << "Spawned vehicle " << normalizedType << " at ("
              << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;
    return true;
}

bool Scene::spawnDebugOfficerAt(const glm::vec3& position, float headingDeg) {
    return m_policeChaseManager && m_policeChaseManager->debugSpawnOfficerAt(position, headingDeg);
}

bool Scene::setDebugPlayerPose(const glm::vec3& position, float headingDeg) {
    if (!m_player) {
        return false;
    }
    if (m_gameLogic && m_gameLogic->isPlayerInVehicle()) {
        if (Vehicle* activeVehicle = m_gameLogic->getActiveVehicle()) {
            activeVehicle->setPosition(position);
            activeVehicle->setRotation(glm::vec3(0.0f, 0.0f, headingDeg));
            activeVehicle->setSpeed(0.0f);
        }
    }
    m_player->setPosition(position);
    m_player->setRotation(glm::vec3(0.0f, 0.0f, headingDeg));
    std::cout << "Set player pose to (" << position.x << ", " << position.y << ", " << position.z
              << ") heading=" << headingDeg << std::endl;
    return true;
}

bool Scene::debugEnterNearestVehicle(float radius) {
    return m_gameLogic && m_gameLogic->tryEnterNearestVehicle(radius);
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

    // Weapon switching with +/-
    if (m_player && (!m_gameLogic || !m_gameLogic->isPlayerInVehicle())) {
        if (input->isKeyPressed(GLFW_KEY_EQUAL) || input->isKeyPressed(GLFW_KEY_KP_ADD)) {
            m_player->switchWeaponNext();
        }
        if (input->isKeyPressed(GLFW_KEY_MINUS) || input->isKeyPressed(GLFW_KEY_KP_SUBTRACT)) {
            m_player->switchWeaponPrev();
        }
    }

    const bool ctrlDown = input->isKeyDown(GLFW_KEY_LEFT_CONTROL) || input->isKeyDown(GLFW_KEY_RIGHT_CONTROL);
    const bool ctrlPressed = input->isKeyPressed(GLFW_KEY_LEFT_CONTROL) || input->isKeyPressed(GLFW_KEY_RIGHT_CONTROL);
    if (!editActive && !captureKeyboard && m_player && m_player->canShoot()
        && (!m_gameLogic || !m_gameLogic->isPlayerInVehicle())) {
        const Weapon* weapon = m_player->getEquippedWeapon();
        const bool shouldFire = weapon && weapon->isAutoFire() ? ctrlDown : ctrlPressed;
        if (shouldFire) {
            fireWeaponShot();
            m_player->recordShot();
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
        const std::string levelPath = "assets/levels/test_level_large.tg";
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

    if (m_policeChaseManager) {
        m_policeChaseManager->resetForWorldRestart();
    }

    m_vehicles.clear();
    m_missionSystem.reset();
    m_showMissionPrompt = false;
    m_promptJob = nullptr;
    m_missionCompletedTimer = 0.0f;
    m_completedBoothId.clear();
    
    // Reset traffic manager
    if (m_trafficManager) {
        m_trafficManager->reset();
    }
    
    // Reset pedestrian manager
    if (m_pedestrianManager) {
        m_pedestrianManager->reset();
    }
    
    if (!m_tileGrid) {
        return;
    }

    rebuildPickupsFromSpawns();
    rebuildPhoneBoothsFromSpawns();

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

    // Set up collision and explode callbacks for all vehicles and player
    setupCollisionCallbacks();

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
        pickup->setAmmoAmount(spawn.ammo);
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

    for (auto& pickup : m_pickups) {
        if (!pickup || !pickup->isActive()) {
            continue;
        }
        const glm::vec3 pickupPos = pickup->getPosition();
        const glm::vec2 delta(pickupPos.x - playerPos.x, pickupPos.y - playerPos.y);
        const float radius = playerRadius + pickup->getRadius();
        const float distanceSq = delta.x * delta.x + delta.y * delta.y;
        if (distanceSq <= radius * radius) {
            const PickupType pickupType = pickup->getType();
            switch (pickupType) {
                case PickupType::Pistol: {
                    if (!m_player->addAmmo(pickupType, pickup->getAmmoAmount())) {
                        m_player->equipWeapon(pickupType, std::make_unique<PistolWeapon>(pickup->getAmmoAmount()));
                    }
                    std::cout << "Picked up pistol (" << pickup->getAmmoAmount() << " ammo)" << std::endl;
                    break;
                }
                case PickupType::MachineGun: {
                    if (!m_player->addAmmo(pickupType, pickup->getAmmoAmount())) {
                        m_player->equipWeapon(pickupType, std::make_unique<MachineGunWeapon>(pickup->getAmmoAmount()));
                    }
                    std::cout << "Picked up machine gun (" << pickup->getAmmoAmount() << " ammo)" << std::endl;
                    break;
                }
            }
            pickup->startRespawn();
            break;
        }
    }
}

void Scene::fireWeaponShot() {
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
    
    // Notify police chase manager that the player fired a weapon
    if (m_policeChaseManager) {
        m_policeChaseManager->onPlayerFiredWeapon();
    }
    
    m_projectileManager.spawnProjectile(glm::vec3(origin.x, origin.y, playerPos.z + 0.15f), dir,
                                        kProjectileSpeed, kMaxRange);
}

std::vector<const Collider*> Scene::getAllColliders() const {
    std::vector<const Collider*> allColliders;
    
    // Add player if active and not in vehicle
    if (m_player && m_player->isActive()) {
        if (!m_gameLogic || !m_gameLogic->isPlayerInVehicle()) {
            allColliders.push_back(m_player.get());
        }
    }
    
    // Add all vehicles (world, traffic, and police are all in m_vehicles)
    for (const auto& vehicle : m_vehicles) {
        if (vehicle && vehicle->isActive()) {
            allColliders.push_back(vehicle.get());
        }
    }
    
    return allColliders;
}

std::vector<const Collider*> Scene::getCharacterPhysicsObstacles() const {
    std::vector<const Collider*> obstacles;
    obstacles.reserve(m_vehicles.size());
    for (const auto& vehicle : m_vehicles) {
        if (vehicle && vehicle->isActive()) {
            obstacles.push_back(vehicle.get());
        }
    }
    return obstacles;
}

void Scene::setupCollisionCallbacks() {
    // Set up collision and explode callbacks for all vehicles
    for (auto& vehicle : m_vehicles) {
        if (vehicle) {
            vehicle->setCollisionCallback([this]() { return getAllColliders(); });
            vehicle->setExplodeCallback([this](Vehicle* exploded) { handleVehicleExploded(exploded); });
        }
    }
}

void Scene::handleVehicleExploded(Vehicle* vehicle) {
    if (!vehicle) {
        return;
    }

    const bool explosionAttributedToGunfire = vehicle->wasShotByPlayer();
    if (explosionAttributedToGunfire && m_policeChaseManager) {
        m_policeChaseManager->onPlayerCausedVehicleExplosion();
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
                if (explosionAttributedToGunfire) {
                    target->markShotByPlayer();
                }
                target->applyHit(damage);
            }
        }
    };

    for (auto& v : m_vehicles) {
        applyExplosionDamage(v.get());
    }

    const bool playerInside = m_gameLogic && m_gameLogic->getActiveVehicle() == vehicle;
    if (playerInside && !m_debugInvincible) {
        restartLevel();
    }
}

void Scene::restartLevel() {
    rebuildVehiclesFromSpawns();
    std::cout << "Restarted level after vehicle explosion" << std::endl;
}

void Scene::rebuildPhoneBoothsFromSpawns() {
    m_phoneBooths.clear();
    if (!m_tileGrid) {
        return;
    }

    auto& texMgr = TextureManager::instance();
    for (const auto& spawn : m_levelData.phoneBooths) {
        PhoneBoothRuntime booth;
        const float tileSize = m_tileGrid->getTileSize();
        glm::vec3 worldPos(
            spawn.gridPosition.x * tileSize,
            spawn.gridPosition.y * tileSize,
            spawn.gridPosition.z * tileSize + 0.1f);
        booth.worldPos = worldPos;
        booth.id = spawn.id;
        booth.jobs.setJobs(spawn.jobIds);
        for (const auto& jobId : spawn.jobIds) {
            if (!m_missionSystem.findJob(jobId)) {
                std::cerr << "[Scene] Phone booth '" << booth.id
                          << "' references unknown job '" << jobId << "'\n";
            }
        }
        booth.texInactive = texMgr.getTextureFromPath("assets/textures/phone.png");
        booth.texActive   = texMgr.getTextureFromPath("assets/textures/phone-active.png");
        m_phoneBooths.push_back(std::move(booth));
    }

    // Wire marker lookup and tileGrid for mission system
    if (m_tileGrid) {
        m_missionSystem.setTileGrid(m_tileGrid.get());
        m_missionSystem.setMarkerLookup([this](const std::string& name) -> glm::vec3 {
            for (const auto& marker : m_levelData.markers) {
                if (marker.name == name) {
                    // Use same z formula as vehicles: z * tileSize + offset
                    // (gridToWorld uses (z-1)*tileSize which is one layer lower)
                    const float tileSize = m_tileGrid->getTileSize();
                    glm::vec3 world(
                        marker.gridPosition.x * tileSize,
                        marker.gridPosition.y * tileSize,
                        marker.gridPosition.z * tileSize + 0.1f);
                    return world;
                }
            }
            return glm::vec3(0.0f);
        });
    }
}

void Scene::handlePhoneBoothInteraction(float deltaTime) {
    // Count down the post-completion cooldown / banner
    if (m_missionCompletedTimer > 0.0f) {
        m_missionCompletedTimer -= deltaTime;
        if (m_missionCompletedTimer <= 0.0f && m_missionSystem.getState() == MissionState::Completed) {
            // Advance the booth to its next mission, then reset to Idle
            advanceBoothJob(m_completedBoothId);
            m_missionSystem.reset();
            m_completedBoothId.clear();
        }
        return;  // Don't process new booth interactions during cooldown
    }

    // Check for mission success or failure
    if (m_missionSystem.getState() == MissionState::Active) {
        if (m_missionSystem.update(deltaTime, *this)) {
            const Job* completedJob = m_missionSystem.getActiveJob();
            if (completedJob && m_player) {
                m_player->addMoney(completedJob->rewardMoney);
            }
            m_missionCompletedTimer = kMissionCooldownTime;
            m_completedBoothId = m_missionSystem.getActiveBoothId();
            m_showMissionPrompt = false;
        }
        // If the mission just failed, reset immediately so the booth is available again
        if (m_missionSystem.getState() == MissionState::Failed) {
            m_missionSystem.reset();
        }
        return;
    }

    if (m_missionSystem.getState() == MissionState::Completed) {
        return;
    }

    if (m_gameLogic && m_gameLogic->isPlayerInVehicle()) {
        m_showMissionPrompt = false;
        m_promptJob = nullptr;
        return;
    }

    // Check proximity to active phone booths
    glm::vec3 playerPos(0.0f);
    if (m_player) {
        playerPos = m_player->getPosition();
    }

    constexpr float kPromptRadius = 3.5f;

    m_showMissionPrompt = false;
    for (const auto& booth : m_phoneBooths) {
        const std::string* jobId = booth.jobs.currentJobId();
        if (!jobId || !m_missionSystem.isBoothActive(*jobId, *this)) {
            continue;
        }
        const float dx = playerPos.x - booth.worldPos.x;
        const float dy = playerPos.y - booth.worldPos.y;
        if (dx * dx + dy * dy <= kPromptRadius * kPromptRadius) {
            const Job* job = m_missionSystem.findJob(*jobId);
            if (job) {
                m_showMissionPrompt = true;
                m_promptJob = job;
                m_promptBoothId = booth.id;
                m_promptBoothWorldPos = booth.worldPos;
            }
            break;
        }
    }
}

void Scene::advanceBoothJob(const std::string& boothId) {
    for (auto& booth : m_phoneBooths) {
        if (booth.id == boothId) {
            if (booth.jobs.advance()) {
                const std::string* currentJobId = booth.jobs.currentJobId();
                std::cout << "[Scene] Booth '" << boothId
                          << "' advanced to job '" << *currentJobId << "'\n";
            } else {
                std::cout << "[Scene] Booth '" << boothId
                          << "' completed its configured job sequence\n";
            }
            break;
        }
    }
}

void Scene::drawMissionGui() {
    if (isEditModeActive()) {
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    const float screenW = io.DisplaySize.x;
    const float screenH = io.DisplaySize.y;

    // Mission prompt
    if (m_showMissionPrompt && m_promptJob && m_missionSystem.getState() == MissionState::Idle) {
        ImGui::SetNextWindowPos(ImVec2(screenW * 0.5f, screenH * 0.65f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Always);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize
            | ImGuiWindowFlags_NoFocusOnAppearing;
        if (ImGui::Begin("MissionPrompt", nullptr, flags)) {
            ImGui::TextUnformatted(m_promptJob->title.c_str());
            ImGui::Separator();
            ImGui::TextWrapped("%s", m_promptJob->description.c_str());
            ImGui::Spacing();
            const bool playerInVehicle = m_gameLogic && m_gameLogic->isPlayerInVehicle();
            if (!playerInVehicle && (ImGui::Button("Accept [Enter]") || ImGui::IsKeyPressed(ImGuiKey_Enter))) {
                m_missionSystem.startMission(m_promptJob, m_promptBoothId, m_promptBoothWorldPos);
                m_showMissionPrompt = false;
            } else if (playerInVehicle) {
                ImGui::BeginDisabled();
                ImGui::Button("Accept [Enter]");
                ImGui::EndDisabled();
            }
        }
        ImGui::End();
        return;
    }

    // Active mission HUD
    if (m_missionSystem.getState() == MissionState::Active) {
        const Job* job = m_missionSystem.getActiveJob();
        if (job) {
            ImGui::SetNextWindowPos(ImVec2(screenW * 0.5f, 12.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
                | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize
                | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBackground;
            if (ImGui::Begin("MissionHUD", nullptr, flags)) {
                ImGui::TextUnformatted(("MISSION: " + job->title).c_str());
            }
            ImGui::End();
        }
        return;
    }

    // Completion banner (show only during the first kMissionBannerTime seconds of cooldown)
    if (m_missionCompletedTimer > kMissionCooldownTime - kMissionBannerTime) {
        const Job* job = m_missionSystem.getActiveJob();
        const std::string msg = job ? job->completionMessage : "Mission complete!";
        ImGui::SetNextWindowPos(ImVec2(screenW * 0.5f, screenH * 0.4f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize
            | ImGuiWindowFlags_NoFocusOnAppearing;
        if (ImGui::Begin("MissionComplete", nullptr, flags)) {
            ImGui::TextUnformatted(msg.c_str());
        }
        ImGui::End();
    }
}
