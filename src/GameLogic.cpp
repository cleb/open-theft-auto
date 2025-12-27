#include "GameLogic.hpp"
#include "InputManager.hpp"
#include "UserPilot.hpp"
#include "TrafficManager.hpp"
#include <glm/glm.hpp>
#include <glm/geometric.hpp>
#include <GLFW/glfw3.h>
#include <iostream>

GameLogic::GameLogic()
    : m_currentControllable(nullptr)
    , m_previousControllable(nullptr)
    , m_player(nullptr)
    , m_vehicles(nullptr)
    , m_inputManager(nullptr)
    , m_trafficManager(nullptr) {
}

void GameLogic::setPlayer(Player* player) {
    m_player = player;
    if (m_player && !m_currentControllable) {
        m_currentControllable = m_player;
    }
}

void GameLogic::setVehicles(std::vector<std::unique_ptr<Vehicle>>* vehicles) {
    m_vehicles = vehicles;
}

void GameLogic::processInput(InputManager* input, float deltaTime) {
    if (!input || !m_currentControllable) {
        return;
    }

    // Handle enter/exit vehicle
    const bool enterPressed = input->isKeyPressed(GLFW_KEY_ENTER) || input->isKeyPressed(GLFW_KEY_KP_ENTER);
    if (enterPressed) {
        if (isPlayerInVehicle()) {
            leaveVehicle();
        } else {
            tryEnterNearestVehicle();
        }
    }

    if (input->isKeyPressed(GLFW_KEY_F) && isPlayerInVehicle()) {
        leaveVehicle();
    }

    // Send input to current controllable object
    if (input->isKeyDown(GLFW_KEY_W) || input->isKeyDown(GLFW_KEY_UP)) {
        m_currentControllable->moveForward(deltaTime);
    }
    if (input->isKeyDown(GLFW_KEY_S) || input->isKeyDown(GLFW_KEY_DOWN)) {
        m_currentControllable->moveBackward(deltaTime);
    }
    if (input->isKeyDown(GLFW_KEY_A) || input->isKeyDown(GLFW_KEY_LEFT)) {
        m_currentControllable->turnLeft(deltaTime);
    }
    if (input->isKeyDown(GLFW_KEY_D) || input->isKeyDown(GLFW_KEY_RIGHT)) {
        m_currentControllable->turnRight(deltaTime);
    }
}

void GameLogic::update(float deltaTime) {
    (void)deltaTime; // Suppress unused parameter warning
    
    // Sync player position with vehicle if in vehicle
    if (isPlayerInVehicle() && m_player) {
        Vehicle* vehicle = getActiveVehicle();
        if (vehicle) {
            m_player->setPosition(vehicle->getPosition());
            m_player->setRotation(vehicle->getRotation());
        }
    }
}

bool GameLogic::tryEnterNearestVehicle(float radius) {
    if (!m_player || !m_vehicles || isPlayerInVehicle()) {
        return false;
    }

    Vehicle* nearestVehicle = nullptr;
    bool nearestIsTraffic = false;
    float nearestDistance = radius;
    const glm::vec2 playerPos(m_player->getPosition().x, m_player->getPosition().y);

    auto considerVehicle = [&](Vehicle* vehicle, bool isTraffic) {
        if (!vehicle || !vehicle->isActive()) {
            return;
        }

        const glm::vec3 vehiclePos3 = vehicle->getPosition();
        const glm::vec2 vehiclePos(vehiclePos3.x, vehiclePos3.y);
        const float distance = glm::length(vehiclePos - playerPos);

        if (distance <= nearestDistance) {
            nearestVehicle = vehicle;
            nearestIsTraffic = isTraffic;
            nearestDistance = distance;
        }
    };

    for (auto& vehicle : *m_vehicles) {
        considerVehicle(vehicle.get(), false);
    }

    if (m_trafficManager) {
        for (const auto& vehicle : m_trafficManager->getTrafficVehicles()) {
            considerVehicle(vehicle.get(), true);
        }
    }

    if (!nearestVehicle) {
        return false;
    }

    if (nearestIsTraffic && m_trafficManager) {
        std::unique_ptr<Vehicle> claimed = m_trafficManager->claimTrafficVehicle(nearestVehicle);
        if (claimed) {
            nearestVehicle = claimed.get();
            m_vehicles->push_back(std::move(claimed));
        }
    }

    // Switch control to vehicle with UserPilot
    m_previousControllable = m_currentControllable;
    m_currentControllable = nearestVehicle;
    
    // Create and assign a UserPilot to the vehicle
    auto userPilot = std::make_unique<UserPilot>();
    userPilot->setInputManager(m_inputManager);
    nearestVehicle->setPilot(std::move(userPilot));
    
    m_player->setActive(false);
    m_player->setPosition(nearestVehicle->getPosition());
    m_player->setRotation(nearestVehicle->getRotation());
    
    return true;
}

void GameLogic::leaveVehicle() {
    Vehicle* vehicle = getActiveVehicle();
    if (!vehicle || !m_player) {
        return;
    }

    // Switch control back to player
    m_currentControllable = m_player;
    
    m_player->setActive(true);
    m_player->setPosition(vehicle->getPosition());
    m_player->setRotation(vehicle->getRotation());
    
    // Remove the UserPilot from the vehicle
    vehicle->clearPilot();
    m_previousControllable = nullptr;
}

bool GameLogic::isPlayerInVehicle() const {
    if (!m_player || !m_currentControllable) {
        return false;
    }
    return m_currentControllable != m_player;
}

Vehicle* GameLogic::getActiveVehicle() const {
    if (!isPlayerInVehicle()) {
        return nullptr;
    }
    return dynamic_cast<Vehicle*>(m_currentControllable);
}

void GameLogic::reset() {
    m_currentControllable = m_player;
    m_previousControllable = nullptr;
    
    if (m_player) {
        m_player->setActive(true);
    }
    
    if (m_vehicles) {
        for (auto& vehicle : *m_vehicles) {
            if (vehicle) {
                vehicle->clearPilot();
            }
        }
    }
}
