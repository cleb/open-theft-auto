#include "EscortMissionState.hpp"
#include "CombatPedestrian.hpp"
#include "SpriteAnimation.hpp"
#include "Scene.hpp"
#include "GameLogic.hpp"
#include "Vehicle.hpp"
#include "Renderer.hpp"
#include "Texture.hpp"
#include "Heading.hpp"
#include "TileGrid.hpp"
#include <glm/geometric.hpp>
#include <cmath>
#include <iostream>

EscortMissionState::EscortMissionState() = default;
EscortMissionState::~EscortMissionState() = default;

void EscortMissionState::start(const glm::vec3& boothWorldPos,
                               const glm::vec3& pickupWorldPos, const glm::vec3& escortWorldPos) {
    m_boothPos = boothWorldPos;
    m_pickupPos = pickupWorldPos;
    m_escortSpawnPos = escortWorldPos;
    m_escortCurrentPos = escortWorldPos;

    m_phase = EscortPhase::WaitingPickup;
    m_escortVisible = true;
    m_escortAnimTime = 0.0f;
    m_enemySpawnTimer = 0.0f;
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

    std::cout << "[EscortMission] Started. Pickup at ("
              << m_pickupPos.x << ", " << m_pickupPos.y << ")\n";
}

void EscortMissionState::reset() {
    m_phase = EscortPhase::WaitingPickup;
    m_escortVisible = false;
    m_enemySpawnTimer = 0.0f;
    m_boardedVehicle = nullptr;
    m_enemies.clear();
}

void EscortMissionState::spawnEnemies() {
    if (!m_enemyAnimation || !m_enemyAnimation->getTexture()) {
        std::cerr << "[EscortMission] Cannot spawn enemies: no animation loaded\n";
        return;
    }

    const glm::vec3 offsets[3] = {
        {-3.0f,  0.0f, 0.0f},
        { 3.0f, -2.0f, 0.0f},
        { 0.0f,  3.5f, 0.0f}
    };

    m_enemies.clear();

    for (int i = 0; i < 3; ++i) {
        auto enemy = std::make_unique<CombatPedestrian>();
        enemy->spawn(m_enemyAnimation.get(), m_tileGrid,
                     m_pickupPos + offsets[i], 270.0f);
        enemy->setShootCallback(m_shootCallback);
        enemy->setSpeed(2.5f);
        enemy->setFireDistance(18.0f);
        enemy->setChaseDistance(5.0f);
        enemy->setShootCooldown(0.55f);
        m_enemies.push_back(std::move(enemy));
    }

    std::cout << "[EscortMission] 3 enemies spawned\n";
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

void EscortMissionState::checkVehicleCollision(const glm::vec3& vehiclePos, const glm::vec2& vehicleSize, float vehicleRotation) {
    for (auto& enemy : m_enemies) {
        if (enemy) {
            enemy->checkVehicleCollision(vehiclePos, vehicleSize, vehicleRotation);
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
        m_escortAnimTime += deltaTime;

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
        m_escortAnimTime += deltaTime;

        // Walk toward the boarded vehicle
        const glm::vec3 targetPos = m_boardedVehicle->getPosition();
        const float dx = targetPos.x - m_escortCurrentPos.x;
        const float dy = targetPos.y - m_escortCurrentPos.y;
        const float dist = std::sqrt(dx * dx + dy * dy);

        if (dist <= kBoardRadius) {
            // Reached the vehicle — board it
            m_escortVisible = false;
            m_phase = EscortPhase::EscortBoarded;
            m_enemySpawnTimer = 0.0f;
            std::cout << "[EscortMission] Escort boarded. Enemies spawn in "
                      << kEnemySpawnDelay << "s\n";
        } else {
            const float step = kEscortWalkSpeed * deltaTime;
            m_escortCurrentPos.x += (dx / dist) * step;
            m_escortCurrentPos.y += (dy / dist) * step;
        }
        return;
    }

    if (m_phase == EscortPhase::EscortBoarded) {
        m_enemySpawnTimer += deltaTime;
        if (m_enemySpawnTimer >= kEnemySpawnDelay) {
            spawnEnemies();
            m_phase = EscortPhase::EnemiesActive;
        }
    }

    if (m_phase == EscortPhase::EscortBoarded || m_phase == EscortPhase::EnemiesActive) {
        if (m_phase == EscortPhase::EnemiesActive) {
            const glm::vec3 target = vehicle ? vehiclePos : glm::vec3(0.0f);
            for (auto& enemy : m_enemies) {
                if (enemy) enemy->update(deltaTime, target);
            }
        }

        // Must return in the SAME vehicle
        if (inVehicle && vehicle == m_boardedVehicle) {
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

void EscortMissionState::renderEscortChar(Renderer* renderer) const {
    if (!m_escortVisible || !m_escortAnimation || !m_escortAnimation->getTexture()) return;

    constexpr float kFrameDuration = 0.1f;
    constexpr int kNumFrames = 8;
    constexpr float kCycle = kFrameDuration * kNumFrames;

    const float wrapped = std::fmod(m_escortAnimTime, kCycle);
    const int frameIndex = static_cast<int>(wrapped / kFrameDuration) % kNumFrames;

    const float fw = static_cast<float>(m_escortAnimation->getFrameWidth());
    const float fh = static_cast<float>(m_escortAnimation->getFrameHeight());
    const float tw = static_cast<float>(m_escortAnimation->getTexture()->getWidth());
    const float th = static_cast<float>(m_escortAnimation->getTexture()->getHeight());
    const glm::vec4 uv(frameIndex * fw / tw, 0.0f, fw / tw, fh / th);

    // Compute facing direction when walking
    float rotation = 270.0f;
    if (m_phase == EscortPhase::WalkingToVehicle && m_boardedVehicle) {
        const glm::vec3 vPos = m_boardedVehicle->getPosition();
        const float dx = vPos.x - m_escortCurrentPos.x;
        const float dy = vPos.y - m_escortCurrentPos.y;
        if (dx * dx + dy * dy > 0.01f) {
            rotation = std::atan2(dy, dx) * 180.0f / 3.14159265f;
        }
    }

    renderer->renderAnimatedSprite(*m_escortAnimation->getTexture(),
                                   glm::vec2(m_escortCurrentPos.x, m_escortCurrentPos.y),
                                   glm::vec2(0.8f, 0.8f), uv, rotation, glm::vec3(1.0f));
}

void EscortMissionState::render(Renderer* renderer) const {
    if (!renderer || m_phase == EscortPhase::Complete || m_phase == EscortPhase::Failed) return;
    renderMarkers(renderer);
    renderEscortChar(renderer);

    for (const auto& enemy : m_enemies) {
        if (enemy) enemy->render(renderer);
    }
}
