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

glm::vec2 AutoPilot::getCornerOffset(CarDirection tileDir, float tileSize) const {
    const float half = tileSize * 0.5f;
    switch (tileDir) {
        case CarDirection::NorthEast: return glm::vec2(+half, +half);  // upper-right
        case CarDirection::NorthWest: return glm::vec2(-half, +half);  // upper-left
        case CarDirection::SouthEast: return glm::vec2(+half, -half);  // lower-right
        case CarDirection::SouthWest: return glm::vec2(-half, -half);  // lower-left
        default: break;
    }
    return glm::vec2(0.0f);
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
    if (!m_curveState.valid || m_curveState.tilePos != gridPos) {
        // Compute tile center (gridToWorld gives corner, add half tile)
        glm::vec3 tileCorner3 = tileGrid->gridToWorld(gridPos);
        glm::vec2 tileCenter(tileCorner3.x + tileSize * 0.5f, tileCorner3.y + tileSize * 0.5f);
        
        // Get the corner that this diagonal points to
        glm::vec2 cornerOffset = getCornerOffset(tileDir, tileSize);
        glm::vec2 cornerPos = tileCenter + cornerOffset;
        
        // Determine if corner is to the left or right of the vehicle's current heading
        glm::vec2 vehiclePos2(pos.x, pos.y);
        glm::vec2 forward2 = Heading::forwardFromHeadingDeg(currentHeading);
        glm::vec2 toCorner = cornerPos - vehiclePos2;
        
        // Cross product: positive = corner is to the left, negative = corner is to the right
        float cross = forward2.x * toCorner.y - forward2.y * toCorner.x;
        bool cornerIsLeft = cross > 0.0f;
        
        // Build the arc: center is at the corner, radius is half tile
        m_curveState.arcCenter = cornerPos;
        m_curveState.arcRadius = tileSize * 0.5f;
        m_curveState.tilePos = gridPos;
        
        // Compute start angle from vehicle's current position
        glm::vec2 fromCenter = vehiclePos2 - cornerPos;
        float len = glm::length(fromCenter);
        if (len > 1e-4f) {
            m_curveState.arcStartAngleRad = angleOf(fromCenter);
        } else {
            // Vehicle is at the corner (shouldn't happen), use heading to derive
            m_curveState.arcStartAngleRad = angleOf(-forward2);
        }
        
        // For a left turn: CCW, totalAngle = +90°
        // For a right turn: CW, totalAngle = -90°
        m_curveState.arcTotalAngleRad = cornerIsLeft ? glm::half_pi<float>() : -glm::half_pi<float>();
        m_curveState.valid = true;
    }
    
    // Follow the cached curve
    if (m_curveState.valid) {
        glm::vec2 vehiclePos2(pos.x, pos.y);
        
        // Project vehicle onto the arc circle
        glm::vec2 fromCenter = vehiclePos2 - m_curveState.arcCenter;
        float len = glm::length(fromCenter);
        if (len < 1e-4f) {
            fromCenter = glm::vec2(std::cos(m_curveState.arcStartAngleRad), std::sin(m_curveState.arcStartAngleRad));
            len = 1.0f;
        }
        fromCenter = (fromCenter / len) * m_curveState.arcRadius;
        
        float currentAngle = angleOf(fromCenter);
        
        // Compute how far along the arc we are (t in [0,1])
        float angleFromStart = normalizeAngleSigned(currentAngle - m_curveState.arcStartAngleRad);
        float t = angleFromStart / m_curveState.arcTotalAngleRad;
        t = std::clamp(t, 0.0f, 1.0f);
        
        // Advance along the arc
        float arcLength = std::abs(m_curveState.arcTotalAngleRad) * m_curveState.arcRadius;
        float arcDist = m_maxSpeed * deltaTime;
        float deltaT = arcDist / arcLength;
        float t2 = std::clamp(t + deltaT, 0.0f, 1.0f);
        
        // Compute new angle and position
        float newAngle = m_curveState.arcStartAngleRad + m_curveState.arcTotalAngleRad * t2;
        glm::vec2 newPos2 = m_curveState.arcCenter + glm::vec2(std::cos(newAngle), std::sin(newAngle)) * m_curveState.arcRadius;
        
        // Compute tangent (perpendicular to radial)
        glm::vec2 radial(std::cos(newAngle), std::sin(newAngle));
        glm::vec2 tangent = (m_curveState.arcTotalAngleRad >= 0.0f) ? rotate90CCW(radial) : rotate90CW(radial);
        
        // Update vehicle
        float newHeading = Heading::headingDegFromForward(tangent);
        vehicle->setRotation(glm::vec3(0.0f, 0.0f, newHeading));
        
        glm::vec3 newPos(newPos2.x, newPos2.y, pos.z);
        if (tileGrid->canOccupy(pos, newPos)) {
            vehicle->setPosition(newPos);
        }
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
