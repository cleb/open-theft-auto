#include "TrafficManager.hpp"
#include "Renderer.hpp"
#include "Heading.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>

namespace {
    struct CurveArc {
        glm::vec2 center;
        float radius = 0.0f;
        float startAngleRad = 0.0f;
        float endAngleRad = 0.0f;
        float totalAngleRad = 0.0f;
        bool valid = false;
    };

    static glm::vec2 rotate90CW(const glm::vec2& v) { return glm::vec2(v.y, -v.x); }
    static glm::vec2 rotate90CCW(const glm::vec2& v) { return glm::vec2(-v.y, v.x); }

    static bool isCurveTile(CarDirection d) {
        return d == CarDirection::NorthEast || d == CarDirection::NorthWest ||
               d == CarDirection::SouthEast || d == CarDirection::SouthWest;
    }

    // Map a curve tile to its entry->exit cardinal directions.
    // We interpret these diagonal carDirections as quarter-turn curves:
    //  - NorthEast: enter from North, exit to East
    //  - SouthEast: enter from South, exit to East
    //  - NorthWest: enter from North, exit to West
    //  - SouthWest: enter from South, exit to West
    static bool curveEntryExit(CarDirection tileDir, CarDirection& entry, CarDirection& exit) {
        switch (tileDir) {
            case CarDirection::NorthEast: entry = CarDirection::North; exit = CarDirection::East; return true;
            case CarDirection::SouthEast: entry = CarDirection::South; exit = CarDirection::East; return true;
            case CarDirection::NorthWest: entry = CarDirection::North; exit = CarDirection::West; return true;
            case CarDirection::SouthWest: entry = CarDirection::South; exit = CarDirection::West; return true;
            default: break;
        }
        return false;
    }

    static glm::vec2 dirToForward(CarDirection d) {
        switch (d) {
            case CarDirection::North: return glm::vec2(0.0f, 1.0f);
            case CarDirection::South: return glm::vec2(0.0f, -1.0f);
            case CarDirection::East:  return glm::vec2(1.0f, 0.0f);
            case CarDirection::West:  return glm::vec2(-1.0f, 0.0f);
            default: break;
        }
        return glm::vec2(0.0f, 1.0f);
    }

    static float angleOf(const glm::vec2& v) {
        return std::atan2(v.y, v.x);
    }

    static float normalizeAngleSigned(float a) {
        while (a > glm::pi<float>()) a -= glm::two_pi<float>();
        while (a < -glm::pi<float>()) a += glm::two_pi<float>();
        return a;
    }

    static float lerpAngleRad(float a, float b, float t) {
        float diff = normalizeAngleSigned(b - a);
        return a + diff * t;
    }

    static CurveArc buildCurveArc(const glm::vec2& tileCenter, float tileSize, CarDirection tileDir) {
        CurveArc arc;
        CarDirection entry, exit;
        if (!curveEntryExit(tileDir, entry, exit)) return arc;

        // Use a quarter-circle with radius = tileSize/2. The center is in the quadrant
        // given by (exit + entry) relative to tile center.
        const float r = tileSize * 0.5f;
        glm::vec2 entryF = dirToForward(entry);
        glm::vec2 exitF = dirToForward(exit);

        // Determine the arc center offset from tile center. For a NE curve (N->E),
        // center sits at (+r,+r) etc.
        glm::vec2 centerOffset((exitF.x + entryF.x) * r, (exitF.y + entryF.y) * r);
        arc.center = tileCenter + centerOffset;
        arc.radius = r;

        // Start point is at the center of the entry edge; end point at center of exit edge.
        glm::vec2 startPoint = tileCenter + entryF * r;
        glm::vec2 endPoint = tileCenter + exitF * r;

        arc.startAngleRad = angleOf(startPoint - arc.center);
        arc.endAngleRad = angleOf(endPoint - arc.center);
        arc.totalAngleRad = normalizeAngleSigned(arc.endAngleRad - arc.startAngleRad);

        // We always want a quarter turn; if numerical wrap yields the long way, flip.
        if (std::abs(arc.totalAngleRad) > glm::half_pi<float>() + 0.001f) {
            // force shortest direction
            arc.totalAngleRad = (arc.totalAngleRad > 0.0f) ? (arc.totalAngleRad - glm::two_pi<float>())
                                                            : (arc.totalAngleRad + glm::two_pi<float>());
        }

        arc.valid = true;
        return arc;
    }

    // Project a point onto the arc and advance along it by arcDistance.
    static bool followCurve(const CurveArc& arc, const glm::vec2& currentPos, float arcDistance,
                            glm::vec2& outPos, glm::vec2& outTangent) {
        if (!arc.valid || arc.radius <= 0.0f) return false;

        glm::vec2 fromCenter = currentPos - arc.center;
        float len = glm::length(fromCenter);
        if (len < 1e-4f) {
            // Fallback to start angle.
            fromCenter = glm::vec2(std::cos(arc.startAngleRad), std::sin(arc.startAngleRad)) * arc.radius;
        } else {
            fromCenter = (fromCenter / len) * arc.radius;
        }

        float currentAngle = angleOf(fromCenter);
        // Clamp to arc range by projecting onto [0,1] along the arc.
        float denom = (std::abs(arc.totalAngleRad) < 1e-5f) ? 1.0f : arc.totalAngleRad;
        float t = normalizeAngleSigned(currentAngle - arc.startAngleRad) / denom;
        t = std::clamp(t, 0.0f, 1.0f);

        float deltaT = arcDistance / (std::abs(arc.totalAngleRad) * arc.radius);
        if (!std::isfinite(deltaT)) deltaT = 0.0f;
        float t2 = std::clamp(t + deltaT, 0.0f, 1.0f);

        float angle2 = lerpAngleRad(arc.startAngleRad, arc.startAngleRad + arc.totalAngleRad, t2);
        outPos = arc.center + glm::vec2(std::cos(angle2), std::sin(angle2)) * arc.radius;

        // Tangent direction: derivative of position w.r.t angle.
        // For position = center + R*(cos(a), sin(a)):
        //   d/da = R*(-sin(a), cos(a)) which is a +90° CCW rotation of radial.
        // If totalAngleRad is negative (clockwise travel), tangent must be flipped.
        glm::vec2 radial(std::cos(angle2), std::sin(angle2));
        outTangent = (arc.totalAngleRad >= 0.0f) ? rotate90CCW(radial) : rotate90CW(radial);
        float tLen = glm::length(outTangent);
        if (tLen > 1e-4f) outTangent /= tLen;
        return true;
    }
}

TrafficManager::TrafficManager()
    : m_tileGrid(nullptr)
    , m_camera(nullptr)
    , m_playerVehicles(nullptr)
    , m_maxTrafficVehicles(10)
    , m_spawnInterval(0.5f)  // Spawn more frequently for testing
    , m_spawnTimer(0.0f)
    , m_viewMargin(5.0f)
    , m_enabled(true)
    , m_rng(std::random_device{}()) {
}

void TrafficManager::initialize(TileGrid* tileGrid, Camera* camera,
                                const std::vector<std::unique_ptr<Vehicle>>* playerVehicles) {
    m_tileGrid = tileGrid;
    m_camera = camera;
    m_playerVehicles = playerVehicles;
    
    if (m_tileGrid) {
        buildRoadSpawnPoints();
    }
    
    std::cout << "TrafficManager initialized with " << m_roadSpawnPoints.size() 
              << " road spawn points" << std::endl;
}

void TrafficManager::reset() {
    m_trafficVehicles.clear();
    m_spawnTimer = 0.0f;
    
    if (m_tileGrid) {
        buildRoadSpawnPoints();
    }
}

void TrafficManager::buildRoadSpawnPoints() {
    m_roadSpawnPoints.clear();
    
    if (!m_tileGrid) return;
    
    const glm::ivec3& gridSize = m_tileGrid->getGridSize();
    const float tileSize = m_tileGrid->getTileSize();
    
    // Scan all tiles for road tiles with traffic directions
    for (int z = 0; z < gridSize.z; ++z) {
        for (int y = 0; y < gridSize.y; ++y) {
            for (int x = 0; x < gridSize.x; ++x) {
                const Tile* tile = m_tileGrid->getTile(x, y, z);
                if (!tile) continue;
                
                CarDirection dir = tile->getCarDirection();
                if (dir == CarDirection::None) continue;
                
                RoadSpawnPoint point;
                point.gridPos = glm::ivec3(x, y, z);
                point.worldPos = glm::vec3(
                    x * tileSize,
                    y * tileSize,
                    z * tileSize + 0.1f  // Slightly above ground
                );
                point.direction = dir;
                point.rotationDegrees = getRotationFromDirection(dir, m_rng);
                
                m_roadSpawnPoints.push_back(point);
            }
        }
    }
}

void TrafficManager::update(float deltaTime) {
    if (!m_enabled || !m_tileGrid || !m_camera) return;
    
    ViewBounds bounds = calculateViewBounds();
    
    // Despawn vehicles that are out of view
    despawnOutOfViewVehicles(bounds);
    
    // Update AI vehicles
    updateAIVehicles(deltaTime);
    
    // Try to spawn new vehicles
    m_spawnTimer += deltaTime;
    if (m_spawnTimer >= m_spawnInterval && 
        static_cast<int>(m_trafficVehicles.size()) < m_maxTrafficVehicles) {
        spawnVehicle();
        m_spawnTimer = 0.0f;
    }
}

void TrafficManager::render(Renderer* renderer) {
    if (!renderer) return;
    
    for (auto& vehicle : m_trafficVehicles) {
        if (vehicle && vehicle->isActive()) {
            vehicle->render(renderer);
        }
    }
}

TrafficManager::ViewBounds TrafficManager::calculateViewBounds() const {
    ViewBounds bounds{0, 0, 0, 0};
    
    if (!m_camera) return bounds;
    
    // Get camera target (center of view)
    const glm::vec3& target = m_camera->getTarget();
    const glm::vec3& cameraPos = m_camera->getPosition();
    
    // Calculate view dimensions based on camera height and field of view
    // The camera is positioned above looking down, so we estimate visible area
    float cameraHeight = cameraPos.z - target.z;
    
    // Estimate visible area (approximately 20x20 units at default camera height)
    // This is a rough approximation - adjust based on actual projection
    float viewRadius = cameraHeight * 0.8f;  // Rough estimate of visible radius
    
    bounds.minX = target.x - viewRadius;
    bounds.maxX = target.x + viewRadius;
    bounds.minY = target.y - viewRadius;
    bounds.maxY = target.y + viewRadius;
    
    return bounds;
}

bool TrafficManager::isInView(const glm::vec3& position, const ViewBounds& bounds) const {
    return position.x >= bounds.minX && position.x <= bounds.maxX &&
           position.y >= bounds.minY && position.y <= bounds.maxY;
}

bool TrafficManager::isInSpawnZone(const glm::vec3& position, const ViewBounds& bounds) const {
    // Spawn zone is the area just outside the view
    float outerMinX = bounds.minX - m_viewMargin;
    float outerMaxX = bounds.maxX + m_viewMargin;
    float outerMinY = bounds.minY - m_viewMargin;
    float outerMaxY = bounds.maxY + m_viewMargin;
    
    // Must be in outer zone but NOT in view
    bool inOuterZone = position.x >= outerMinX && position.x <= outerMaxX &&
                       position.y >= outerMinY && position.y <= outerMaxY;
    bool inView = isInView(position, bounds);
    
    return inOuterZone && !inView;
}

void TrafficManager::spawnVehicle() {
    if (m_roadSpawnPoints.empty()) return;
    
    ViewBounds bounds = calculateViewBounds();
    
    // Collect valid spawn points (outside view, in spawn zone)
    std::vector<const RoadSpawnPoint*> validPoints;
    for (const auto& point : m_roadSpawnPoints) {
        if (isInSpawnZone(point.worldPos, bounds) && 
            !isTooCloseToOtherVehicles(point.worldPos)) {
            // Only spawn on edges pointing into the view
            glm::vec2 forward = getForwardFromDirection(point.direction, point.rotationDegrees);
            glm::vec2 toCenter(
                (bounds.minX + bounds.maxX) * 0.5f - point.worldPos.x,
                (bounds.minY + bounds.maxY) * 0.5f - point.worldPos.y
            );
            
            // Check if the traffic direction generally points toward the center
            float dot = glm::dot(forward, glm::normalize(toCenter));
            if (dot > -0.3f) {  // Allow some sideways movement too
                validPoints.push_back(&point);
            }
        }
    }
    
    if (validPoints.empty()) return;
    
    // Pick a random valid spawn point
    std::uniform_int_distribution<size_t> dist(0, validPoints.size() - 1);
    const RoadSpawnPoint* spawnPoint = validPoints[dist(m_rng)];
    
    // Create the vehicle
    auto vehicle = std::make_unique<Vehicle>();
    vehicle->initialize("assets/textures/car.png");
    vehicle->setPosition(spawnPoint->worldPos);
    
    // Recalculate rotation for bidirectional roads
    float rotation = getRotationFromDirection(spawnPoint->direction, m_rng);
    vehicle->setRotation(glm::vec3(0.0f, 0.0f, rotation));
    vehicle->setSpriteSize(glm::vec2(1.5f, 3.0f));
    vehicle->setTileGrid(m_tileGrid);
    vehicle->setPlayerControlled(false);  // AI controlled
    
    m_trafficVehicles.push_back(std::move(vehicle));
}

void TrafficManager::despawnOutOfViewVehicles(const ViewBounds& bounds) {
    // Extend bounds by margin for despawning
    ViewBounds despawnBounds;
    despawnBounds.minX = bounds.minX - m_viewMargin * 2.0f;
    despawnBounds.maxX = bounds.maxX + m_viewMargin * 2.0f;
    despawnBounds.minY = bounds.minY - m_viewMargin * 2.0f;
    despawnBounds.maxY = bounds.maxY + m_viewMargin * 2.0f;
    
    // Remove vehicles that are too far outside the view
    m_trafficVehicles.erase(
        std::remove_if(m_trafficVehicles.begin(), m_trafficVehicles.end(),
            [this, &despawnBounds](const std::unique_ptr<Vehicle>& vehicle) {
                if (!vehicle) return true;
                const glm::vec3& pos = vehicle->getPosition();
                return pos.x < despawnBounds.minX || pos.x > despawnBounds.maxX ||
                       pos.y < despawnBounds.minY || pos.y > despawnBounds.maxY;
            }),
        m_trafficVehicles.end()
    );
}

void TrafficManager::updateAIVehicles(float deltaTime) {
    if (!m_tileGrid) return;
    
    for (auto& vehicle : m_trafficVehicles) {
        if (!vehicle || !vehicle->isActive()) continue;
        
        // Get current position and check road tile
        glm::vec3 pos = vehicle->getPosition();
        glm::ivec3 gridPos = m_tileGrid->worldToGrid(pos);
        
        // Fix Z level - vehicles at ground level should check z=0 tiles
        int actualZ = static_cast<int>(std::floor(pos.z / m_tileGrid->getTileSize()));
        if (actualZ < 0) actualZ = 0;
        gridPos.z = actualZ;
        
        const Tile* tile = m_tileGrid->getTile(gridPos);
    // Heading convention: 0°=+X (East), CCW positive.
    float currentHeading = vehicle->getRotation().z;
        
        if (!tile) {
            // Off the grid, keep moving in current direction
            glm::vec2 f2 = Heading::forwardFromHeadingDeg(currentHeading);
            glm::vec3 forward(f2.x, f2.y, 0.0f);
            const float aiSpeed = 12.0f;
            glm::vec3 newPos = pos + forward * aiSpeed * deltaTime;
            newPos.z = pos.z;
            vehicle->setPosition(newPos);
            continue;
        }
        
        CarDirection tileDir = tile->getCarDirection();

        // Deterministic curve following on diagonal tiles.
        // For curves we compute a quarter-circle arc inside the tile and advance along it.
        const float tileSize = m_tileGrid->getTileSize();
        const float maxSpeed = 12.0f;

        if (isCurveTile(tileDir)) {
            glm::vec3 tileCenter3 = m_tileGrid->gridToWorld(gridPos);
            glm::vec2 tileCenter(tileCenter3.x, tileCenter3.y);

            CurveArc arc = buildCurveArc(tileCenter, tileSize, tileDir);
            glm::vec2 current2(pos.x, pos.y);
            glm::vec2 new2, tangent;

            const float arcDist = maxSpeed * deltaTime;
            if (followCurve(arc, current2, arcDist, new2, tangent)) {
                vehicle->setRotation(glm::vec3(0.0f, 0.0f, Heading::headingDegFromForward(tangent)));

                glm::vec3 newPos(new2.x, new2.y, pos.z);
                if (m_tileGrid->canOccupy(pos, newPos)) {
                    vehicle->setPosition(newPos);
                }
                continue;
            }
            // If for some reason arc following failed, fall through to the old straight behavior.
        }

        // Straight (and bidirectional) behavior: steer toward tile direction with gentle look-ahead.
    glm::vec2 fd2 = Heading::forwardFromHeadingDeg(currentHeading);
    glm::vec3 forwardDir(fd2.x, fd2.y, 0.0f);

        glm::vec3 tileCenter = m_tileGrid->gridToWorld(gridPos);
        glm::vec3 toCenter = tileCenter - pos;
        float dotToCenter = 1.0f;
        if (glm::length(glm::vec2(toCenter)) > 1e-4f) {
            dotToCenter = glm::dot(glm::normalize(glm::vec2(toCenter)), glm::vec2(forwardDir));
        }

    float targetHeading = calculateTargetRotation(tileDir, currentHeading);

        // Look ahead when moving toward tile edge (for straight tiles approaching curves)
        if (dotToCenter < 0.3f) {
            const float lookAheadDist = 2.0f;
            glm::vec3 lookAheadPos = pos + forwardDir * lookAheadDist;
            glm::ivec3 lookAheadGridPos = m_tileGrid->worldToGrid(lookAheadPos);
            lookAheadGridPos.z = 0;

            if (lookAheadGridPos != gridPos) {
                const Tile* lookAheadTile = m_tileGrid->getTile(lookAheadGridPos);
                if (lookAheadTile) {
                    CarDirection lookAheadDir = lookAheadTile->getCarDirection();
                    if (lookAheadDir != CarDirection::None && !isCurveTile(lookAheadDir)) {
                        targetHeading = calculateTargetRotation(lookAheadDir, currentHeading);
                    }
                }
            }
        }

        float rotDiff = 0.0f;
        if (tileDir != CarDirection::None) {
            rotDiff = Heading::shortestAngleDeltaDeg(currentHeading, targetHeading);

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

        glm::vec2 fwd2 = Heading::forwardFromHeadingDeg(currentHeading);
        glm::vec3 forward(fwd2.x, fwd2.y, 0.0f);
        glm::vec3 newPos = pos + forward * maxSpeed * deltaTime;
        newPos.z = pos.z;

        if (m_tileGrid->canOccupy(pos, newPos)) {
            vehicle->setPosition(newPos);
        }
    }
}

float TrafficManager::calculateTargetRotation(CarDirection tileDir, float currentRotation) const {
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
            return currentRotation;  // Unknown direction, keep current
    }
    
    // Calculate angular difference to each option (handling wraparound)
    float diff1 = std::abs(currentRotation - rot1);
    float diff2 = std::abs(currentRotation - rot2);
    if (diff1 > 180.0f) diff1 = 360.0f - diff1;
    if (diff2 > 180.0f) diff2 = 360.0f - diff2;
    
    return (diff1 <= diff2) ? rot1 : rot2;
}

float TrafficManager::getRotationFromDirection(CarDirection dir, std::mt19937& rng) {
    std::uniform_int_distribution<int> coin(0, 1);
    
    switch (dir) {
        case CarDirection::East:  return 0.0f;
        case CarDirection::North: return 90.0f;
        case CarDirection::West:  return 180.0f;
        case CarDirection::South: return 270.0f;
        case CarDirection::NorthEast: return 45.0f;
        case CarDirection::NorthWest: return 135.0f;
        case CarDirection::SouthEast: return 315.0f;
        case CarDirection::SouthWest: return 225.0f;
        case CarDirection::SouthNorth: return coin(rng) ? 90.0f : 270.0f;
        case CarDirection::WestEast: return coin(rng) ? 0.0f : 180.0f;
        case CarDirection::NorthEastSouthWest: return coin(rng) ? 45.0f : 225.0f;
        case CarDirection::NorthWestSouthEast: return coin(rng) ? 135.0f : 315.0f;
        default: return 0.0f;
    }
}

glm::vec2 TrafficManager::getForwardFromDirection([[maybe_unused]] CarDirection dir, float rotation) {
    // Convert heading degrees to forward vector (standardized convention)
    (void)dir;
    return Heading::forwardFromHeadingDeg(rotation);
}

bool TrafficManager::isTooCloseToOtherVehicles(const glm::vec3& position) const {
    const float minDistance = 6.0f;  // Minimum distance between vehicles
    const float minDistSq = minDistance * minDistance;
    
    // Check against traffic vehicles
    for (const auto& vehicle : m_trafficVehicles) {
        if (!vehicle) continue;
        glm::vec3 diff = vehicle->getPosition() - position;
        float distSq = diff.x * diff.x + diff.y * diff.y;
        if (distSq < minDistSq) return true;
    }
    
    // Check against player vehicles
    if (m_playerVehicles) {
        for (const auto& vehicle : *m_playerVehicles) {
            if (!vehicle) continue;
            glm::vec3 diff = vehicle->getPosition() - position;
            float distSq = diff.x * diff.x + diff.y * diff.y;
            if (distSq < minDistSq) return true;
        }
    }
    
    return false;
}
