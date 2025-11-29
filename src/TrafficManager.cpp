#include "TrafficManager.hpp"
#include "Renderer.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>

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
        float currentRotation = vehicle->getRotation().z;
        
        if (!tile) {
            // Off the grid, keep moving in current direction
            float radians = glm::radians(currentRotation);
            glm::vec3 forward(std::sin(radians), std::cos(radians), 0.0f);
            const float aiSpeed = 12.0f;
            glm::vec3 newPos = pos + forward * aiSpeed * deltaTime;
            newPos.z = pos.z;
            vehicle->setPosition(newPos);
            continue;
        }
        
        CarDirection tileDir = tile->getCarDirection();
        
        // Look ahead to start turning early and find steering target
        float radians = glm::radians(currentRotation);
        glm::vec3 forwardDir(std::sin(radians), std::cos(radians), 0.0f);
        
        // Calculate if we're heading toward tile edge (past center)
        glm::vec3 tileCenter = m_tileGrid->gridToWorld(gridPos);
        glm::vec3 toCenter = tileCenter - pos;
        float dotToCenter = glm::dot(glm::normalize(glm::vec2(toCenter)), glm::vec2(forwardDir));
        
        float targetRotation = calculateTargetRotation(tileDir, currentRotation);
        
        // Check if current tile is a corner - if so, look for exit tile and aim toward it
        bool currentIsCorner = (tileDir == CarDirection::NorthEast ||
                               tileDir == CarDirection::NorthWest ||
                               tileDir == CarDirection::SouthEast ||
                               tileDir == CarDirection::SouthWest);
        
        if (currentIsCorner) {
            // Find the exit tile by looking in the corner's exit direction
            float exitRotation = calculateTargetRotation(tileDir, currentRotation);
            float exitRadians = glm::radians(exitRotation);
            glm::vec3 exitDir(std::sin(exitRadians), std::cos(exitRadians), 0.0f);
            
            // Look for the tile we'll exit into
            glm::vec3 exitLookPos = tileCenter + exitDir * m_tileGrid->getTileSize();
            glm::ivec3 exitGridPos = m_tileGrid->worldToGrid(exitLookPos);
            exitGridPos.z = 0;
            
            const Tile* exitTile = m_tileGrid->getTile(exitGridPos);
            if (exitTile && exitTile->getCarDirection() != CarDirection::None) {
                // Aim toward the center of the exit tile
                glm::vec3 exitTileCenter = m_tileGrid->gridToWorld(exitGridPos);
                glm::vec3 toExit = exitTileCenter - pos;
                targetRotation = glm::degrees(std::atan2(toExit.x, toExit.y));
                if (targetRotation < 0) targetRotation += 360.0f;
            }
        }
        // Look ahead when moving toward tile edge (for straight tiles approaching corners)
        else if (dotToCenter < 0.3f) {
            const float lookAheadDist = 2.0f;
            glm::vec3 lookAheadPos = pos + forwardDir * lookAheadDist;
            glm::ivec3 lookAheadGridPos = m_tileGrid->worldToGrid(lookAheadPos);
            lookAheadGridPos.z = 0;
            
            if (lookAheadGridPos != gridPos) {
                const Tile* lookAheadTile = m_tileGrid->getTile(lookAheadGridPos);
                if (lookAheadTile) {
                    CarDirection lookAheadDir = lookAheadTile->getCarDirection();
                    if (lookAheadDir != CarDirection::None) {
                        bool nextIsCorner = (lookAheadDir == CarDirection::NorthEast ||
                                            lookAheadDir == CarDirection::NorthWest ||
                                            lookAheadDir == CarDirection::SouthEast ||
                                            lookAheadDir == CarDirection::SouthWest);
                        
                        if (nextIsCorner) {
                            // Approaching a corner - aim toward corner center to start the turn
                            glm::vec3 cornerCenter = m_tileGrid->gridToWorld(lookAheadGridPos);
                            glm::vec3 toCorner = cornerCenter - pos;
                            targetRotation = glm::degrees(std::atan2(toCorner.x, toCorner.y));
                            if (targetRotation < 0) targetRotation += 360.0f;
                        } else {
                            // Straight tile ahead - use its direction
                            targetRotation = calculateTargetRotation(lookAheadDir, currentRotation);
                        }
                    }
                }
            }
        }
        
        // If we're on a road with a direction, adjust heading
        float rotDiff = 0.0f;
        if (tileDir != CarDirection::None) {
            // targetRotation already calculated above (may include look-ahead)
            
            // Calculate rotation difference
            rotDiff = targetRotation - currentRotation;
            while (rotDiff > 180.0f) rotDiff -= 360.0f;
            while (rotDiff < -180.0f) rotDiff += 360.0f;
            
            // Only turn if there's a significant difference
            if (std::abs(rotDiff) > 1.0f) {
                // Very fast turn rate with dynamic multiplier for larger angles
                float baseTurnRate = 600.0f;  // Degrees per second
                float dynamicMultiplier = 1.0f + std::abs(rotDiff) / 45.0f;  // Up to 2x for 45° turns
                float turnRate = baseTurnRate * dynamicMultiplier * deltaTime;
                
                if (std::abs(rotDiff) <= turnRate) {
                    currentRotation = targetRotation;
                } else if (rotDiff > 0) {
                    currentRotation += turnRate;
                    if (currentRotation >= 360.0f) currentRotation -= 360.0f;
                } else {
                    currentRotation -= turnRate;
                    if (currentRotation < 0.0f) currentRotation += 360.0f;
                }
                vehicle->setRotation(glm::vec3(0.0f, 0.0f, currentRotation));
            }
        }
        
        // Move forward based on current rotation
        radians = glm::radians(currentRotation);
        glm::vec3 forward(std::sin(radians), std::cos(radians), 0.0f);
        
        // AI vehicles slow down while turning, full speed when straight
        const float maxSpeed = 12.0f;  // Units per second
        const float minSpeed = 3.0f;   // Minimum speed while turning
        float turnFactor = std::min(std::abs(rotDiff) / 30.0f, 1.0f);  // 0 to 1 based on turn angle
        float aiSpeed = maxSpeed - (maxSpeed - minSpeed) * turnFactor;
        
        glm::vec3 newPos = pos + forward * aiSpeed * deltaTime;
        newPos.z = pos.z;  // Keep same Z
        
        // Check if we can move there
        if (m_tileGrid->canOccupy(pos, newPos)) {
            vehicle->setPosition(newPos);
        }
    }
}

float TrafficManager::calculateTargetRotation(CarDirection tileDir, float currentRotation) const {
    // Single direction tiles - always use their direction
    switch (tileDir) {
        case CarDirection::North: return 0.0f;
        case CarDirection::South: return 180.0f;
        case CarDirection::East: return 90.0f;
        case CarDirection::West: return 270.0f;
        case CarDirection::NorthEast: return 45.0f;
        case CarDirection::NorthWest: return 315.0f;
        case CarDirection::SouthEast: return 135.0f;
        case CarDirection::SouthWest: return 225.0f;
        default:
            break;
    }
    
    // Bidirectional tiles - pick the direction closest to current heading
    float rot1, rot2;
    switch (tileDir) {
        case CarDirection::SouthNorth:
            rot1 = 0.0f;    // North
            rot2 = 180.0f;  // South
            break;
        case CarDirection::WestEast:
            rot1 = 90.0f;   // East
            rot2 = 270.0f;  // West
            break;
        case CarDirection::NorthEastSouthWest:
            rot1 = 45.0f;   // NorthEast
            rot2 = 225.0f;  // SouthWest
            break;
        case CarDirection::NorthWestSouthEast:
            rot1 = 315.0f;  // NorthWest
            rot2 = 135.0f;  // SouthEast
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
        case CarDirection::North: return 0.0f;
        case CarDirection::South: return 180.0f;
        case CarDirection::East: return 90.0f;
        case CarDirection::West: return 270.0f;
        case CarDirection::NorthEast: return 45.0f;
        case CarDirection::NorthWest: return 315.0f;
        case CarDirection::SouthEast: return 135.0f;
        case CarDirection::SouthWest: return 225.0f;
        case CarDirection::SouthNorth: return coin(rng) ? 0.0f : 180.0f;
        case CarDirection::WestEast: return coin(rng) ? 90.0f : 270.0f;
        case CarDirection::NorthEastSouthWest: return coin(rng) ? 45.0f : 225.0f;
        case CarDirection::NorthWestSouthEast: return coin(rng) ? 315.0f : 135.0f;
        default: return 0.0f;
    }
}

glm::vec2 TrafficManager::getForwardFromDirection([[maybe_unused]] CarDirection dir, float rotation) {
    // Convert rotation to forward vector
    float radians = glm::radians(rotation);
    return glm::vec2(std::sin(radians), std::cos(radians));
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
