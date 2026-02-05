#include "PolicePilot.hpp"

#include "Collider.hpp"
#include "Heading.hpp"
#include "TileGrid.hpp"
#include "Vehicle.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <queue>

namespace {

constexpr float kRepathIntervalSeconds = 0.30f;
constexpr float kRoadFirstRatio = 1.35f;
constexpr float kHeadingTurnSpeedDegPerSec = 260.0f;

constexpr std::array<glm::ivec2, 4> kCardinalSteps = {
    glm::ivec2(1, 0),
    glm::ivec2(-1, 0),
    glm::ivec2(0, 1),
    glm::ivec2(0, -1)
};

}  // namespace

PolicePilot::PolicePilot() {
    // Police are faster than regular traffic.
    setMaxSpeed(25.0f);
}

void PolicePilot::onAssign(Vehicle* vehicle) {
    AutoPilot::onAssign(vehicle);
    m_cachedPath.clear();
    m_cachedGoal = glm::ivec3(-1, -1, -1);
    m_repathCooldown = 0.0f;
    m_detourTimer = 0.0f;
}

void PolicePilot::onRelease(Vehicle* vehicle) {
    AutoPilot::onRelease(vehicle);
    m_cachedPath.clear();
    m_cachedGoal = glm::ivec3(-1, -1, -1);
    m_repathCooldown = 0.0f;
    m_detourTimer = 0.0f;
}

float PolicePilot::angleDifference(float from, float to) const {
    float diff = normalizeAngle(to) - normalizeAngle(from);
    if (diff > 180.0f) {
        diff -= 360.0f;
    }
    if (diff < -180.0f) {
        diff += 360.0f;
    }
    return diff;
}

glm::ivec3 PolicePilot::worldToDriveGrid(const TileGrid* tileGrid, const glm::vec3& worldPos) const {
    glm::ivec3 gridPos = tileGrid->worldToGrid(worldPos);
    int actualZ = static_cast<int>(std::floor(worldPos.z / tileGrid->getTileSize()));
    if (actualZ < 0) {
        actualZ = 0;
    }
    gridPos.z = actualZ;
    return gridPos;
}

glm::vec3 PolicePilot::gridToDriveWorld(const TileGrid* tileGrid, const glm::ivec3& gridPos, float z) const {
    glm::vec3 world = tileGrid->gridToWorld(gridPos);
    world.z = z;
    return world;
}

bool PolicePilot::isRoadTile(const TileGrid* tileGrid, const glm::ivec3& gridPos) const {
    const Tile* tile = tileGrid->getTile(gridPos);
    return tile && tile->getCarDirection() != CarDirection::None;
}

bool PolicePilot::isMoveAllowedByDirection(CarDirection dir, const glm::ivec2& step) const {
    switch (dir) {
        case CarDirection::East:
            return step == glm::ivec2(1, 0);
        case CarDirection::West:
            return step == glm::ivec2(-1, 0);
        case CarDirection::North:
            return step == glm::ivec2(0, 1);
        case CarDirection::South:
            return step == glm::ivec2(0, -1);
        case CarDirection::WestEast:
            return step == glm::ivec2(1, 0) || step == glm::ivec2(-1, 0);
        case CarDirection::SouthNorth:
            return step == glm::ivec2(0, 1) || step == glm::ivec2(0, -1);
        case CarDirection::NorthEast:
            return step == glm::ivec2(1, 0) || step == glm::ivec2(0, 1);
        case CarDirection::NorthWest:
            return step == glm::ivec2(-1, 0) || step == glm::ivec2(0, 1);
        case CarDirection::SouthEast:
            return step == glm::ivec2(1, 0) || step == glm::ivec2(0, -1);
        case CarDirection::SouthWest:
            return step == glm::ivec2(-1, 0) || step == glm::ivec2(0, -1);
        case CarDirection::NorthEastSouthWest:
        case CarDirection::NorthWestSouthEast:
            return true;
        default:
            return false;
    }
}

float PolicePilot::heuristicCost(const glm::ivec3& from, const glm::ivec3& goal) const {
    return static_cast<float>(std::abs(goal.x - from.x) + std::abs(goal.y - from.y));
}

float PolicePilot::computeMoveCost(const TileGrid* tileGrid, const glm::ivec3& from, const glm::ivec3& to,
                                   const glm::ivec3& goal, RouteMode mode) const {
    const Tile* fromTile = tileGrid->getTile(from);
    const Tile* toTile = tileGrid->getTile(to);
    if (!fromTile || !toTile) {
        return std::numeric_limits<float>::infinity();
    }

    const glm::ivec2 step(to.x - from.x, to.y - from.y);
    const bool fromRoad = fromTile->getCarDirection() != CarDirection::None;
    const bool toRoad = toTile->getCarDirection() != CarDirection::None;
    const bool withLaneFlow = isMoveAllowedByDirection(fromTile->getCarDirection(), step);

    if (mode == RouteMode::StrictLane) {
        if (!fromRoad || !toRoad || !withLaneFlow) {
            return std::numeric_limits<float>::infinity();
        }
        return 1.0f;
    }

    float cost = 1.0f;
    if (fromRoad && toRoad) {
        cost += withLaneFlow ? 0.0f : 3.5f;
    } else if (toRoad) {
        cost += 1.5f;
    } else {
        cost += 5.0f;
    }

    const glm::ivec2 toGoal(goal.x - to.x, goal.y - to.y);
    const int towardGoal = step.x * toGoal.x + step.y * toGoal.y;
    if (towardGoal > 0) {
        cost -= 0.15f;
    }

    return std::max(0.2f, cost);
}

PolicePilot::SearchResult PolicePilot::findPath(const TileGrid* tileGrid, const glm::ivec3& start,
                                                const glm::ivec3& goal, RouteMode mode) const {
    SearchResult result;
    result.totalCost = std::numeric_limits<float>::infinity();

    if (!tileGrid->isValidPosition(start) || !tileGrid->isValidPosition(goal)) {
        return result;
    }

    const glm::ivec3 gridSize = tileGrid->getGridSize();
    const int width = gridSize.x;
    const int height = gridSize.y;
    const int layerSize = width * height;
    const float driveZ = tileGrid->gridToWorld(start).z + tileGrid->getTileSize() + 0.1f;

    auto idxOf = [width](const glm::ivec3& p) {
        return p.y * width + p.x;
    };
    auto posOf = [width, start](int idx) {
        return glm::ivec3(idx % width, idx / width, start.z);
    };

    std::vector<float> gScore(layerSize, std::numeric_limits<float>::infinity());
    std::vector<int> parent(layerSize, -1);
    std::vector<bool> closed(layerSize, false);

    struct QueueItem {
        float f = 0.0f;
        int idx = -1;
        bool operator>(const QueueItem& other) const {
            return f > other.f;
        }
    };

    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> open;

    const int startIdx = idxOf(start);
    const int goalIdx = idxOf(goal);
    gScore[startIdx] = 0.0f;
    open.push({heuristicCost(start, goal), startIdx});

    int bestIdx = startIdx;
    float bestHeuristic = heuristicCost(start, goal);

    while (!open.empty()) {
        QueueItem current = open.top();
        open.pop();

        if (current.idx < 0 || current.idx >= layerSize || closed[current.idx]) {
            continue;
        }

        closed[current.idx] = true;
        const glm::ivec3 currentPos = posOf(current.idx);

        const float h = heuristicCost(currentPos, goal);
        if (h < bestHeuristic) {
            bestHeuristic = h;
            bestIdx = current.idx;
        }

        if (current.idx == goalIdx) {
            bestIdx = goalIdx;
            result.reachedGoal = true;
            break;
        }

        for (const glm::ivec2& step : kCardinalSteps) {
            glm::ivec3 nextPos = currentPos + glm::ivec3(step.x, step.y, 0);
            if (!tileGrid->isValidPosition(nextPos)) {
                continue;
            }

            glm::vec3 fromWorld = gridToDriveWorld(tileGrid, currentPos, driveZ);
            glm::vec3 toWorld = gridToDriveWorld(tileGrid, nextPos, driveZ);
            if (!tileGrid->canOccupy(fromWorld, toWorld)) {
                continue;
            }

            const float moveCost = computeMoveCost(tileGrid, currentPos, nextPos, goal, mode);
            if (!std::isfinite(moveCost)) {
                continue;
            }

            const int nextIdx = idxOf(nextPos);
            const float tentative = gScore[current.idx] + moveCost;
            if (tentative < gScore[nextIdx]) {
                gScore[nextIdx] = tentative;
                parent[nextIdx] = current.idx;
                const float fScore = tentative + heuristicCost(nextPos, goal);
                open.push({fScore, nextIdx});
            }
        }
    }

    if (!std::isfinite(gScore[bestIdx])) {
        return result;
    }

    result.totalCost = gScore[bestIdx];

    std::vector<glm::ivec3> reversePath;
    int cursor = bestIdx;
    while (cursor >= 0) {
        reversePath.push_back(posOf(cursor));
        cursor = parent[cursor];
    }

    std::reverse(reversePath.begin(), reversePath.end());
    result.path = std::move(reversePath);
    return result;
}

bool PolicePilot::wouldCollideAt(const Vehicle* vehicle, const glm::vec3& newPos, float heading) const {
    if (!vehicle) {
        return true;
    }

    const CollisionManager& collisionMgr = vehicle->getCollisionManager();
    if (!collisionMgr.hasCallback()) {
        return false;
    }

    return collisionMgr.wouldCollide(vehicle, newPos, heading);
}

bool PolicePilot::canMoveTo(const TileGrid* tileGrid, const glm::vec3& fromPos, const glm::vec3& toPos) const {
    return tileGrid && tileGrid->canOccupy(fromPos, toPos);
}

float PolicePilot::chooseDetourHeading(const Vehicle* vehicle, const TileGrid* tileGrid, const glm::vec3& pos,
                                       float desiredHeading, float travelDistance, bool& foundDetour) const {
    foundDetour = false;

    const std::array<float, 11> offsets = {0.0f, 20.0f, -20.0f, 40.0f, -40.0f, 65.0f, -65.0f, 90.0f, -90.0f,
                                           135.0f, -135.0f};

    float bestHeading = desiredHeading;
    float bestPenalty = std::numeric_limits<float>::infinity();

    for (float offset : offsets) {
        const float candidateHeading = normalizeAngle(desiredHeading + offset);
        const glm::vec2 fwd2 = Heading::forwardFromHeadingDeg(candidateHeading);
        glm::vec3 candidatePos = pos + glm::vec3(fwd2.x, fwd2.y, 0.0f) * travelDistance;
        candidatePos.z = pos.z;

        if (!canMoveTo(tileGrid, pos, candidatePos) || wouldCollideAt(vehicle, candidatePos, candidateHeading)) {
            continue;
        }

        const glm::ivec3 candidateGrid = worldToDriveGrid(tileGrid, candidatePos);
        float penalty = std::abs(offset);
        if (!isRoadTile(tileGrid, candidateGrid)) {
            penalty += 35.0f;
        }

        if (penalty < bestPenalty) {
            bestPenalty = penalty;
            bestHeading = candidateHeading;
            foundDetour = true;
        }
    }

    return bestHeading;
}

void PolicePilot::update(Vehicle* vehicle, TileGrid* tileGrid, float deltaTime) {
    if (!vehicle || !tileGrid || !m_playerPositionCallback) {
        return;
    }

    glm::vec3 pos = vehicle->getPosition();
    const glm::vec3 targetPos = m_playerPositionCallback();

    const glm::ivec3 currentGrid = worldToDriveGrid(tileGrid, pos);
    glm::ivec3 goalGrid = worldToDriveGrid(tileGrid, targetPos);
    goalGrid.z = currentGrid.z;

    if (!tileGrid->isValidPosition(currentGrid)) {
        return;
    }

    if (!tileGrid->isValidPosition(goalGrid)) {
        glm::ivec3 clamped = goalGrid;
        const glm::ivec3 size = tileGrid->getGridSize();
        clamped.x = std::clamp(clamped.x, 0, size.x - 1);
        clamped.y = std::clamp(clamped.y, 0, size.y - 1);
        clamped.z = std::clamp(clamped.z, 0, size.z - 1);
        goalGrid = clamped;
    }

    m_repathCooldown = std::max(0.0f, m_repathCooldown - deltaTime);
    m_detourTimer = std::max(0.0f, m_detourTimer - deltaTime);

    bool needRepath = m_cachedPath.empty() || m_repathCooldown <= 0.0f || m_cachedGoal != goalGrid;

    if (!needRepath && !m_cachedPath.empty() && m_cachedPath.front() != currentGrid) {
        auto it = std::find(m_cachedPath.begin(), m_cachedPath.end(), currentGrid);
        if (it != m_cachedPath.end()) {
            m_cachedPath.erase(m_cachedPath.begin(), it);
        } else {
            needRepath = true;
        }
    }

    if (needRepath) {
        const SearchResult strict = findPath(tileGrid, currentGrid, goalGrid, RouteMode::StrictLane);
        const SearchResult flexible = findPath(tileGrid, currentGrid, goalGrid, RouteMode::Flexible);

        bool useFlexible = false;
        if (!strict.reachedGoal && flexible.reachedGoal) {
            useFlexible = true;
        } else if (strict.reachedGoal && flexible.reachedGoal && strict.totalCost > flexible.totalCost * kRoadFirstRatio) {
            useFlexible = true;
        } else if (!strict.reachedGoal && !strict.path.empty() && !flexible.path.empty() &&
                   strict.totalCost > flexible.totalCost * kRoadFirstRatio) {
            useFlexible = true;
        }

        m_cachedPath = useFlexible ? flexible.path : strict.path;
        if (m_cachedPath.empty()) {
            m_cachedPath.push_back(currentGrid);
        }
        if (m_cachedPath.front() != currentGrid) {
            m_cachedPath.insert(m_cachedPath.begin(), currentGrid);
        }

        m_cachedGoal = goalGrid;
        m_repathCooldown = kRepathIntervalSeconds;
    }

    const float tileSize = tileGrid->getTileSize();
    while (m_cachedPath.size() > 1) {
        const glm::vec3 nextWorld = gridToDriveWorld(tileGrid, m_cachedPath[1], pos.z);
        const glm::vec2 toNext(nextWorld.x - pos.x, nextWorld.y - pos.y);
        if (glm::length(toNext) > tileSize * 0.22f) {
            break;
        }
        m_cachedPath.erase(m_cachedPath.begin());
    }

    glm::vec3 waypoint = targetPos;
    if (m_cachedPath.size() > 1) {
        waypoint = gridToDriveWorld(tileGrid, m_cachedPath[1], pos.z);
    }

    glm::vec2 toWaypoint(waypoint.x - pos.x, waypoint.y - pos.y);
    if (glm::length(toWaypoint) < 0.01f) {
        return;
    }

    const float desiredHeading = Heading::headingDegFromForward(glm::normalize(toWaypoint));
    const float baseSpeed = getMaxSpeed();
    const float travelDistance = std::max(0.6f, baseSpeed * deltaTime);

    bool foundDetour = false;
    float movementHeading = desiredHeading;

    const glm::vec2 desiredFwd = Heading::forwardFromHeadingDeg(desiredHeading);
    glm::vec3 desiredProbe = pos + glm::vec3(desiredFwd.x, desiredFwd.y, 0.0f) * travelDistance;
    desiredProbe.z = pos.z;

    if (!canMoveTo(tileGrid, pos, desiredProbe) || wouldCollideAt(vehicle, desiredProbe, desiredHeading)) {
        movementHeading = chooseDetourHeading(vehicle, tileGrid, pos, desiredHeading, travelDistance, foundDetour);
        if (!foundDetour) {
            if (!vehicle->isInCollision()) {
                vehicle->applyDamage(CollisionDirection::Front);
            }
            vehicle->setInCollision(true);
            return;
        }
        m_detourTimer = 0.45f;
    }

    const float currentHeading = vehicle->getRotation().z;
    const float turnDiff = angleDifference(currentHeading, movementHeading);
    const float maxTurn = kHeadingTurnSpeedDegPerSec * deltaTime;
    const float appliedTurn = std::clamp(turnDiff, -maxTurn, maxTurn);
    const float newHeading = normalizeAngle(currentHeading + appliedTurn);
    vehicle->setRotation(glm::vec3(0.0f, 0.0f, newHeading));

    float speed = baseSpeed;
    const float headingErrorToWaypoint = std::abs(angleDifference(newHeading, desiredHeading));
    if (headingErrorToWaypoint > 70.0f) {
        speed *= 0.55f;
    } else if (headingErrorToWaypoint > 35.0f) {
        speed *= 0.75f;
    }
    if (m_detourTimer > 0.0f || foundDetour) {
        speed *= 0.85f;
    }

    const glm::vec2 fwd2 = Heading::forwardFromHeadingDeg(newHeading);
    const glm::vec3 forward(fwd2.x, fwd2.y, 0.0f);
    const std::array<float, 3> stepScales = {1.0f, 0.6f, 0.3f};

    for (float scale : stepScales) {
        glm::vec3 candidatePos = pos + forward * (speed * deltaTime * scale);
        candidatePos.z = pos.z;

        if (!canMoveTo(tileGrid, pos, candidatePos)) {
            continue;
        }
        if (wouldCollideAt(vehicle, candidatePos, newHeading)) {
            continue;
        }

        vehicle->setPosition(candidatePos);
        vehicle->setInCollision(false);
        return;
    }

    if (!vehicle->isInCollision()) {
        vehicle->applyDamage(CollisionDirection::Front);
    }
    vehicle->setInCollision(true);
}
