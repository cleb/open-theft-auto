#include "AutoPilot.hpp"
#include "Vehicle.hpp"
#include "TileGrid.hpp"
#include "Heading.hpp"
#include "Collider.hpp"
#include <cmath>
#include <algorithm>

AutoPilot::AutoPilot() = default;

void AutoPilot::onAssign(Vehicle* vehicle) {
    (void)vehicle;
    m_curveState.clear();
    m_currentSpeed = m_maxSpeed;
}

void AutoPilot::onRelease(Vehicle* vehicle) {
    (void)vehicle;
    m_curveState.clear();
    m_currentSpeed = m_maxSpeed;
}

bool AutoPilot::isCurveTile(CarDirection dir) const {
    return dir == CarDirection::NorthEast || dir == CarDirection::NorthWest ||
           dir == CarDirection::SouthEast || dir == CarDirection::SouthWest;
}

bool AutoPilot::isOptionalTurnTile(CarDirection dir) const {
    return dir == CarDirection::OptionalNorthEast || dir == CarDirection::OptionalNorthWest ||
           dir == CarDirection::OptionalSouthEast || dir == CarDirection::OptionalSouthWest ||
           dir == CarDirection::OptionalNorthEastSouthWest || dir == CarDirection::OptionalNorthWestSouthEast;
}

CarDirection AutoPilot::optionalTurnToCurve(CarDirection dir) const {
    switch (dir) {
        case CarDirection::OptionalNorthEast: return CarDirection::NorthEast;
        case CarDirection::OptionalNorthWest: return CarDirection::NorthWest;
        case CarDirection::OptionalSouthEast: return CarDirection::SouthEast;
        case CarDirection::OptionalSouthWest: return CarDirection::SouthWest;
        case CarDirection::OptionalNorthEastSouthWest: return CarDirection::NorthEastSouthWest;
        case CarDirection::OptionalNorthWestSouthEast: return CarDirection::NorthWestSouthEast;
        default: return dir;
    }
}

CarDirection AutoPilot::optionalTurnToStraight(CarDirection dir, float currentAngle) const {
    // Determine which straight direction the vehicle is currently closest to
    float angle = normalizeAngle(currentAngle);

    // For each optional turn, figure out which two cardinal directions
    // the curve connects and pick the one the vehicle is currently heading.
    auto pickClosest = [&](float a1, CarDirection d1, float a2, CarDirection d2) -> CarDirection {
        float diff1 = std::fabs(Heading::shortestAngleDeltaDeg(angle, a1));
        float diff2 = std::fabs(Heading::shortestAngleDeltaDeg(angle, a2));
        return diff1 < diff2 ? d1 : d2;
    };

    switch (dir) {
        case CarDirection::OptionalNorthEast:
            // Connects North (90) and East (0) – keep whichever the vehicle is heading
            return pickClosest(90.0f, CarDirection::North, 0.0f, CarDirection::East);
        case CarDirection::OptionalNorthWest:
            return pickClosest(90.0f, CarDirection::North, 180.0f, CarDirection::West);
        case CarDirection::OptionalSouthEast:
            return pickClosest(270.0f, CarDirection::South, 0.0f, CarDirection::East);
        case CarDirection::OptionalSouthWest:
            return pickClosest(270.0f, CarDirection::South, 180.0f, CarDirection::West);
        case CarDirection::OptionalNorthEastSouthWest:
            return pickClosest(90.0f, CarDirection::SouthNorth, 0.0f, CarDirection::WestEast);
        case CarDirection::OptionalNorthWestSouthEast:
            return pickClosest(90.0f, CarDirection::SouthNorth, 180.0f, CarDirection::WestEast);
        default:
            return dir;
    }
}

CarDirection AutoPilot::resolveOptionalTurn(CarDirection dir, const glm::ivec3& gridPos, float currentAngle) {
    // If we already resolved this tile, return the cached decision
    if (m_optionalTurnTilePos == gridPos) {
        if (m_optionalTurnIsCurve) {
            return optionalTurnToCurve(dir);
        } else {
            return optionalTurnToStraight(dir, currentAngle);
        }
    }
    
    // New tile – make a random decision: 50% curve, 50% straight
    std::uniform_int_distribution<int> coin(0, 1);
    m_optionalTurnIsCurve = coin(m_rng) == 0;
    m_optionalTurnTilePos = gridPos;
    
    if (m_optionalTurnIsCurve) {
        return optionalTurnToCurve(dir);
    } else {
        return optionalTurnToStraight(dir, currentAngle);
    }
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
        case CarDirection::SouthEast: return angle < fabs(angle - 270.0f) ? -1 : 1;
        case CarDirection::SouthWest: return fabs(angle - 180.0f ) < fabs(angle - 270.0f) ? 1 : -1;
        default: break;
    }
    return 0;
}

int AutoPilot::getStartAngle(CarDirection tileDir, float currentAngle) const {
    float angle = normalizeAngle(currentAngle);
    switch (tileDir) {
        case CarDirection::NorthEast: return angle < fabs(angle - 90.0f) ? 270 : 180;
        case CarDirection::NorthWest: return fabs(angle - 90.0f ) < fabs(angle - 180.0f) ? 0 : 270;
        case CarDirection::SouthEast: return angle < fabs(angle - 270.0f) ? 90 : 180;
        case CarDirection::SouthWest: return fabs(angle - 180.0f ) < fabs(angle - 270.0f) ? 90 : 0;
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
        updateSpeedForObstacles(vehicle, currentHeading, deltaTime);
        
        glm::vec2 f2 = Heading::forwardFromHeadingDeg(currentHeading);
        glm::vec3 forward(f2.x, f2.y, 0.0f);
        glm::vec3 newPos = pos + forward * m_currentSpeed * deltaTime;
        newPos.z = pos.z;
        
        // Check for collision before moving
        const CollisionManager& collisionMgr = vehicle->getCollisionManager();
        if (collisionMgr.hasCallback() && collisionMgr.wouldCollide(vehicle, newPos, currentHeading)) {
            // Apply damage only once when collision first occurs
            if (!vehicle->isInCollision()) {
                vehicle->applyDamage(CollisionDirection::Front);
                vehicle->setInCollision(true);
            }
            vehicle->setSpeed(0.0f);
            m_currentSpeed = 0.0f;
            return;
        }
        
        // Clear collision state when no longer colliding
        vehicle->setInCollision(false);
        vehicle->setPosition(newPos);
        return;
    }
    
    CarDirection tileDir = tile->getCarDirection();
    
    // Resolve optional turn tiles to either curve or straight
    if (isOptionalTurnTile(tileDir)) {
        tileDir = resolveOptionalTurn(tileDir, gridPos, currentHeading);
    } else {
        // Clear the optional turn tracking when on a non-optional tile
        if (m_optionalTurnTilePos != glm::ivec3(-1, -1, -1)) {
            m_optionalTurnTilePos = glm::ivec3(-1, -1, -1);
        }
    }
    
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
    if (!m_curveState.valid) {
        // Compute tile center (gridToWorld gives corner, add half tile)
        glm::vec3 tileCenter3 = tileGrid->gridToWorld(gridPos);
        glm::vec2 tileCenter(tileCenter3.x, tileCenter3.y);
        
        // Get the corner that this diagonal points to
        glm::vec2 cornerOffset = getCornerOffset(tileDir, tileSize, currentHeading);
        int direction = getCurveDirection(tileDir, currentHeading);
        glm::vec2 cornerPos = tileCenter + cornerOffset;
        
        // Determine if corner is to the left or right of the vehicle's current heading
        glm::vec2 vehiclePos2(pos.x, pos.y);

        float startAngle = getStartAngle(tileDir, currentHeading);


        // Build the arc: center is at the corner, radius is half tile
        m_curveState.arcCenter = cornerPos;
        m_curveState.arcRadius = tileSize * 0.5f;
        m_curveState.tilePos = gridPos;
        m_curveState.direction = direction;
        m_curveState.distance = 0.01f;
        m_curveState.startAngle = startAngle;
        m_curveState.startVehicleAngle = currentHeading;
        

        
        m_curveState.valid = true;
    }
    
    // Follow the cached curve
    if (m_curveState.valid) {
        // Calculate arc length for a 90-degree turn
        float arcLength = m_curveState.arcRadius * glm::half_pi<float>();  // quarter circle
        
        updateSpeedForObstacles(vehicle, currentHeading, deltaTime);
        
        // Calculate how much distance we travel this frame
        float distanceTraveled = m_currentSpeed * deltaTime;
        
        // Convert to a fraction of the curve (0 to 1)
        float distanceIncrement = distanceTraveled / arcLength;

        // Update vehicle
        float newDistance = m_curveState.distance + distanceIncrement;
        float newAngleDegrees = m_curveState.startAngle + 90.0f * newDistance * m_curveState.direction;
        float newRotation = m_curveState.startVehicleAngle + 90.0f * newDistance * m_curveState.direction;
        float newAngle = glm::radians(newAngleDegrees);

        glm::vec3 newPos(cos(newAngle) * m_curveState.arcRadius + m_curveState.arcCenter.x, sin(newAngle)* m_curveState.arcRadius + m_curveState.arcCenter.y, pos.z);

        // Check for collision before moving
        const CollisionManager& collisionMgr = vehicle->getCollisionManager();
        if (collisionMgr.hasCallback() && collisionMgr.wouldCollide(vehicle, newPos, newRotation)) {
            // Collision detected - apply damage only once when collision first occurs
            if (!vehicle->isInCollision()) {
                vehicle->applyDamage(CollisionDirection::Front);
                vehicle->setInCollision(true);
            }
            vehicle->setSpeed(0.0f);
            m_currentSpeed = 0.0f;
            return;
        }

        // Clear collision state when no longer colliding
        vehicle->setInCollision(false);
        m_curveState.distance = newDistance;
        vehicle->setRotation(glm::vec3(0.0f, 0.0f, newRotation));
        vehicle->setPosition(newPos);

        if(m_curveState.distance >= 1.0f) {
            vehicle->setRotation(glm::vec3(0.0f, 0.0f, m_curveState.startVehicleAngle + 90.0f * m_curveState.direction));
            glm::vec2 f2 = Heading::forwardFromHeadingDeg(vehicle->getRotation().z);
            glm::vec3 forward(f2.x, f2.y, 0.0f);
            
            // Check collision for the exit position too
            glm::vec3 exitPos = newPos + forward;
            if (collisionMgr.hasCallback() && collisionMgr.wouldCollide(vehicle, exitPos, vehicle->getRotation().z)) {
                if (!vehicle->isInCollision()) {
                    vehicle->applyDamage(CollisionDirection::Front);
                    vehicle->setInCollision(true);
                }
                vehicle->setSpeed(0.0f);
                m_currentSpeed = 0.0f;
                return;
            }
            
            vehicle->setPosition(exitPos);
            m_curveState.clear();
        }
    }
}

void AutoPilot::updateOnStraight(Vehicle* vehicle, TileGrid* tileGrid, float deltaTime, const glm::ivec3& /*gridPos*/, CarDirection /*tileDir*/) {
    glm::vec3 pos = vehicle->getPosition();
    float currentHeading = vehicle->getRotation().z;
    
    updateSpeedForObstacles(vehicle, currentHeading, deltaTime);
    
    // Move forward at current speed
    glm::vec2 fwd2 = Heading::forwardFromHeadingDeg(currentHeading);
    glm::vec3 forward(fwd2.x, fwd2.y, 0.0f);
    glm::vec3 newPos = pos + forward * m_currentSpeed * deltaTime;
    newPos.z = pos.z;
    
    // Check for collision with other vehicles first
    const CollisionManager& collisionMgr = vehicle->getCollisionManager();
    if (collisionMgr.hasCallback() && collisionMgr.wouldCollide(vehicle, newPos, currentHeading)) {
        // Collision detected - apply damage only once when collision first occurs
        if (!vehicle->isInCollision()) {
            vehicle->applyDamage(CollisionDirection::Front);
            vehicle->setInCollision(true);
        }
        vehicle->setSpeed(0.0f);
        m_currentSpeed = 0.0f;
        return;
    }
    
    // Clear collision state when no longer colliding
    vehicle->setInCollision(false);
    
    // Check tile grid collision
    if (tileGrid->canOccupy(pos, newPos)) {
        vehicle->setPosition(newPos);
    }
}

float AutoPilot::checkForObstaclesAhead(Vehicle* vehicle, float heading) const {
    if (!vehicle) return -1.0f;
    
    const CollisionManager& collisionMgr = vehicle->getCollisionManager();
    if (!collisionMgr.hasCallback()) return -1.0f;
    
    glm::vec3 pos = vehicle->getPosition();
    glm::vec2 fwd2 = Heading::forwardFromHeadingDeg(heading);
    glm::vec3 forward(fwd2.x, fwd2.y, 0.0f);
    
    // Check multiple points along the lookahead path
    const int numChecks = 5;
    float stepSize = m_lookaheadDistance / static_cast<float>(numChecks);
    
    for (int i = 1; i <= numChecks; ++i) {
        float checkDistance = stepSize * static_cast<float>(i);
        glm::vec3 checkPos = pos + forward * checkDistance;
        
        if (collisionMgr.wouldCollide(vehicle, checkPos, heading)) {
            // Return the distance to the first collision point
            // Subtract a small amount to account for the vehicle's own size
            return checkDistance - (vehicle->getColliderSize().y * 0.5f);
        }
    }
    
    return -1.0f;  // No obstacle found
}

void AutoPilot::updateSpeedForObstacles(Vehicle* vehicle, float heading, float deltaTime) {
    float obstacleDistance = checkForObstaclesAhead(vehicle, heading);
    
    if (obstacleDistance > 0.0f && obstacleDistance < m_lookaheadDistance) {
        // Calculate speed needed to stop at a safe distance
        // Using v² = 2*a*d, so v = sqrt(2*a*d)
        float availableDistance = std::max(0.0f, obstacleDistance - m_minStoppingDistance);
        float maxSafeSpeed = std::sqrt(2.0f * m_brakingDeceleration * availableDistance);
        
        // Apply braking at a realistic deceleration rate
        if (m_currentSpeed > maxSafeSpeed) {
            m_currentSpeed = std::max(0.0f, m_currentSpeed - m_brakingDeceleration * deltaTime);
        }
    } else {
        // No obstacle ahead, accelerate back to max speed
        if (m_currentSpeed < m_maxSpeed) {
            m_currentSpeed = std::min(m_maxSpeed, m_currentSpeed + m_brakingDeceleration * 0.5f * deltaTime);
        }
    }
}