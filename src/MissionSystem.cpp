#include "MissionSystem.hpp"
#include "EscortMissionState.hpp"
#include "Scene.hpp"
#include "GameLogic.hpp"
#include "Vehicle.hpp"
#include "Renderer.hpp"
#include <glm/geometric.hpp>
#include <iostream>

MissionSystem::MissionSystem() {
    m_escortState = std::make_shared<EscortMissionState>();
    registerBuiltinJobs();
}

void MissionSystem::setEnemyShootCallback(ShootCallback cb) {
    m_enemyShootCallback = cb;
    if (m_escortState) {
        m_escortState->setShootCallback(cb);
    }
}

void MissionSystem::setTileGrid(TileGrid* tileGrid) {
    if (m_escortState) {
        m_escortState->setTileGrid(tileGrid);
    }
}

void MissionSystem::registerBuiltinJobs() {
    m_jobs.clear();

    // ── Job 1: Bring a police vehicle to the phone booth ────────────────────
    Job policeCar;
    policeCar.id = "police_delivery";
    policeCar.title = "Hot Wheels";
    policeCar.description =
        "Get a police vehicle and bring it to this booth.\n\n"
        "Find a police car, steal it, and drive back here.";
    policeCar.completionMessage = "Mission complete! Nice wheels, pal.";
    policeCar.nextJobId = "escort_mission";

    policeCar.activationCondition = [](const Scene& /*scene*/) -> bool {
        return true;
    };

    policeCar.successCondition = [](const Scene& scene, const glm::vec3& boothWorldPos) -> bool {
        const GameLogic* logic = scene.getGameLogic();
        if (!logic || !logic->isPlayerInVehicle()) return false;
        const Vehicle* vehicle = logic->getActiveVehicle();
        if (!vehicle || vehicle->getVehicleTypeId() != "police") return false;
        const glm::vec3 vehiclePos = vehicle->getPosition();
        const float dx = vehiclePos.x - boothWorldPos.x;
        const float dy = vehiclePos.y - boothWorldPos.y;
        constexpr float kSuccessRadius = 4.0f;
        return dx * dx + dy * dy <= kSuccessRadius * kSuccessRadius;
    };

    m_jobs.push_back(std::move(policeCar));

    // ── Job 2: Escort mission ────────────────────────────────────────────────
    auto escortState = m_escortState;

    Job escort;
    escort.id = "escort_mission";
    escort.title = "Safe Passage";
    escort.description =
        "Drive to the marked location and pick up the contact.\n\n"
        "Get there by car, load the contact, and bring them back here safely.";
    escort.completionMessage = "Mission complete! The contact is safe.";
    escort.nextJobId = "police_delivery";

    escort.activationCondition = [](const Scene& /*scene*/) -> bool {
        return true;
    };

    escort.onStart = [escortState, this](const glm::vec3& boothWorldPos) {
        escortState->setShootCallback(m_enemyShootCallback);

        glm::vec3 pickupPos = boothWorldPos + glm::vec3(12.0f, 0.0f, 0.0f);
        glm::vec3 escortCharPos = pickupPos + glm::vec3(0.0f, 2.0f, 0.0f);

        if (m_markerLookup) {
            const glm::vec3 markerPickup = m_markerLookup("escort_pickup");
            const glm::vec3 markerEscort = m_markerLookup("escort_character");
            if (markerPickup != glm::vec3(0.0f)) pickupPos = markerPickup;
            if (markerEscort != glm::vec3(0.0f)) escortCharPos = markerEscort;
        }

        escortState->start(boothWorldPos, pickupPos, escortCharPos);
    };

    escort.onUpdate = [escortState](float deltaTime, const Scene& scene) {
        escortState->update(deltaTime, scene);
    };

    escort.onRender = [escortState](Renderer* renderer) {
        escortState->render(renderer);
    };

    escort.onReset = [escortState]() {
        escortState->reset();
    };

    escort.successCondition = [escortState](const Scene& /*scene*/, const glm::vec3& /*boothWorldPos*/) -> bool {
        return escortState->isComplete();
    };

    m_jobs.push_back(std::move(escort));
}

const Job* MissionSystem::findJob(const std::string& jobId) const {
    for (const auto& job : m_jobs) {
        if (job.id == jobId) return &job;
    }
    return nullptr;
}

void MissionSystem::startMission(const Job* job, const std::string& boothId, const glm::vec3& boothWorldPos) {
    m_activeJob = job;
    m_activeBoothId = boothId;
    m_activeBoothWorldPos = boothWorldPos;
    m_state = MissionState::Active;
    if (job) {
        std::cout << "[Mission] Started: " << job->title << std::endl;
        if (job->onStart) {
            job->onStart(boothWorldPos);
        }
    }
}

bool MissionSystem::update(float deltaTime, const Scene& scene) {
    if (m_state != MissionState::Active || !m_activeJob) return false;

    if (m_activeJob->onUpdate) {
        m_activeJob->onUpdate(deltaTime, scene);
    }

    // Check for escort mission failure (vehicle destroyed)
    if (m_escortState && m_escortState->isFailed()) {
        m_state = MissionState::Failed;
        std::cout << "[Mission] Failed: " << m_activeJob->title << std::endl;
        return false;
    }

    if (m_activeJob->successCondition(scene, m_activeBoothWorldPos)) {
        m_state = MissionState::Completed;
        std::cout << "[Mission] Completed: " << m_activeJob->title << std::endl;
        return true;
    }
    return false;
}

void MissionSystem::render(Renderer* renderer) const {
    if (m_state != MissionState::Active || !m_activeJob) return;
    if (m_activeJob->onRender) {
        m_activeJob->onRender(renderer);
    }
}

void MissionSystem::reset() {
    if (m_activeJob && m_activeJob->onReset) {
        m_activeJob->onReset();
    }
    m_state = MissionState::Idle;
    m_activeJob = nullptr;
    m_activeBoothId.clear();
    m_activeBoothWorldPos = glm::vec3(0.0f);
}

bool MissionSystem::isBoothActive(const std::string& jobId, const Scene& scene) const {
    if (m_state == MissionState::Active && m_activeBoothId == jobId) return false;
    if (m_state == MissionState::Completed) return false;
    const Job* job = findJob(jobId);
    if (!job) return false;
    return job->activationCondition(scene);
}

bool MissionSystem::checkProjectileHitEnemy(const glm::vec2& projPos, float projRadius) {
    if (m_state != MissionState::Active || !m_activeJob) return false;
    if (m_escortState && m_escortState->hasActiveEnemies()) {
        return m_escortState->checkBulletHit(glm::vec3(projPos.x, projPos.y, 0.0f), projRadius);
    }
    return false;
}

void MissionSystem::checkVehicleHitEnemies(const glm::vec3& vehiclePos, const glm::vec2& vehicleSize, float vehicleRotation) {
    if (m_state != MissionState::Active || !m_activeJob) return;
    if (m_escortState && m_escortState->hasActiveEnemies()) {
        m_escortState->checkVehicleCollision(vehiclePos, vehicleSize, vehicleRotation);
    }
}
