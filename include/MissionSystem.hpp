#pragma once

#include <functional>
#include <string>
#include <vector>
#include <glm/glm.hpp>

class Scene;
class Vehicle;

enum class MissionState {
    Idle,       // No active mission
    Prompted,   // Player is at phone, waiting for acceptance (not used yet, auto-start)
    Active,     // Mission in progress
    Completed,  // Mission successfully completed
    Failed      // Mission failed (unused currently)
};

struct Job {
    std::string id;
    std::string title;
    std::string description;
    std::string completionMessage;
    // Returns true when the phone booth should appear active (glowing)
    std::function<bool(const Scene&)> activationCondition;
    // Returns true when the mission is successfully completed. Receives booth world pos.
    std::function<bool(const Scene&, const glm::vec3& boothWorldPos)> successCondition;
};

class MissionSystem {
public:
    MissionSystem();

    // Register all built-in jobs
    void registerBuiltinJobs();

    const Job* findJob(const std::string& jobId) const;

    MissionState getState() const { return m_state; }
    const Job* getActiveJob() const { return m_activeJob; }
    const std::string& getActiveBoothId() const { return m_activeBoothId; }
    glm::vec3 getActiveBoothWorldPos() const { return m_activeBoothWorldPos; }

    // Called when player walks up to an active booth
    void startMission(const Job* job, const std::string& boothId, const glm::vec3& boothWorldPos);

    // Returns true if just completed
    bool update(const Scene& scene);

    void reset();

    bool isBoothActive(const std::string& jobId, const Scene& scene) const;

private:
    std::vector<Job> m_jobs;
    MissionState m_state = MissionState::Idle;
    const Job* m_activeJob = nullptr;
    std::string m_activeBoothId;
    glm::vec3 m_activeBoothWorldPos{0.0f};
};
