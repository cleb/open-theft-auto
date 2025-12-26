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
    void setMaxSpeed(float speed) { m_maxSpeed = speed; }
    float getMaxSpeed() const { return m_maxSpeed; }
    
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
    
    // Helper methods
    bool isCurveTile(CarDirection dir) const;
    glm::vec2 getCornerOffset(CarDirection tileDir, float tileSize, float currentAngle) const;
    void updateOnCurve(Vehicle* vehicle, TileGrid* tileGrid, float deltaTime, const glm::ivec3& gridPos, CarDirection tileDir);
    void updateOnStraight(Vehicle* vehicle, TileGrid* tileGrid, float deltaTime, const glm::ivec3& gridPos, CarDirection tileDir);

    float normalizeAngle(float angle) const;

    int getCurveDirection(CarDirection tileDir, float currentAngle) const;

    float getTargetAngle(CarDirection tileDir, float currentAngle) const;

    int getStartAngle(CarDirection tileDir, float currentAngle) const;
};
