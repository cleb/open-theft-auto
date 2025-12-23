#include "AutoPilot.hpp"
#include "Vehicle.hpp"
#include "TileGrid.hpp"
#include "Heading.hpp"
#include <cmath>
#include <algorithm>

namespace {
    static glm::vec2 rotate90CW(const glm::vec2& v) { return glm::vec2(v.y, -v.x); }
    static glm::vec2 rotate90CCW(const glm::vec2& v) { return glm::vec2(-v.y, v.x); }

    static float angleOf(const glm::vec2& v) {
        return std::atan2(v.y, v.x);
    }

    static float normalizeAngleSigned(float a) {
        while (a > glm::pi<float>()) a -= glm::two_pi<float>();
        while (a < -glm::pi<float>()) a += glm::two_pi<float>();
        return a;
    }
}

AutoPilot::AutoPilot() = default;

void AutoPilot::onAssign(Vehicle* vehicle) {
    (void)vehicle;
    m_curveState.clear();
}

void AutoPilot::onRelease(Vehicle* vehicle) {
    (void)vehicle;
    m_curveState.clear();
}

bool AutoPilot::isCurveTile(CarDirection dir) const {
    return dir == CarDirection::NorthEast || dir == CarDirection::NorthWest ||
           dir == CarDirection::SouthEast || dir == CarDirection::SouthWest;
}

float AutoPilot::normalizeAngle(float angle) const {
    while (angle < 0.0f) angle += 360.0f;
    while (angle >= 360.0f) angle -= 360.0f;
    return angle;
}

glm::vec2 AutoPilot::getCornerOffset(CarDirection tileDir, float tileSize, float currentAngle) const {
    const float half = tileSize * 0.5f;
    float angle = normalizeAngle(currentAngle);
    switch (tileDir) {
        case CarDirection::NorthEast: return angle < fabs(angle - 90.0f) ? glm::vec2(-half, +half) : glm::vec2(+half, -half);
        case CarDirection::NorthWest:
            return fabs(angle - 90.0f ) < fabs(angle - 180.0f) ? glm::vec2(-half, -half) : glm::vec2(+half, +half);
        case CarDirection::SouthEast: return angle < fabs(angle - 270.0f) ? glm::vec2(-half, -half) : glm::vec2(+half, +half);
        case CarDirection::SouthWest: return fabs(angle - 180.0f ) < fabs(angle - 270.0f) ? glm::vec2(+half, -half) : glm::vec2(-half, +half);
        default: break;
    }
    return glm::vec2(0.0f);
}

int AutoPilot::getCurveDirection(CarDirection tileDir, float currentAngle) const {
    float angle = normalizeAngle(currentAngle);
    switch (tileDir) {
        case CarDirection::NorthEast: return angle < fabs(angle - 90.0f) ? 1 : -1;
        case CarDirection::NorthWest: return fabs(angle - 90.0f ) < fabs(angle - 180.0f) ? 1 : -1;
        case CarDirection::SouthEast: return angle < fabs(angle - 270.0f) ? 270 : 0;
        case CarDirection::SouthWest: return fabs(angle - 180.0f ) < fabs(angle - 270.0f) ? 270 : 180;
        default: break;
    }
    return 0;
}

float AutoPilot::getTargetAngle(CarDirection tileDir, float currentAngle) const {
    float angle = normalizeAngle(currentAngle);
    switch (tileDir) {
        case CarDirection::NorthEast: return angle < fabs(angle - 90.0f) ? 90 : 0;
        case CarDirection::NorthWest: return fabs(angle - 90.0f ) < fabs(angle - 180.0f) ? 180 : 90;
        case CarDirection::SouthEast: return -1;
        case CarDirection::SouthWest: return fabs(angle - 180.0f ) < fabs(angle - 270.0f) ? 1 : -1;
        default: break;
    }
    return 0;
}

void AutoPilot::update(Vehicle* vehicle, TileGrid* tileGrid, float deltaTime) {
    if (!vehicle || !tileGrid) return;
    
    glm::vec3 pos = vehicle->getPosition();
    glm::ivec3 gridPos = tileGrid->worldToGrid(pos);
    
    // Fix Z level - vehicles at ground level should check z=0 tiles
    int actualZ = static_cast<int>(std::floor(pos.z / tileGrid->getTileSize()));
    if (actualZ < 0) actualZ = 0;
    gridPos.z = actualZ;
    
    const Tile* tile = tileGrid->getTile(gridPos);
    float currentHeading = vehicle->getRotation().z;
    
    if (!tile) {
        // Off the grid, keep moving in current direction
        glm::vec2 f2 = Heading::forwardFromHeadingDeg(currentHeading);
        glm::vec3 forward(f2.x, f2.y, 0.0f);
        glm::vec3 newPos = pos + forward * m_maxSpeed * deltaTime;
        newPos.z = pos.z;
        vehicle->setPosition(newPos);
        return;
    }
    
    CarDirection tileDir = tile->getCarDirection();
    
    if (isCurveTile(tileDir)) {
        updateOnCurve(vehicle, tileGrid, deltaTime, gridPos, tileDir);
    } else {
        // Clear curve state when leaving a curve tile
        if (m_curveState.valid) {
            m_curveState.clear();
        }
        updateOnStraight(vehicle, tileGrid, deltaTime, gridPos, tileDir);
    }
}

void AutoPilot::updateOnCurve(Vehicle* vehicle, TileGrid* tileGrid, float deltaTime, const glm::ivec3& gridPos, CarDirection tileDir) {
    glm::vec3 pos = vehicle->getPosition();
    float currentHeading = vehicle->getRotation().z;
    const float tileSize = tileGrid->getTileSize();
    
    // Check if we need to initialize a new curve (entering curve tile or tile changed)
    if (!m_curveState.valid || m_curveState.distance >= 1.0f) {
        // Compute tile center (gridToWorld gives corner, add half tile)
        glm::vec3 tileCorner3 = tileGrid->gridToWorld(gridPos);
        glm::vec2 tileCenter(tileCorner3.x + tileSize * 0.5f, tileCorner3.y + tileSize * 0.5f);
        
        // Get the corner that this diagonal points to
        glm::vec2 cornerOffset = getCornerOffset(tileDir, tileSize, currentHeading);
        int direction = getCurveDirection(tileDir, currentHeading);
        float targetAngleDeg = getTargetAngle(tileDir, currentHeading);
        glm::vec2 cornerPos = tileCenter + cornerOffset;
        
        // Determine if corner is to the left or right of the vehicle's current heading
        glm::vec2 vehiclePos2(pos.x, pos.y);

        // Build the arc: center is at the corner, radius is half tile
        m_curveState.arcCenter = cornerPos;
        m_curveState.arcRadius = tileSize * 0.5f;
        m_curveState.tilePos = gridPos;
        m_curveState.direction = direction;
        m_curveState.distance = direction == 1 ? 0.01f : 0.99f;
        m_curveState.startAngle = currentHeading;
        

        
        m_curveState.valid = true;
    }
    
    // Follow the cached curve
    if (m_curveState.valid) {
        glm::vec2 vehiclePos2(pos.x, pos.y);


        


        // Update vehicle
        float newAngleDegrees = m_curveState.startAngle + 90.0f * m_curveState.distance;
        vehicle->setRotation(glm::vec3(0.0f, 0.0f, newAngleDegrees - 90.0f));
        float newAngle = glm::radians(newAngleDegrees);

        glm::vec3 newPos(cos(newAngle) * m_curveState.arcRadius + m_curveState.arcCenter.x - tileSize / 2, sin(newAngle)* m_curveState.arcRadius + m_curveState.arcCenter.y - tileSize / 2, pos.z);

        m_curveState.distance += 0.01 * m_curveState.direction;

        vehicle->setPosition(newPos);
    }
}

void AutoPilot::updateOnStraight(Vehicle* vehicle, TileGrid* tileGrid, float deltaTime, const glm::ivec3& gridPos, CarDirection tileDir) {
    glm::vec3 pos = vehicle->getPosition();
    float currentHeading = vehicle->getRotation().z;
    
    glm::vec2 fd2 = Heading::forwardFromHeadingDeg(currentHeading);
    glm::vec3 forwardDir(fd2.x, fd2.y, 0.0f);
    
    glm::vec3 tileCenter = tileGrid->gridToWorld(gridPos);
    glm::vec3 toCenter = tileCenter - pos;
    float dotToCenter = 1.0f;
    if (glm::length(glm::vec2(toCenter)) > 1e-4f) {
        dotToCenter = glm::dot(glm::normalize(glm::vec2(toCenter)), glm::vec2(forwardDir));
    }
    
    float targetHeading = calculateTargetHeading(tileDir, currentHeading);
    
    // Look ahead when moving toward tile edge (for straight tiles approaching curves)
    if (dotToCenter < 0.3f) {
        const float lookAheadDist = 2.0f;
        glm::vec3 lookAheadPos = pos + forwardDir * lookAheadDist;
        glm::ivec3 lookAheadGridPos = tileGrid->worldToGrid(lookAheadPos);
        lookAheadGridPos.z = 0;
        
        if (lookAheadGridPos != gridPos) {
            const Tile* lookAheadTile = tileGrid->getTile(lookAheadGridPos);
            if (lookAheadTile) {
                CarDirection lookAheadDir = lookAheadTile->getCarDirection();
                if (lookAheadDir != CarDirection::None && !isCurveTile(lookAheadDir)) {
                    targetHeading = calculateTargetHeading(lookAheadDir, currentHeading);
                }
            }
        }
    }
    
    // Steer toward target heading
    if (tileDir != CarDirection::None) {
        float rotDiff = Heading::shortestAngleDeltaDeg(currentHeading, targetHeading);
        
        if (std::abs(rotDiff) > 1.0f) {
            float baseTurnRate = 600.0f;
            float dynamicMultiplier = 1.0f + std::abs(rotDiff) / 45.0f;
            float turnRate = baseTurnRate * dynamicMultiplier * deltaTime;
            
            if (std::abs(rotDiff) <= turnRate) {
                currentHeading = targetHeading;
            } else if (rotDiff > 0) {
                currentHeading += turnRate;
            } else {
                currentHeading -= turnRate;
            }
            currentHeading = Heading::wrapDegrees360(currentHeading);
            vehicle->setRotation(glm::vec3(0.0f, 0.0f, currentHeading));
        }
    }
    
    // Move forward
    glm::vec2 fwd2 = Heading::forwardFromHeadingDeg(currentHeading);
    glm::vec3 forward(fwd2.x, fwd2.y, 0.0f);
    glm::vec3 newPos = pos + forward * m_maxSpeed * deltaTime;
    newPos.z = pos.z;
    
    if (tileGrid->canOccupy(pos, newPos)) {
        vehicle->setPosition(newPos);
    }
}

float AutoPilot::calculateTargetHeading(CarDirection tileDir, float currentHeading) const {
    // Single direction tiles - always use their direction
    switch (tileDir) {
        case CarDirection::East:  return 0.0f;
        case CarDirection::North: return 90.0f;
        case CarDirection::West:  return 180.0f;
        case CarDirection::South: return 270.0f;
        case CarDirection::NorthEast: return 45.0f;
        case CarDirection::NorthWest: return 135.0f;
        case CarDirection::SouthWest: return 225.0f;
        case CarDirection::SouthEast: return 315.0f;
        default:
            break;
    }
    
    // Bidirectional tiles - pick the direction closest to current heading
    float rot1, rot2;
    switch (tileDir) {
        case CarDirection::SouthNorth:
            rot1 = 90.0f;   // North
            rot2 = 270.0f;  // South
            break;
        case CarDirection::WestEast:
            rot1 = 0.0f;    // East
            rot2 = 180.0f;  // West
            break;
        case CarDirection::NorthEastSouthWest:
            rot1 = 45.0f;   // NorthEast
            rot2 = 225.0f;  // SouthWest
            break;
        case CarDirection::NorthWestSouthEast:
            rot1 = 135.0f;  // NorthWest
            rot2 = 315.0f;  // SouthEast
            break;
        default:
            return currentHeading;  // Unknown direction, keep current
    }
    
    // Calculate angular difference to each option (handling wraparound)
    float diff1 = std::abs(currentHeading - rot1);
    float diff2 = std::abs(currentHeading - rot2);
    if (diff1 > 180.0f) diff1 = 360.0f - diff1;
    if (diff2 > 180.0f) diff2 = 360.0f - diff2;
    
    return (diff1 <= diff2) ? rot1 : rot2;
}
