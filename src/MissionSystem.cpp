#include "MissionSystem.hpp"
#include "Scene.hpp"
#include "GameLogic.hpp"
#include "Vehicle.hpp"
#include <glm/geometric.hpp>
#include <iostream>

MissionSystem::MissionSystem() {
    registerBuiltinJobs();
}

void MissionSystem::registerBuiltinJobs() {
    // Job 1: Bring a police vehicle to the phone booth
    Job policeCar;
    policeCar.id = "police_delivery";
    policeCar.title = "Hot Wheels";
    policeCar.description =
        "Get a police vehicle and bring it to this booth.\n\n"
        "Find a police car, steal it, and drive back here.";
    policeCar.completionMessage = "Mission complete! Nice wheels, pal.";

    // Always active
    policeCar.activationCondition = [](const Scene& /*scene*/) -> bool {
        return true;
    };

    // Success: player drives a police vehicle within range of the booth
    policeCar.successCondition = [](const Scene& scene, const glm::vec3& boothWorldPos) -> bool {
        const GameLogic* logic = scene.getGameLogic();
        if (!logic || !logic->isPlayerInVehicle()) {
            return false;
        }
        const Vehicle* vehicle = logic->getActiveVehicle();
        if (!vehicle || vehicle->getVehicleTypeId() != "police") {
            return false;
        }
        const glm::vec3 vehiclePos = vehicle->getPosition();
        const float dx = vehiclePos.x - boothWorldPos.x;
        const float dy = vehiclePos.y - boothWorldPos.y;
        const float distSq = dx * dx + dy * dy;
        constexpr float kSuccessRadius = 4.0f;
        return distSq <= kSuccessRadius * kSuccessRadius;
    };

    m_jobs.push_back(std::move(policeCar));
}

const Job* MissionSystem::findJob(const std::string& jobId) const {
    for (const auto& job : m_jobs) {
        if (job.id == jobId) {
            return &job;
        }
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
    }
}

bool MissionSystem::update(const Scene& scene) {
    if (m_state != MissionState::Active || !m_activeJob) {
        return false;
    }
    if (m_activeJob->successCondition(scene, m_activeBoothWorldPos)) {
        m_state = MissionState::Completed;
        std::cout << "[Mission] Completed: " << m_activeJob->title << std::endl;
        return true;
    }
    return false;
}

void MissionSystem::reset() {
    m_state = MissionState::Idle;
    m_activeJob = nullptr;
    m_activeBoothId.clear();
    m_activeBoothWorldPos = glm::vec3(0.0f);
}

bool MissionSystem::isBoothActive(const std::string& jobId, const Scene& scene) const {
    // A booth is visually active if the job's activation condition is met
    // and we don't already have an active or completed mission from this booth
    if (m_state == MissionState::Active && m_activeBoothId == jobId) {
        return false; // Mission running — booth goes back to inactive
    }
    if (m_state == MissionState::Completed) {
        return false; // Completed — no longer active
    }
    const Job* job = findJob(jobId);
    if (!job) {
        return false;
    }
    return job->activationCondition(scene);
}
