#include "PolicePilot.hpp"
#include "Vehicle.hpp"
#include "TileGrid.hpp"
#include "Heading.hpp"
#include "Collider.hpp"
#include <cmath>
#include <algorithm>

PolicePilot::PolicePilot() {
    // Police are faster than regular traffic
    setMaxSpeed(25.0f);
}

void PolicePilot::onAssign(Vehicle* vehicle) {
    AutoPilot::onAssign(vehicle);
}

void PolicePilot::onRelease(Vehicle* vehicle) {
    AutoPilot::onRelease(vehicle);
}

float PolicePilot::angleDifference(float from, float to) const {
    float diff = normalizeAngle(to) - normalizeAngle(from);
    if (diff > 180.0f) diff -= 360.0f;
    if (diff < -180.0f) diff += 360.0f;
    return diff;
}

std::vector<float> PolicePilot::getHeadingsForRoadDirection(CarDirection roadDir) const {
    switch (roadDir) {
        case CarDirection::East:
            return {0.0f};
        case CarDirection::West:
            return {180.0f};
        case CarDirection::North:
            return {90.0f};
        case CarDirection::South:
            return {270.0f};
        case CarDirection::WestEast:
            return {0.0f, 180.0f};
        case CarDirection::SouthNorth:
            return {90.0f, 270.0f};
        case CarDirection::NorthEast:
            return {0.0f, 90.0f};
        case CarDirection::NorthWest:
            return {90.0f, 180.0f};
        case CarDirection::SouthEast:
            return {270.0f, 0.0f};
        case CarDirection::SouthWest:
            return {180.0f, 270.0f};
        default:
            return {};
    }
}

bool PolicePilot::isHeadingTowardsTarget(float heading, const glm::vec3& fromPos, const glm::vec3& targetPos) const {
    glm::vec2 toTarget(targetPos.x - fromPos.x, targetPos.y - fromPos.y);
    if (glm::length(toTarget) < 0.001f) return true;
    
    float targetHeading = Heading::headingDegFromForward(glm::normalize(toTarget));
    float diff = std::abs(angleDifference(heading, targetHeading));
    
    return diff < 90.0f;
}

float PolicePilot::getBestHeadingForTarget(float currentHeading, const glm::vec3& pos, const glm::vec3& targetPos, CarDirection roadDir) const {
    std::vector<float> headings = getHeadingsForRoadDirection(roadDir);
    if (headings.empty()) return currentHeading;
    
    glm::vec2 toTarget(targetPos.x - pos.x, targetPos.y - pos.y);
    if (glm::length(toTarget) < 0.001f) return currentHeading;
    
    float targetHeading = Heading::headingDegFromForward(glm::normalize(toTarget));
    
    float bestHeading = headings[0];
    float bestDiff = std::abs(angleDifference(headings[0], targetHeading));
    
    for (size_t i = 1; i < headings.size(); ++i) {
        float diff = std::abs(angleDifference(headings[i], targetHeading));
        if (diff < bestDiff) {
            bestDiff = diff;
            bestHeading = headings[i];
        }
    }
    
    return bestHeading;
}

bool PolicePilot::shouldOverrideHeading(Vehicle* vehicle, TileGrid* tileGrid, float& newHeading) const {
    if (!m_playerPositionCallback || !vehicle || !tileGrid) return false;
    
    glm::vec3 pos = vehicle->getPosition();
    glm::vec3 targetPos = m_playerPositionCallback();
    float currentHeading = vehicle->getRotation().z;
    
    // Check if we're already heading towards the player
    if (isHeadingTowardsTarget(currentHeading, pos, targetPos)) {
        return false;  // Already going the right way
    }
    
    // We're heading away from the player - look for a lane change opportunity
    glm::ivec3 gridPos = tileGrid->worldToGrid(pos);
    gridPos.z = 0;
    
    // Calculate which direction we need to look for an adjacent lane
    // (perpendicular to current heading)
    glm::vec2 forward = Heading::forwardFromHeadingDeg(currentHeading);
    glm::vec2 right(-forward.y, forward.x);  // perpendicular
    
    // Check adjacent tiles for a road going the opposite direction
    glm::ivec3 checkOffsets[] = {
        glm::ivec3(static_cast<int>(right.x), static_cast<int>(right.y), 0),
        glm::ivec3(static_cast<int>(-right.x), static_cast<int>(-right.y), 0)
    };
    
    for (const auto& offset : checkOffsets) {
        glm::ivec3 adjacentGridPos = gridPos + offset;
        const Tile* adjacentTile = tileGrid->getTile(adjacentGridPos);
        if (!adjacentTile) continue;
        
        CarDirection adjacentDir = adjacentTile->getCarDirection();
        if (adjacentDir == CarDirection::None) continue;
        
        // Check if this adjacent road has a direction that goes towards the player
        std::vector<float> adjacentHeadings = getHeadingsForRoadDirection(adjacentDir);
        for (float adjHeading : adjacentHeadings) {
            if (isHeadingTowardsTarget(adjHeading, pos, targetPos)) {
                // Found a good lane! Check if we can actually move there
                glm::vec3 adjacentWorldPos = tileGrid->gridToWorld(adjacentGridPos);
                adjacentWorldPos.z = pos.z;
                
                if (tileGrid->canOccupy(pos, adjacentWorldPos)) {
                    // We can move there - return the new heading
                    newHeading = adjHeading;
                    m_pendingLaneChange = true;
                    m_laneChangeTargetPos = adjacentWorldPos;
                    return true;
                }
            }
        }
    }
    
    return false;
}

bool PolicePilot::trySteerAroundObstacle(Vehicle* vehicle, TileGrid* tileGrid, float deltaTime) {
    if (!vehicle || !tileGrid) return false;
    
    glm::vec3 pos = vehicle->getPosition();
    float currentHeading = vehicle->getRotation().z;
    const CollisionManager& collisionMgr = vehicle->getCollisionManager();
    
    if (!collisionMgr.hasCallback()) return false;
    
    // Check if there's an obstacle directly ahead
    glm::vec2 fwd2 = Heading::forwardFromHeadingDeg(currentHeading);
    glm::vec3 forward(fwd2.x, fwd2.y, 0.0f);
    float checkDist = 4.0f;
    glm::vec3 aheadPos = pos + forward * checkDist;
    
    if (!collisionMgr.wouldCollide(vehicle, aheadPos, currentHeading)) {
        return false;  // No obstacle ahead
    }
    
    // There's an obstacle - try lateral movement to get around it
    // Keep same heading, just shift left or right
    glm::vec2 right2 = Heading::forwardFromHeadingDeg(normalizeAngle(currentHeading - 90.0f));
    glm::vec3 right(right2.x, right2.y, 0.0f);
    
    float speed = getMaxSpeed() * 0.6f;
    float lateralSpeed = 3.0f;  // How fast to shift sideways
    
    // Try shifting right and left
    float lateralOffsets[] = {1.0f, -1.0f};
    
    for (float lateralDir : lateralOffsets) {
        glm::vec3 testPos = pos + forward * speed * deltaTime + right * lateralDir * lateralSpeed * deltaTime;
        testPos.z = pos.z;
        
        // Check if clear and still on drivable surface
        if (!collisionMgr.wouldCollide(vehicle, testPos, currentHeading) &&
            tileGrid->canOccupy(pos, testPos)) {
            vehicle->setPosition(testPos);
            return true;
        }
    }
    
    // Can't go around - just move forward slowly if possible
    glm::vec3 slowPos = pos + forward * speed * 0.3f * deltaTime;
    slowPos.z = pos.z;
    if (!collisionMgr.wouldCollide(vehicle, slowPos, currentHeading) &&
        tileGrid->canOccupy(pos, slowPos)) {
        vehicle->setPosition(slowPos);
        return true;
    }
    
    return false;  // Let AutoPilot handle (will brake)
}

void PolicePilot::update(Vehicle* vehicle, TileGrid* tileGrid, float deltaTime) {
    if (!vehicle || !tileGrid) return;
    
    // Check if we should switch lanes to head towards player
    float newHeading;
    if (shouldOverrideHeading(vehicle, tileGrid, newHeading)) {
        // Execute lane change
        if (m_pendingLaneChange) {
            glm::vec3 pos = vehicle->getPosition();
            
            // Move towards the lane change target
            glm::vec3 toTarget = m_laneChangeTargetPos - pos;
            float dist = glm::length(glm::vec2(toTarget.x, toTarget.y));
            
            if (dist > 0.5f) {
                // Still moving to new lane
                float moveSpeed = getMaxSpeed() * 0.7f;
                glm::vec3 moveDir = glm::normalize(toTarget);
                glm::vec3 newPos = pos + moveDir * moveSpeed * deltaTime;
                newPos.z = pos.z;
                
                if (tileGrid->canOccupy(pos, newPos)) {
                    vehicle->setPosition(newPos);
                    // Gradually turn to new heading
                    float currentHeading = vehicle->getRotation().z;
                    float diff = angleDifference(currentHeading, newHeading);
                    float turnSpeed = 180.0f;
                    float maxTurn = turnSpeed * deltaTime;
                    if (std::abs(diff) > maxTurn) {
                        newHeading = normalizeAngle(currentHeading + (diff > 0 ? maxTurn : -maxTurn));
                    }
                    vehicle->setRotation(glm::vec3(0.0f, 0.0f, newHeading));
                    return;
                }
            }
            
            // Lane change complete
            m_pendingLaneChange = false;
            vehicle->setRotation(glm::vec3(0.0f, 0.0f, newHeading));
        }
    }
    
    // Try to drive around obstacles
    if (trySteerAroundObstacle(vehicle, tileGrid, deltaTime)) {
        return;
    }
    
    // Normal AutoPilot road following
    AutoPilot::update(vehicle, tileGrid, deltaTime);
}
