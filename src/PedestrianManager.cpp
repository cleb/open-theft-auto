#include "PedestrianManager.hpp"
#include "Renderer.hpp"
#include "Heading.hpp"
#include "Vehicle.hpp"
#include <iostream>
#include <algorithm>

PedestrianManager::PedestrianManager()
    : m_tileGrid(nullptr)
    , m_camera(nullptr)
    , m_maxPedestrians(15)
    , m_spawnIntervalMin(1.0f)
    , m_spawnIntervalMax(3.0f)
    , m_spawnTimer(0.0f)
    , m_nextSpawnInterval(1.5f)
    , m_viewMargin(6.0f)
    , m_enabled(true)
    , m_fovRadians(1.57f)
    , m_aspectRatio(16.0f / 9.0f)
    , m_rng(std::random_device{}()) {
}

void PedestrianManager::initialize(TileGrid* tileGrid, Camera* camera) {
    m_tileGrid = tileGrid;
    m_camera = camera;
    
    // Load the shared animation once for all pedestrians
    m_sharedAnimation = std::make_unique<SpriteAnimation>();
    if (m_sharedAnimation->loadFromFile("assets/textures/pedestrian-animation.json")) {
        m_sharedAnimation->play("walk");
        std::cout << "Loaded shared pedestrian animation" << std::endl;
    } else {
        std::cerr << "Failed to load pedestrian animation" << std::endl;
        m_sharedAnimation.reset();
    }
    
    if (m_tileGrid) {
        buildSidewalkSpawnPoints();
    }
    
    std::cout << "PedestrianManager initialized with " << m_sidewalkSpawnPoints.size() 
              << " sidewalk spawn points" << std::endl;
}

void PedestrianManager::reset() {
    m_pedestrians.clear();
    m_spawnTimer = 0.0f;
    
    if (m_tileGrid) {
        buildSidewalkSpawnPoints();
        std::cout << "PedestrianManager reset: " << m_sidewalkSpawnPoints.size() 
                  << " sidewalk spawn points" << std::endl;
    }
}

void PedestrianManager::setProjectionInfo(float fovRadians, float aspectRatio) {
    m_fovRadians = fovRadians;
    m_aspectRatio = aspectRatio;
}

void PedestrianManager::buildSidewalkSpawnPoints() {
    m_sidewalkSpawnPoints.clear();
    
    if (!m_tileGrid) return;
    
    const glm::ivec3& gridSize = m_tileGrid->getGridSize();
    const float tileSize = m_tileGrid->getTileSize();
    
    // Scan all tiles for sidewalk tiles
    for (int z = 0; z < gridSize.z; ++z) {
        for (int y = 0; y < gridSize.y; ++y) {
            for (int x = 0; x < gridSize.x; ++x) {
                const Tile* tile = m_tileGrid->getTile(x, y, z);
                if (!tile) continue;
                
                SidewalkDirection dir = tile->getSidewalkDirection();
                if (dir == SidewalkDirection::None) continue;
                
                SidewalkSpawnPoint point;
                point.gridPos = glm::ivec3(x, y, z);
                // Calculate world position same way as player spawn
                point.worldPos = glm::vec3(
                    x * tileSize,
                    y * tileSize,
                    z * tileSize + 0.1f  // Slightly above tile surface
                );
                point.direction = dir;
                
                m_sidewalkSpawnPoints.push_back(point);
            }
        }
    }
}

void PedestrianManager::update(float deltaTime) {
    if (!m_enabled || !m_tileGrid || !m_camera) return;
    
    ViewBounds bounds = ViewBounds::calculate(m_camera, m_fovRadians, m_aspectRatio);
    
    // Despawn pedestrians that are out of view
    despawnOutOfViewPedestrians(bounds);
    
    // Update pedestrians
    updatePedestrians(deltaTime);
    
    // Check for vehicle collisions (kills pedestrians)
    checkVehicleCollisions();
    
    // Try to spawn new pedestrians
    m_spawnTimer += deltaTime;
    if (m_spawnTimer >= m_nextSpawnInterval && 
        static_cast<int>(m_pedestrians.size()) < m_maxPedestrians) {
        spawnPedestrian();
        m_spawnTimer = 0.0f;
        // Randomize next spawn interval
        std::uniform_real_distribution<float> intervalDist(m_spawnIntervalMin, m_spawnIntervalMax);
        m_nextSpawnInterval = intervalDist(m_rng);
    }
}

void PedestrianManager::notifyGunshot(const glm::vec3& sourcePosition) {
    if (!m_enabled) {
        return;
    }

    std::uniform_real_distribution<float> durationDist(10.0f, 15.0f);

    for (auto& pedestrian : m_pedestrians) {
        if (!pedestrian || !pedestrian->isActive() || pedestrian->isDead()) {
            continue;
        }
        const float duration = durationDist(m_rng);
        pedestrian->startPanic(sourcePosition, duration);
    }
}

void PedestrianManager::render(Renderer* renderer) {
    if (!renderer) return;
    
    for (auto& pedestrian : m_pedestrians) {
        if (pedestrian && pedestrian->isActive()) {
            pedestrian->render(renderer);
        }
    }
}

float PedestrianManager::getRotationFromDirection(SidewalkDirection dir) {
    std::uniform_int_distribution<int> coin(0, 1);
    
    switch (dir) {
        case SidewalkDirection::NorthSouth:
            return coin(m_rng) ? 90.0f : 270.0f;  // North or South
        case SidewalkDirection::EastWest:
            return coin(m_rng) ? 0.0f : 180.0f;   // East or West
        case SidewalkDirection::NorthEastSouthWest:
            return coin(m_rng) ? 45.0f : 225.0f;  // NE or SW
        case SidewalkDirection::NorthWestSouthEast:
            return coin(m_rng) ? 135.0f : 315.0f; // NW or SE
        default:
            return 0.0f;
    }
}

bool PedestrianManager::isTooCloseToOthers(const glm::vec3& position) const {
    const float minDistance = 2.0f;
    const float minDistSq = minDistance * minDistance;
    
    for (const auto& pedestrian : m_pedestrians) {
        if (!pedestrian) continue;
        glm::vec3 diff = pedestrian->getPosition() - position;
        float distSq = diff.x * diff.x + diff.y * diff.y;
        if (distSq < minDistSq) return true;
    }
    
    return false;
}

void PedestrianManager::spawnPedestrian() {
    if (m_sidewalkSpawnPoints.empty() || !m_sharedAnimation) return;
    
    ViewBounds bounds = ViewBounds::calculate(m_camera, m_fovRadians, m_aspectRatio);
    
    // Collect valid spawn points
    std::vector<const SidewalkSpawnPoint*> validPoints;
    for (const auto& point : m_sidewalkSpawnPoints) {
        if (bounds.isInSpawnZone(point.worldPos, m_viewMargin) && 
            !isTooCloseToOthers(point.worldPos)) {
            validPoints.push_back(&point);
        }
    }
    
    if (validPoints.empty()) return;
    
    // Pick a random valid spawn point
    std::uniform_int_distribution<size_t> dist(0, validPoints.size() - 1);
    const SidewalkSpawnPoint* spawnPoint = validPoints[dist(m_rng)];
    
    // Create the pedestrian
    auto pedestrian = std::make_unique<Pedestrian>();
    pedestrian->initialize(m_sharedAnimation.get());
    
    pedestrian->setPosition(spawnPoint->worldPos);
    pedestrian->setWalkingDirection(spawnPoint->direction);
    
    // Set rotation based on sidewalk direction
    float rotation = getRotationFromDirection(spawnPoint->direction);
    pedestrian->setRotation(glm::vec3(0.0f, 0.0f, rotation));
    
    pedestrian->setTileGrid(m_tileGrid);
    pedestrian->setActive(true);
    
    // Randomize speed slightly
    std::uniform_real_distribution<float> speedDist(1.5f, 2.5f);
    pedestrian->setSpeed(speedDist(m_rng));
    
    m_pedestrians.push_back(std::move(pedestrian));
}

void PedestrianManager::despawnOutOfViewPedestrians(const ViewBounds& bounds) {
    ViewBounds despawnBounds = bounds.expanded(m_viewMargin * 2.0f);
    
    auto it = std::remove_if(m_pedestrians.begin(), m_pedestrians.end(),
        [&despawnBounds](const std::unique_ptr<Pedestrian>& pedestrian) {
            if (!pedestrian) return true;
            return !despawnBounds.contains(pedestrian->getPosition());
        });
    m_pedestrians.erase(it, m_pedestrians.end());
}

void PedestrianManager::updatePedestrians(float deltaTime) {
    for (auto& pedestrian : m_pedestrians) {
        if (pedestrian && pedestrian->isActive()) {
            pedestrian->update(deltaTime);
        }
    }
}

void PedestrianManager::checkVehicleCollisions() {
    if (!m_vehicleCallback) return;
    
    auto vehicles = m_vehicleCallback();
    
    for (auto& pedestrian : m_pedestrians) {
        if (!pedestrian || !pedestrian->isActive() || pedestrian->isDead()) continue;
        
        glm::vec3 pedPos = pedestrian->getPosition();
        glm::vec2 pedSize = pedestrian->getSize();
        
        for (Vehicle* vehicle : vehicles) {
            if (!vehicle || !vehicle->isActive()) continue;
            
            // Simple AABB collision for pedestrian vs vehicle's oriented bounding box
            glm::vec3 vehPos = vehicle->getPosition();
            glm::vec2 vehSize = vehicle->getSpriteSize();
            float vehRotation = vehicle->getRotation().z;
            
            // Get vehicle corners
            glm::vec2 forward = Heading::forwardFromHeadingDeg(vehRotation);
            glm::vec2 right(forward.y, -forward.x);
            
            float halfWidth = vehSize.x * 0.5f;
            float halfLength = vehSize.y * 0.5f;
            
            // Transform pedestrian position to vehicle's local space
            glm::vec2 toP(pedPos.x - vehPos.x, pedPos.y - vehPos.y);
            float localX = glm::dot(toP, right);
            float localY = glm::dot(toP, forward);
            
            // Check if pedestrian center is within vehicle bounds (with some tolerance for pedestrian size)
            float pedRadius = std::max(pedSize.x, pedSize.y) * 0.3f;  // Use smaller collision radius
            
            if (std::abs(localX) < halfWidth + pedRadius && 
                std::abs(localY) < halfLength + pedRadius) {
                // Collision detected - kill the pedestrian
                pedestrian->kill();
                break;  // No need to check other vehicles for this pedestrian
            }
        }
    }
}

void PedestrianManager::spawnCarjackedPedestrian(const glm::vec3& position, float rotation) {
    if (!m_sharedAnimation) {
        std::cerr << "Cannot spawn carjacked pedestrian: no shared animation loaded" << std::endl;
        return;
    }
    
    auto pedestrian = std::make_unique<Pedestrian>();
    pedestrian->initialize(m_sharedAnimation.get());
    pedestrian->setPosition(position);
    pedestrian->setRotation(glm::vec3(0.0f, 0.0f, rotation));
    pedestrian->setTileGrid(m_tileGrid);
    pedestrian->setActive(true);
    pedestrian->setSpeed(2.0f);
    
    // Start the carjack exit animation
    pedestrian->startCarjackExit();
    
    m_pedestrians.push_back(std::move(pedestrian));
    
    std::cout << "Spawned carjacked pedestrian at (" << position.x << ", " << position.y << ")" << std::endl;
}
