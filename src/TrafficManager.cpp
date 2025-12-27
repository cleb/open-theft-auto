#include "TrafficManager.hpp"
#include "TextureManager.hpp"
#include "Renderer.hpp"
#include "Heading.hpp"
#include "AutoPilot.hpp"
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
                if (dir != CarDirection::South && dir != CarDirection::North && dir != CarDirection::East && dir != CarDirection::West) continue;
                
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
    
    // Update traffic vehicles (via their AutoPilot)
    updateTrafficVehicles(deltaTime);
    
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

std::unique_ptr<Vehicle> TrafficManager::claimTrafficVehicle(Vehicle* vehicle) {
    if (!vehicle) {
        return nullptr;
    }

    for (auto it = m_trafficVehicles.begin(); it != m_trafficVehicles.end(); ++it) {
        if (it->get() == vehicle) {
            std::unique_ptr<Vehicle> claimed = std::move(*it);
            m_trafficVehicles.erase(it);
            return claimed;
        }
    }

    return nullptr;
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
    
    // Create the vehicle with an AutoPilot
    auto vehicle = std::make_unique<Vehicle>();
    // Get vehicle texture from TextureManager (shared across all traffic vehicles)
    auto carTexture = TextureManager::instance().getVehicleTexture("car");
    if (carTexture) {
        vehicle->initialize(carTexture);
    } else {
        vehicle->initialize("assets/textures/car.png"); // fallback, should not happen
    }
    vehicle->setPosition(spawnPoint->worldPos);
    
    // Recalculate rotation for bidirectional roads
    float rotation = getRotationFromDirection(spawnPoint->direction, m_rng);
    vehicle->setRotation(glm::vec3(0.0f, 0.0f, rotation));
    vehicle->setSpriteSize(glm::vec2(1.5f, 3.0f));
    vehicle->setTileGrid(m_tileGrid);
    
    // Assign an AutoPilot
    vehicle->setPilot(std::make_unique<AutoPilot>());
    
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
    auto it = std::remove_if(m_trafficVehicles.begin(), m_trafficVehicles.end(),
        [&despawnBounds](const std::unique_ptr<Vehicle>& vehicle) {
            if (!vehicle) return true;
            const glm::vec3& pos = vehicle->getPosition();
            return pos.x < despawnBounds.minX || pos.x > despawnBounds.maxX ||
                   pos.y < despawnBounds.minY || pos.y > despawnBounds.maxY;
        });
    m_trafficVehicles.erase(it, m_trafficVehicles.end());
}

void TrafficManager::updateTrafficVehicles(float deltaTime) {
    // Each vehicle's update() will call its AutoPilot::update()
    for (auto& vehicle : m_trafficVehicles) {
        if (vehicle && vehicle->isActive()) {
            vehicle->update(deltaTime);
        }
    }
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
