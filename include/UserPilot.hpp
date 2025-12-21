#pragma once

#include "Pilot.hpp"
#include <glm/glm.hpp>

class InputManager;

// Player-controlled vehicle pilot
class UserPilot : public Pilot {
public:
    UserPilot();
    ~UserPilot() override = default;
    
    void update(Vehicle* vehicle, TileGrid* tileGrid, float deltaTime) override;
    
    // Set the input manager for reading player input
    void setInputManager(InputManager* inputManager) { m_inputManager = inputManager; }
    
private:
    InputManager* m_inputManager = nullptr;
};
