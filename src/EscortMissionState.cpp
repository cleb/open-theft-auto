#include "EscortMissionState.hpp"
#include "CombatPedestrian.hpp"
#include "Pedestrian.hpp"
#include "Player.hpp"
#include "SpriteAnimation.hpp"
#include "Scene.hpp"
#include "GameLogic.hpp"
#include "Vehicle.hpp"
#include "Renderer.hpp"
#include "Heading.hpp"
#include "TileGrid.hpp"
#include <glm/geometric.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>

EscortMissionState::EscortMissionState() = default;
EscortMissionState::~EscortMissionState() = default;

void EscortMissionState::start(const glm::vec3& boothWorldPos,
                               const glm::vec3& pickupWorldPos, const glm::vec3& escortWorldPos) {
    m_boothPos = boothWorldPos;
    m_pickupPos = pickupWorldPos;
    m_escortSpawnPos = escortWorldPos;

    m_phase = EscortPhase::WaitingPickup;
    m_enemySpawnTimer = 0.0f;
    m_enemySpawnCount = 0;
    m_boardedVehicle = nullptr;

    m_enemies.clear();

    if (!m_escortAnimation) {
        m_escortAnimation = std::make_unique<SpriteAnimation>();
        if (!m_escortAnimation->loadFromFile("assets/textures/escort-animation.json")) {
            std::cerr << "[EscortMission] Failed to load escort animation\n";
            m_escortAnimation.reset();
        }
    }

    if (!m_enemyAnimation) {
        m_enemyAnimation = std::make_unique<SpriteAnimation>();
        if (!m_enemyAnimation->loadFromFile("assets/textures/enemy-animation.json")) {
            std::cerr << "[EscortMission] Failed to load enemy animation\n";
            m_enemyAnimation.reset();
        }
    }

    m_escortCharacter.reset();
    if (m_characterPhysics && m_tileGrid && m_escortAnimation) {
        m_escortCharacter = std::make_unique<Pedestrian>(*m_characterPhysics, m_tileGrid);
        m_escortCharacter->initialize(m_escortAnimation.get());
        m_escortCharacter->setPosition(m_escortSpawnPos);
        m_escortCharacter->setRotation(glm::vec3(0.0f, 0.0f, 270.0f));
        m_escortCharacter->setSpeed(0.0f);
        m_escortCharacter->setActive(true);
    } else {
        std::cerr << "[EscortMission] Cannot create escort character: physics or animation unavailable\n";
    }

    std::cout << "[EscortMission] Started. Pickup at ("
              << m_pickupPos.x << ", " << m_pickupPos.y << ")\n";
}

void EscortMissionState::reset() {
    m_phase = EscortPhase::WaitingPickup;
    m_enemySpawnTimer = 0.0f;
    m_enemySpawnCount = 0;
    m_boardedVehicle = nullptr;
    m_escortCharacter.reset();
    m_enemies.clear();
}

bool EscortMissionState::spawnEnemy() {
    if (!m_enemyAnimation || !m_enemyAnimation->getTexture() ||
        !m_characterPhysics || !m_tileGrid) {
        std::cerr << "[EscortMission] Cannot spawn enemies: physics or animation unavailable\n";
        return false;
    }

    auto enemy = std::make_unique<CombatPedestrian>();
    enemy->spawn(m_enemyAnimation.get(), m_tileGrid, *m_characterPhysics,
                 m_escortSpawnPos, 270.0f);
    enemy->setShootCallback(m_shootCallback);
    enemy->setSpeed(2.5f);
    enemy->setFireDistance(18.0f);
    enemy->setChaseDistance(1.25f);
    enemy->setShootCooldown(0.55f);
    m_enemies.push_back(std::move(enemy));

    std::cout << "[EscortMission] Enemy " << (m_enemySpawnCount + 1)
              << "/" << kEnemyCount << " spawned at escort marker\n";
    return true;
}

bool EscortMissionState::hasActiveEnemies() const {
    if (m_phase != EscortPhase::EnemiesActive) return false;
    for (const auto& e : m_enemies) {
        if (e && e->isAlive()) return true;
    }
    return false;
}

bool EscortMissionState::checkBulletHit(const glm::vec3& bulletPos, float bulletRadius) {
    for (auto& enemy : m_enemies) {
        if (enemy && enemy->checkBulletHit(bulletPos, bulletRadius)) {
            return true;
        }
    }
    return false;
}

void EscortMissionState::checkVehicleCollision(const glm::vec3& vehiclePos, const glm::vec2& vehicleSize,
                                               float vehicleRotation, float vehicleSpeed) {
    for (auto& enemy : m_enemies) {
        if (enemy) {
            enemy->checkVehicleCollision(vehiclePos, vehicleSize, vehicleRotation, vehicleSpeed);
        }
    }
}

void EscortMissionState::update(float deltaTime, const Scene& scene) {
    if (m_phase == EscortPhase::Complete || m_phase == EscortPhase::Failed) return;

    const GameLogic* logic = scene.getGameLogic();
    const bool inVehicle = logic && logic->isPlayerInVehicle();
    const Vehicle* vehicle = inVehicle ? logic->getActiveVehicle() : nullptr;
    const glm::vec3 vehiclePos = vehicle ? vehicle->getPosition() : glm::vec3(0.0f);

    // After boarding, check if the tracked vehicle was destroyed
    if (m_boardedVehicle && (m_boardedVehicle->isWrecked() || m_boardedVehicle->isExploding())) {
        m_phase = EscortPhase::Failed;
        m_enemies.clear();
        std::cout << "[EscortMission] Vehicle destroyed — mission failed\n";
        return;
    }

    if (m_phase == EscortPhase::WaitingPickup) {
        if (m_escortCharacter) {
            m_escortCharacter->update(deltaTime);
        }

        if (inVehicle && vehicle) {
            const float dx = vehiclePos.x - m_pickupPos.x;
            const float dy = vehiclePos.y - m_pickupPos.y;
            if (dx * dx + dy * dy <= kPickupRadius * kPickupRadius) {
                m_phase = EscortPhase::WalkingToVehicle;
                m_boardedVehicle = vehicle;
                std::cout << "[EscortMission] Walking to vehicle\n";
            }
        }
        return;
    }

    if (m_phase == EscortPhase::WalkingToVehicle) {
        if (!m_escortCharacter || !m_boardedVehicle) {
            m_phase = EscortPhase::Failed;
            std::cerr << "[EscortMission] Escort character or vehicle unavailable\n";
            return;
        }
        m_escortCharacter->update(deltaTime);

        // Walk toward the boarded vehicle
        const glm::vec3 targetPos = m_boardedVehicle->getPosition();
        const glm::vec3 escortPos = m_escortCharacter->getPosition();
        const float dx = targetPos.x - escortPos.x;
        const float dy = targetPos.y - escortPos.y;
        const float dist = std::sqrt(dx * dx + dy * dy);

        if (dist <= kBoardRadius) {
            // Reached the vehicle — board it
            m_escortCharacter->setActive(false);
            m_phase = EscortPhase::EscortBoarded;
            m_enemySpawnTimer = 0.0f;
            std::cout << "[EscortMission] Escort boarded. Enemies spawn in "
                      << kEnemySpawnDelay << "s\n";
        } else {
            const glm::vec2 direction(dx / dist, dy / dist);
            const float step = std::min(kEscortWalkSpeed * deltaTime, dist);
            m_escortCharacter->setRotation(
                glm::vec3(0.0f, 0.0f, Heading::headingDegFromForward(direction)));
            m_escortCharacter->tryMove(
                glm::vec3(direction.x * step, direction.y * step, 0.0f),
                CharacterMoveMode::Slide,
                m_boardedVehicle);
        }
        return;
    }

    if ((m_phase == EscortPhase::EscortBoarded || m_phase == EscortPhase::EnemiesActive) &&
        m_enemySpawnCount < kEnemyCount) {
        m_enemySpawnTimer += deltaTime;
        const float nextSpawnDelay =
            m_enemySpawnCount == 0 ? kEnemySpawnDelay : kEnemySpawnInterval;
        if (m_enemySpawnTimer >= nextSpawnDelay) {
            m_enemySpawnTimer = 0.0f;
            if (spawnEnemy()) {
                ++m_enemySpawnCount;
                m_phase = EscortPhase::EnemiesActive;
            }
        }
    }

    if (m_phase == EscortPhase::EscortBoarded || m_phase == EscortPhase::EnemiesActive) {
        if (m_phase == EscortPhase::EnemiesActive) {
            const Player* player = scene.getPlayer();
            if (player) {
                const glm::vec3 target = player->getPosition();
                for (auto& enemy : m_enemies) {
                    if (enemy) enemy->update(deltaTime, target);
                }
            }
        }

        // Must return in the SAME vehicle
        if (m_enemySpawnCount == kEnemyCount &&
            inVehicle && vehicle == m_boardedVehicle) {
            const float dx = vehiclePos.x - m_boothPos.x;
            const float dy = vehiclePos.y - m_boothPos.y;
            if (dx * dx + dy * dy <= kReturnRadius * kReturnRadius) {
                m_phase = EscortPhase::Complete;
                std::cout << "[EscortMission] Escort delivered!\n";
            }
        }
    }
}

void EscortMissionState::renderMarkers(Renderer* renderer) const {
    constexpr glm::vec2 kMarkerSize(1.8f, 1.8f);

    if (m_phase == EscortPhase::WaitingPickup) {
        renderer->renderDebugMarker(glm::vec2(m_pickupPos.x, m_pickupPos.y),
                                    kMarkerSize, glm::vec3(1.0f, 0.9f, 0.0f));
    }

    if (m_phase == EscortPhase::EscortBoarded || m_phase == EscortPhase::EnemiesActive) {
        renderer->renderDebugMarker(glm::vec2(m_boothPos.x, m_boothPos.y),
                                    kMarkerSize, glm::vec3(0.0f, 1.0f, 0.9f));
    }
}

void EscortMissionState::render(Renderer* renderer) const {
    if (!renderer || m_phase == EscortPhase::Complete || m_phase == EscortPhase::Failed) return;
    renderMarkers(renderer);
    if (m_escortCharacter && m_escortCharacter->isActive()) {
        m_escortCharacter->render(renderer);
    }

    for (const auto& enemy : m_enemies) {
        if (enemy) enemy->render(renderer);
    }
}
