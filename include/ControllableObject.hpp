#pragma once

#include "GameObject.hpp"

class ControllableObject : public GameObject {
public:
    ControllableObject() = default;
    virtual ~ControllableObject() = default;
    
    // Input handling methods that all controllable objects must implement
    virtual void moveForward(float deltaTime) = 0;
    virtual void moveBackward(float deltaTime) = 0;
    virtual void turnLeft(float deltaTime) = 0;
    virtual void turnRight(float deltaTime) = 0;
};
