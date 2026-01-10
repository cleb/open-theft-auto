#pragma once

#include "Pilot.hpp"
#include "Tile.hpp"
#include <glm/glm.hpp>

// AI-controlled vehicle pilot with curve following
class AutoPilot : public Pilot {
public:
    AutoPilot();
    ~AutoPilot() override = default;
    
    void update(Vehicle* vehicle, TileGrid* tileGrid, float deltaTime) override;
    void onAssign(Vehicle* vehicle) override;
    void onRelease(Vehicle* vehicle) override;
    
    // Configuration
    void setMaxSpeed(float speed) { m_maxSpeed = speed; m_currentSpeed = speed; }
    float getMaxSpeed() const { return m_maxSpeed; }
    void setLookaheadDistance(float distance) { m_lookaheadDistance = distance; }
    float getLookaheadDistance() const { return m_lookaheadDistance; }
    void setBrakingDeceleration(float decel) { m_brakingDeceleration = decel; }
    float getBrakingDeceleration() const { return m_brakingDeceleration; }
    
private:
    // Curve following state
    struct CurveState {
        glm::ivec3 tilePos{-1, -1, -1};  // Which tile this curve was computed for
        glm::vec2 arcCenter{0.0f};
        float arcRadius = 0.0f;
        float distance;
        int direction;
        float startAngle;
        float startVehicleAngle;
        bool valid = false;
        
        void clear() {
            tilePos = glm::ivec3(-1, -1, -1);
            valid = false;
        }
    };
    
    CurveState m_curveState;
    float m_maxSpeed = 12.0f;
    float m_currentSpeed = 12.0f;  // Current actual speed (for braking)
    float m_brakingDeceleration = 25.0f;  // How fast we brake (units/sec^2)
    float m_lookaheadDistance = 8.0f;  // How far ahead to check for obstacles
    float m_minStoppingDistance = 1.5f;  // Minimum distance to maintain from obstacles
    
    // Helper methods
    float checkForObstaclesAhead(Vehicle* vehicle, float heading) const;
    void updateSpeedForObstacles(Vehicle* vehicle, float heading, float deltaTime);
    bool isCurveTile(CarDirection dir) const;
    glm::vec2 getCornerOffset(CarDirection tileDir, float tileSize, float currentAngle) const;
    void updateOnCurve(Vehicle* vehicle, TileGrid* tileGrid, float deltaTime, const glm::ivec3& gridPos, CarDirection tileDir);
    void updateOnStraight(Vehicle* vehicle, TileGrid* tileGrid, float deltaTime, const glm::ivec3& gridPos, CarDirection tileDir);

    float normalizeAngle(float angle) const;

    int getCurveDirection(CarDirection tileDir, float currentAngle) const;

    float getTargetAngle(CarDirection tileDir, float currentAngle) const;

    int getStartAngle(CarDirection tileDir, float currentAngle) const;
};
