#include "UserPilot.hpp"
#include "Vehicle.hpp"
#include "InputManager.hpp"
#include <GLFW/glfw3.h>

UserPilot::UserPilot() = default;

void UserPilot::update(Vehicle* vehicle, [[maybe_unused]] TileGrid* tileGrid, float deltaTime) {
    if (!vehicle || !m_inputManager) return;
    
    // Process movement input
    if (m_inputManager->isKeyDown(GLFW_KEY_W) || m_inputManager->isKeyDown(GLFW_KEY_UP)) {
        vehicle->moveForward(deltaTime);
    }
    if (m_inputManager->isKeyDown(GLFW_KEY_S) || m_inputManager->isKeyDown(GLFW_KEY_DOWN)) {
        vehicle->moveBackward(deltaTime);
    }
    if (m_inputManager->isKeyDown(GLFW_KEY_A) || m_inputManager->isKeyDown(GLFW_KEY_LEFT)) {
        vehicle->turnLeft(deltaTime);
    }
    if (m_inputManager->isKeyDown(GLFW_KEY_D) || m_inputManager->isKeyDown(GLFW_KEY_RIGHT)) {
        vehicle->turnRight(deltaTime);
    }
}
