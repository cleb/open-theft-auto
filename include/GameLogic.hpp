#pragma once

#include "ControllableObject.hpp"
#include "Player.hpp"
#include "Vehicle.hpp"
#include <vector>
#include <memory>

class InputManager;

class GameLogic {
private:
    ControllableObject* m_currentControllable;
    ControllableObject* m_previousControllable;
    Player* m_player;
    std::vector<std::unique_ptr<Vehicle>>* m_vehicles;

public:
    GameLogic();
    ~GameLogic() = default;

    void setPlayer(Player* player);
    void setVehicles(std::vector<std::unique_ptr<Vehicle>>* vehicles);
    
    void processInput(InputManager* input, float deltaTime);
    void update(float deltaTime);
    
    bool tryEnterNearestVehicle(float radius = 2.0f);
    void leaveVehicle();
    
    ControllableObject* getCurrentControllable() const { return m_currentControllable; }
    bool isPlayerInVehicle() const;
    Vehicle* getActiveVehicle() const;
    
    void reset();
};
