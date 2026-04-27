#include "CombatPedestrian.hpp"
#include "SpriteAnimation.hpp"
#include "TileGrid.hpp"
#include "Renderer.hpp"
#include <glm/geometric.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <queue>

namespace {

constexpr float kRepathIntervalSeconds = 0.25f;
constexpr float kVehicleRunDownMinSpeed = 0.5f;
constexpr float kPedestrianVehicleBlockRadiusScale = 0.3f;

constexpr std::array<glm::ivec2, 4> kCardinalSteps = {
    glm::ivec2(1, 0),
    glm::ivec2(-1, 0),
    glm::ivec2(0, 1),
    glm::ivec2(0, -1)
};

float manhattanDistance(const glm::ivec3& a, const glm::ivec3& b) {
    return static_cast<float>(std::abs(a.x - b.x) + std::abs(a.y - b.y));
}

}  // namespace

CombatPedestrian::CombatPedestrian() = default;

void CombatPedestrian::spawn(SpriteAnimation* animation, TileGrid* tileGrid,
                              const glm::vec3& position, float headingDeg) {
    m_tileGrid = tileGrid;
    invalidatePath();
    m_pedestrian = std::make_unique<Pedestrian>();
    m_pedestrian->initialize(animation);
    m_pedestrian->setTileGrid(tileGrid);
    m_pedestrian->setSpeed(0.0f);  // We drive movement manually
    m_pedestrian->setActive(true);
    m_pedestrian->setPosition(position);
    m_pedestrian->setRotation(glm::vec3(0.0f, 0.0f, headingDeg));
    m_shootCooldown = 0.2f;
}

void CombatPedestrian::update(float deltaTime, const glm::vec3& targetPos) {
    if (!m_pedestrian || !m_pedestrian->isActive()) return;

    // Pedestrian::update handles animation states (death animation, etc.)
    m_pedestrian->update(deltaTime);

    if (m_pedestrian->isDead()) return;

    const glm::vec3 pos = m_pedestrian->getPosition();
    const glm::vec2 toTarget(targetPos.x - pos.x, targetPos.y - pos.y);
    const float dist = glm::length(toTarget);
    if (dist < 0.001f) return;

    // Chase if too far
    if (dist > m_chaseDistance) {
        moveToward(deltaTime, targetPos, m_chaseDistance);
    } else {
        const glm::vec2 dir = toTarget / dist;
        m_pedestrian->setRotation(glm::vec3(0.0f, 0.0f, Heading::headingDegFromForward(dir)));
    }

    // Shoot
    const glm::vec3 shootPos = m_pedestrian->getPosition();
    const glm::vec2 toShotTarget(targetPos.x - shootPos.x, targetPos.y - shootPos.y);
    const float shotDist = glm::length(toShotTarget);
    if (shotDist < 0.001f) return;
    const glm::vec2 shotDir = toShotTarget / shotDist;
    m_shootCooldown = std::max(0.0f, m_shootCooldown - deltaTime);
    const glm::vec3 shotOrigin(shootPos.x, shootPos.y, shootPos.z + 0.15f);
    if (shotDist <= m_fireDistance && m_shootCooldown <= 0.0f && m_shootCallback &&
        !isLineOfSightBlocked(shotOrigin, targetPos, 0.08f)) {
        m_shootCallback(shotOrigin, shotDir);
        m_shootCooldown = m_shootCooldownTime;
    }
}

bool CombatPedestrian::moveToward(float deltaTime, const glm::vec3& targetPos, float stopDistance) {
    if (!m_pedestrian || !m_pedestrian->isActive() || m_pedestrian->isDead()) {
        return false;
    }

    const glm::vec3 pos = m_pedestrian->getPosition();
    const glm::vec3 adjustedTarget = adjustMovementTarget(pos, targetPos);
    const glm::vec2 toTarget(adjustedTarget.x - pos.x, adjustedTarget.y - pos.y);
    const float distToTarget = glm::length(toTarget);
    if (distToTarget <= stopDistance || distToTarget < 0.001f) {
        return true;
    }

    if (!m_tileGrid) {
        const glm::vec2 dir = toTarget / distToTarget;
        const glm::vec3 nextPos = pos + glm::vec3(dir.x, dir.y, 0.0f) * (m_speed * deltaTime);
        m_pedestrian->setRotation(glm::vec3(0.0f, 0.0f, Heading::headingDegFromForward(dir)));
        if (!isBlockedByVehicle(nextPos)) {
            m_pedestrian->setPosition(nextPos);
        }
        return false;
    }

    m_repathCooldown = std::max(0.0f, m_repathCooldown - deltaTime);

    const glm::ivec3 currentGrid = m_tileGrid->worldToGrid(pos);
    const glm::ivec3 goalGrid = clampGridPosition(m_tileGrid->worldToGrid(adjustedTarget));
    if (!m_tileGrid->isValidPosition(currentGrid)) {
        invalidatePath();
        return false;
    }

    bool needRepath = m_cachedPath.empty() || m_cachedGoal != goalGrid || m_repathCooldown <= 0.0f;
    if (!needRepath && m_cachedPath.size() > 1 && isBlockedByVehicle(gridToMoveWorld(m_cachedPath[1], pos.z))) {
        needRepath = true;
    }
    if (!needRepath && m_cachedPath.front() != currentGrid) {
        auto it = std::find(m_cachedPath.begin(), m_cachedPath.end(), currentGrid);
        if (it != m_cachedPath.end()) {
            m_cachedPath.erase(m_cachedPath.begin(), it);
        } else {
            needRepath = true;
        }
    }

    if (needRepath) {
        m_cachedPath = findPath(pos, adjustedTarget);
        m_cachedGoal = goalGrid;
        m_repathCooldown = kRepathIntervalSeconds;
    }

    const float tileSize = m_tileGrid->getTileSize();
    while (m_cachedPath.size() > 1) {
        const glm::vec3 nextWorld = gridToMoveWorld(m_cachedPath[1], pos.z);
        const glm::vec2 toNext(nextWorld.x - pos.x, nextWorld.y - pos.y);
        if (glm::length(toNext) > tileSize * 0.18f) {
            break;
        }
        m_cachedPath.erase(m_cachedPath.begin());
    }

    glm::vec3 waypoint = adjustedTarget;
    if (m_cachedPath.size() > 1) {
        waypoint = gridToMoveWorld(m_cachedPath[1], pos.z);
    }
    if (isBlockedByVehicle(waypoint) && !isBlockedByVehicle(adjustedTarget)) {
        waypoint = adjustedTarget;
    }

    glm::vec2 toWaypoint(waypoint.x - pos.x, waypoint.y - pos.y);
    if (glm::length(toWaypoint) < 0.001f) {
        toWaypoint = toTarget;
    }

    const float desiredHeading = Heading::headingDegFromForward(glm::normalize(toWaypoint));
    constexpr std::array<float, 9> headingOffsets = {0.0f, 35.0f, -35.0f, 70.0f, -70.0f, 110.0f, -110.0f,
                                                     150.0f, -150.0f};
    const std::array<float, 3> stepScales = {1.0f, 0.55f, 0.25f};

    for (float offset : headingOffsets) {
        const float candidateHeading = Heading::wrapDegrees360(desiredHeading + offset);
        const glm::vec2 candidateDir = Heading::forwardFromHeadingDeg(candidateHeading);
        for (float scale : stepScales) {
            glm::vec3 nextPos = pos + glm::vec3(candidateDir.x, candidateDir.y, 0.0f) * (m_speed * deltaTime * scale);
            nextPos.z = pos.z;
            if (m_tileGrid->canOccupy(pos, nextPos) && !isBlockedByVehicle(nextPos)) {
                m_pedestrian->setRotation(glm::vec3(0.0f, 0.0f, candidateHeading));
                m_pedestrian->setPosition(nextPos);
                return false;
            }
        }
    }

    invalidatePath();
    return false;
}

std::vector<glm::ivec3> CombatPedestrian::findPath(const glm::vec3& startPos, const glm::vec3& targetPos) const {
    std::vector<glm::ivec3> emptyPath;
    if (!m_tileGrid) {
        return emptyPath;
    }

    const glm::ivec3 start = m_tileGrid->worldToGrid(startPos);
    const glm::ivec3 goal = clampGridPosition(m_tileGrid->worldToGrid(targetPos));
    if (!m_tileGrid->isValidPosition(start) || !m_tileGrid->isValidPosition(goal)) {
        return emptyPath;
    }

    const glm::ivec3 gridSize = m_tileGrid->getGridSize();
    const int width = gridSize.x;
    const int height = gridSize.y;
    const int layerSize = width * height;

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
    open.push({manhattanDistance(start, goal), startIdx});

    int bestIdx = startIdx;
    float bestHeuristic = manhattanDistance(start, goal);

    while (!open.empty()) {
        const QueueItem current = open.top();
        open.pop();

        if (current.idx < 0 || current.idx >= layerSize || closed[current.idx]) {
            continue;
        }

        closed[current.idx] = true;
        const glm::ivec3 currentPos = posOf(current.idx);
        const float h = manhattanDistance(currentPos, goal);
        if (h < bestHeuristic) {
            bestHeuristic = h;
            bestIdx = current.idx;
        }
        if (current.idx == goalIdx) {
            bestIdx = goalIdx;
            break;
        }

        for (const glm::ivec2& step : kCardinalSteps) {
            const glm::ivec3 nextPos = currentPos + glm::ivec3(step.x, step.y, 0);
            if (!m_tileGrid->isValidPosition(nextPos)) {
                continue;
            }

            const glm::vec3 fromWorld = gridToMoveWorld(currentPos, startPos.z);
            const glm::vec3 toWorld = gridToMoveWorld(nextPos, startPos.z);
            if (!m_tileGrid->canOccupy(fromWorld, toWorld)) {
                continue;
            }

            const int nextIdx = idxOf(nextPos);
            const float tentative = gScore[current.idx] + 1.0f;
            if (tentative < gScore[nextIdx]) {
                gScore[nextIdx] = tentative;
                parent[nextIdx] = current.idx;
                open.push({tentative + manhattanDistance(nextPos, goal), nextIdx});
            }
        }
    }

    if (!std::isfinite(gScore[bestIdx])) {
        return emptyPath;
    }

    std::vector<glm::ivec3> reversePath;
    int cursor = bestIdx;
    while (cursor >= 0) {
        reversePath.push_back(posOf(cursor));
        cursor = parent[cursor];
    }

    std::reverse(reversePath.begin(), reversePath.end());
    return reversePath;
}

glm::ivec3 CombatPedestrian::clampGridPosition(glm::ivec3 gridPos) const {
    if (!m_tileGrid) {
        return gridPos;
    }
    const glm::ivec3 size = m_tileGrid->getGridSize();
    gridPos.x = std::clamp(gridPos.x, 0, size.x - 1);
    gridPos.y = std::clamp(gridPos.y, 0, size.y - 1);
    gridPos.z = std::clamp(gridPos.z, 0, size.z - 1);
    return gridPos;
}

glm::vec3 CombatPedestrian::gridToMoveWorld(const glm::ivec3& gridPos, float z) const {
    glm::vec3 world = m_tileGrid->gridToWorld(gridPos);
    world.z = z;
    return world;
}

glm::vec3 CombatPedestrian::adjustMovementTarget(const glm::vec3& from, const glm::vec3& target) const {
    if (!m_movementTargetAdjustCallback) {
        return target;
    }
    return m_movementTargetAdjustCallback(from, target, getPedestrianBlockRadius());
}

bool CombatPedestrian::isBlockedByVehicle(const glm::vec3& position) const {
    if (!m_vehicleBlockCheck || !m_pedestrian) {
        return false;
    }

    return m_vehicleBlockCheck(position, getPedestrianBlockRadius());
}

bool CombatPedestrian::isLineOfSightBlocked(const glm::vec3& from, const glm::vec3& target,
                                            float clearanceRadius) const {
    if (!m_lineOfSightBlockCallback) {
        return false;
    }
    return m_lineOfSightBlockCallback(from, target, clearanceRadius);
}

float CombatPedestrian::getPedestrianBlockRadius() const {
    if (!m_pedestrian) {
        return 0.0f;
    }
    const glm::vec2 pedSize = m_pedestrian->getSize();
    return std::max(pedSize.x, pedSize.y) * kPedestrianVehicleBlockRadiusScale;
}

void CombatPedestrian::invalidatePath() {
    m_cachedPath.clear();
    m_cachedGoal = glm::ivec3(-1, -1, -1);
    m_repathCooldown = 0.0f;
}

void CombatPedestrian::render(Renderer* renderer) const {
    if (m_pedestrian && m_pedestrian->isActive()) {
        m_pedestrian->render(renderer);
    }
}

bool CombatPedestrian::checkVehicleCollision(const glm::vec3& vehiclePos, const glm::vec2& vehicleSize,
                                             float vehicleRotation, float vehicleSpeed) {
    if (!m_pedestrian || !m_pedestrian->isActive() || m_pedestrian->isDead()) return false;
    if (std::abs(vehicleSpeed) < kVehicleRunDownMinSpeed) return false;

    const glm::vec3 pedPos = m_pedestrian->getPosition();
    const glm::vec2 pedSize = m_pedestrian->getSize();
    const float pedRadius = std::max(pedSize.x, pedSize.y) * 0.3f;

    const glm::vec2 forward = Heading::forwardFromHeadingDeg(vehicleRotation);
    const glm::vec2 right(forward.y, -forward.x);
    const glm::vec2 toP(pedPos.x - vehiclePos.x, pedPos.y - vehiclePos.y);

    const float localX = glm::dot(toP, right);
    const float localY = glm::dot(toP, forward);
    const float halfWidth = vehicleSize.x * 0.5f;
    const float halfLength = vehicleSize.y * 0.5f;

    if (std::abs(localX) < halfWidth + pedRadius && std::abs(localY) < halfLength + pedRadius) {
        m_pedestrian->kill();
        return true;
    }
    return false;
}

bool CombatPedestrian::checkBulletHit(const glm::vec3& bulletPos, float bulletRadius) {
    if (!m_pedestrian || !m_pedestrian->isActive() || m_pedestrian->isDead()) return false;

    const glm::vec3 pos = m_pedestrian->getPosition();
    const glm::vec2 pedSize = m_pedestrian->getSize();
    const float pedRadius = std::max(pedSize.x, pedSize.y) * 0.4f;

    const float dx = bulletPos.x - pos.x;
    const float dy = bulletPos.y - pos.y;
    const float r = pedRadius + bulletRadius;
    if (dx * dx + dy * dy <= r * r) {
        m_pedestrian->kill();
        return true;
    }
    return false;
}

bool CombatPedestrian::isAlive() const {
    return m_pedestrian && m_pedestrian->isActive() && !m_pedestrian->isDead();
}

bool CombatPedestrian::isDead() const {
    return !m_pedestrian || !m_pedestrian->isActive() || m_pedestrian->isDead();
}

const glm::vec3& CombatPedestrian::getPosition() const {
    static const glm::vec3 zero(0.0f);
    return m_pedestrian ? m_pedestrian->getPosition() : zero;
}
