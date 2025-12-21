#pragma once

#include <glm/glm.hpp>

class Vehicle;
class TileGrid;

// Abstract base class for vehicle control (AI or player)
class Pilot {
public:
    virtual ~Pilot() = default;
    
    // Called each frame to control the vehicle
    // TileGrid is passed from the vehicle for road/curve awareness
    virtual void update(Vehicle* vehicle, TileGrid* tileGrid, float deltaTime) = 0;
    
    // Called when pilot is assigned to a vehicle
    virtual void onAssign(Vehicle* vehicle) { (void)vehicle; }
    
    // Called when pilot is removed from a vehicle
    virtual void onRelease(Vehicle* vehicle) { (void)vehicle; }
};
