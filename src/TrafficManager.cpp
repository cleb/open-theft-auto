#include "TrafficManager.hpp"
#include "TextureManager.hpp"
#include "Renderer.hpp"
#include "Heading.hpp"
#include "AutoPilot.hpp"
#include "PedestrianManager.hpp"
#include "VehicleConfig.hpp"
#include <iostream>
#include <algorithm>

TrafficManager::TrafficManager()
    : m_tileGrid(nullptr)
    , m_camera(nullptr)
    , m_vehicles(nullptr)
    , m_pedestrianManager(nullptr)
    , m_maxTrafficVehicles(10)
    , m_spawnIntervalMin(1.5f)   // Minimum time between spawns
    , m_spawnIntervalMax(4.0f)   // Maximum time between spawns
    , m_spawnTimer(0.0f)
    , m_nextSpawnInterval(2.0f)  // Initial spawn interval
    , m_viewMargin(8.0f)         // Larger margin to spawn further from view
    , m_enabled(true)
    , m_debugRenderSpawnPoints(false)
    , m_fovRadians(1.57f)        // Default ~90 degree FOV
    , m_aspectRatio(16.0f/9.0f)  // Default aspect ratio
    , m_rng(std::random_device{}()) {
}

void TrafficManager::initialize(TileGrid* tileGrid, Camera* camera,
                                std::vector<std::unique_ptr<Vehicle>>* vehicles) {
    m_tileGrid = tileGrid;
    m_camera = camera;
    m_vehicles = vehicles;
    
    if (m_tileGrid) {
        buildRoadSpawnPoints();
    }
    
    std::cout << "TrafficManager initialized with " << m_roadSpawnPoints.size() 
              << " road spawn points" << std::endl;
}

void TrafficManager::reset() {
    if (m_vehicles) {
        m_vehicles->erase(
            std::remove_if(m_vehicles->begin(), m_vehicles->end(),
                [](const std::unique_ptr<Vehicle>& v) {
                    return v && v->getOwner() == VehicleOwner::Traffic;
                }),
            m_vehicles->end());
    }
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
    
    ViewBounds bounds = ViewBounds::calculate(m_camera, m_fovRadians, m_aspectRatio);
    
    // Despawn vehicles that are out of view
    despawnOutOfViewVehicles(bounds);
    
    // Update traffic vehicles (via their AutoPilot)
    updateTrafficVehicles(deltaTime);
    
    // Try to spawn new vehicles
    m_spawnTimer += deltaTime;
    int trafficCount = 0;
    if (m_vehicles) {
        for (const auto& v : *m_vehicles) {
            if (v && v->getOwner() == VehicleOwner::Traffic) {
                ++trafficCount;
            }
        }
    }
    if (m_spawnTimer >= m_nextSpawnInterval && 
        trafficCount < m_maxTrafficVehicles) {
        spawnVehicle();
        m_spawnTimer = 0.0f;
        // Randomize next spawn interval for irregular traffic
        std::uniform_real_distribution<float> intervalDist(m_spawnIntervalMin, m_spawnIntervalMax);
        m_nextSpawnInterval = intervalDist(m_rng);
    }
}

void TrafficManager::render(Renderer* renderer) {
    // Vehicles are rendered by Scene from the shared list.
    // This method is intentionally empty.
    (void)renderer;
}

void TrafficManager::setProjectionInfo(float fovRadians, float aspectRatio) {
    m_fovRadians = fovRadians;
    m_aspectRatio = aspectRatio;
}

void TrafficManager::spawnVehicle() {
    if (m_roadSpawnPoints.empty()) return;
    
    ViewBounds bounds = ViewBounds::calculate(m_camera, m_fovRadians, m_aspectRatio);
    
    // Collect valid spawn points (outside view, in spawn zone)
    // Use minSpawnOffset of 3.0f for vehicles (slightly larger than default)
    std::vector<const RoadSpawnPoint*> validPoints;
    for (const auto& point : m_roadSpawnPoints) {
        if (bounds.isInSpawnZone(point.worldPos, m_viewMargin, 3.0f) && 
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
    
    // Get the tile to check spawn weights
    const Tile* tile = m_tileGrid->getTile(spawnPoint->gridPos);
    
    // Determine which vehicle type to spawn based on tile spawn weights and config
    const auto& config = VehicleConfig::getInstance();
    const auto& definitions = config.getAllDefinitions();
    
    std::string vehicleTypeId = "sedan";  // Default
    if (tile && !definitions.empty()) {
        // Calculate total weight
        float totalWeight = 0.0f;
        std::vector<std::pair<std::string, float>> typeWeights;
        
        for (const auto& def : definitions) {
            // Skip police vehicles - they are only spawned by PoliceChaseManager
            if (def.id == "police") continue;
            
            float weight = tile->getVehicleSpawnWeight(def.id);
            if (weight > 0.0f) {
                totalWeight += weight;
                typeWeights.push_back({def.id, weight});
            }
        }
        
        if (totalWeight > 0.0f && !typeWeights.empty()) {
            std::uniform_real_distribution<float> weightDist(0.0f, totalWeight);
            float roll = weightDist(m_rng);
            
            float cumulative = 0.0f;
            for (const auto& tw : typeWeights) {
                cumulative += tw.second;
                if (roll < cumulative) {
                    vehicleTypeId = tw.first;
                    break;
                }
            }
        }
    }
    
    // Get the definition for this vehicle type
    const auto* typeDef = config.getDefinition(vehicleTypeId);
    
    // Create the vehicle with the selected type
    auto vehicle = std::make_unique<Vehicle>();
    vehicle->setVehicleType(vehicleTypeId);
    
    // Get vehicle texture based on type
    std::string texturePath = typeDef ? typeDef->texturePath : "textures/car.png";
    auto vehicleTexture = TextureManager::instance().getTextureFromPath(texturePath);
    if (vehicleTexture) {
        vehicle->initialize(vehicleTexture);
    } else {
        vehicle->initialize(texturePath);
    }
    vehicle->setPosition(spawnPoint->worldPos);
    
    // Recalculate rotation for bidirectional roads
    float rotation = getRotationFromDirection(spawnPoint->direction, m_rng);
    vehicle->setRotation(glm::vec3(0.0f, 0.0f, rotation));
    
    // Set sprite size from config
    glm::vec2 spriteSize = typeDef ? typeDef->size : glm::vec2(1.5f, 3.0f);
    vehicle->setSpriteSize(spriteSize);
    vehicle->setTileGrid(m_tileGrid);
    
    // Set carjack callback to spawn a pedestrian when vehicle is taken
    if (m_pedestrianManager) {
        PedestrianManager* pedMgr = m_pedestrianManager;
        vehicle->setCarjackCallback([pedMgr](const glm::vec3& vehiclePos, float vehicleRotation, const glm::vec2& vehicleSize) {
            // Calculate exit position to the left of the vehicle (driver's side)
            glm::vec2 forward = Heading::forwardFromHeadingDeg(vehicleRotation);
            // Left is perpendicular to forward (90° counter-clockwise)
            glm::vec2 left(-forward.y, forward.x);
            
            // Place pedestrian to the left of the vehicle
            float exitOffset = (vehicleSize.x * 0.5f) + 0.8f; // Half vehicle width + margin
            glm::vec3 exitPosition = vehiclePos + glm::vec3(left.x * exitOffset, left.y * exitOffset, 0.0f);
            
            // Pedestrian faces away from the vehicle (towards where they exited)
            float pedestrianRotation = Heading::headingDegFromForward(left);
            
            pedMgr->spawnCarjackedPedestrian(exitPosition, pedestrianRotation);
        });
    }
    
    // Assign an AutoPilot
    vehicle->setPilot(std::make_unique<AutoPilot>());

    vehicle->setOwner(VehicleOwner::Traffic);
    if (m_addVehicleCallback) {
        m_addVehicleCallback(std::move(vehicle));
    } else {
        m_vehicles->push_back(std::move(vehicle));
    }
}

void TrafficManager::despawnOutOfViewVehicles(const ViewBounds& bounds) {
    if (!m_vehicles) return;

    // Extend bounds by margin for despawning
    ViewBounds despawnBounds = bounds.expanded(m_viewMargin * 2.0f);
    
    // Remove traffic vehicles that are too far outside the view
    auto it = std::remove_if(m_vehicles->begin(), m_vehicles->end(),
        [&despawnBounds](const std::unique_ptr<Vehicle>& vehicle) {
            if (!vehicle) return true;
            if (vehicle->getOwner() != VehicleOwner::Traffic) return false;
            return !despawnBounds.contains(vehicle->getPosition());
        });
    m_vehicles->erase(it, m_vehicles->end());
}

void TrafficManager::updateTrafficVehicles(float deltaTime) {
    if (!m_vehicles) return;

    // Each vehicle's update() will call its AutoPilot::update()
    for (auto& vehicle : *m_vehicles) {
        if (vehicle && vehicle->isActive() && vehicle->getOwner() == VehicleOwner::Traffic) {
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
        case CarDirection::OptionalNorthEast: return 45.0f;
        case CarDirection::OptionalNorthWest: return 135.0f;
        case CarDirection::OptionalSouthEast: return 315.0f;
        case CarDirection::OptionalSouthWest: return 225.0f;
        case CarDirection::OptionalNorthEastSouthWest: return coin(rng) ? 45.0f : 225.0f;
        case CarDirection::OptionalNorthWestSouthEast: return coin(rng) ? 135.0f : 315.0f;
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
    
    if (m_vehicles) {
        for (const auto& vehicle : *m_vehicles) {
            if (!vehicle) continue;
            glm::vec3 diff = vehicle->getPosition() - position;
            float distSq = diff.x * diff.x + diff.y * diff.y;
            if (distSq < minDistSq) return true;
        }
    }
    
    return false;
}

void TrafficManager::renderDebugSpawnPoints(Renderer* renderer) {
    if (!renderer || !m_debugRenderSpawnPoints) return;
    
    ViewBounds bounds = ViewBounds::calculate(m_camera, m_fovRadians, m_aspectRatio);
    
    for (const auto& point : m_roadSpawnPoints) {
        glm::vec2 pos(point.worldPos.x, point.worldPos.y);
        glm::vec2 size(0.8f, 0.8f);
        
        // Determine color based on spawn validity
        bool inSpawnZone = bounds.isInSpawnZone(point.worldPos, m_viewMargin, 3.0f);
        bool tooClose = isTooCloseToOtherVehicles(point.worldPos);
        bool inView = bounds.contains(point.worldPos);
        
        // Check if direction points toward center
        glm::vec2 forward = getForwardFromDirection(point.direction, point.rotationDegrees);
        glm::vec2 toCenter(
            (bounds.minX + bounds.maxX) * 0.5f - point.worldPos.x,
            (bounds.minY + bounds.maxY) * 0.5f - point.worldPos.y
        );
        float dot = glm::dot(forward, glm::normalize(toCenter));
        bool directionValid = dot > -0.3f;
        
        glm::vec3 color;
        if (inView) {
            // Red: inside viewport (should NOT spawn here)
            color = glm::vec3(1.0f, 0.0f, 0.0f);
        } else if (!inSpawnZone) {
            // Orange: outside spawn zone (too far)
            color = glm::vec3(1.0f, 0.5f, 0.0f);
        } else if (tooClose) {
            // Yellow: too close to other vehicles
            color = glm::vec3(1.0f, 1.0f, 0.0f);
        } else if (!directionValid) {
            // Magenta: wrong direction (pointing away from view)
            color = glm::vec3(1.0f, 0.0f, 1.0f);
        } else {
            // Green: valid spawn point
            color = glm::vec3(0.0f, 1.0f, 0.0f);
        }
        
        renderer->renderDebugMarker(pos, size, color);
    }
    
    // Also render the view bounds as markers at corners
    glm::vec3 boundsColor(0.0f, 0.5f, 1.0f); // Blue for view bounds
    glm::vec2 cornerSize(0.5f, 0.5f);
    renderer->renderDebugMarker(glm::vec2(bounds.minX, bounds.minY), cornerSize, boundsColor);
    renderer->renderDebugMarker(glm::vec2(bounds.maxX, bounds.minY), cornerSize, boundsColor);
    renderer->renderDebugMarker(glm::vec2(bounds.minX, bounds.maxY), cornerSize, boundsColor);
    renderer->renderDebugMarker(glm::vec2(bounds.maxX, bounds.maxY), cornerSize, boundsColor);
    
    // Render spawn zone bounds (outer bounds)
    glm::vec3 spawnZoneColor(0.0f, 1.0f, 1.0f); // Cyan for spawn zone outer bounds
    float outerMinX = bounds.minX - m_viewMargin;
    float outerMaxX = bounds.maxX + m_viewMargin;
    float outerMinY = bounds.minY - m_viewMargin;
    float outerMaxY = bounds.maxY + m_viewMargin;
    renderer->renderDebugMarker(glm::vec2(outerMinX, outerMinY), cornerSize, spawnZoneColor);
    renderer->renderDebugMarker(glm::vec2(outerMaxX, outerMinY), cornerSize, spawnZoneColor);
    renderer->renderDebugMarker(glm::vec2(outerMinX, outerMaxY), cornerSize, spawnZoneColor);
    renderer->renderDebugMarker(glm::vec2(outerMaxX, outerMaxY), cornerSize, spawnZoneColor);
    
    // Render inner spawn exclusion bounds (where we don't spawn - visible + buffer)
    glm::vec3 innerBoundsColor(1.0f, 0.3f, 0.3f); // Light red for inner exclusion zone
    const float minSpawnOffset = 3.0f;  // Same as used in isInSpawnZone
    float innerMinX = bounds.minX - minSpawnOffset;
    float innerMaxX = bounds.maxX + minSpawnOffset;
    float innerMinY = bounds.minY - minSpawnOffset;
    float innerMaxY = bounds.maxY + minSpawnOffset;
    renderer->renderDebugMarker(glm::vec2(innerMinX, innerMinY), cornerSize, innerBoundsColor);
    renderer->renderDebugMarker(glm::vec2(innerMaxX, innerMinY), cornerSize, innerBoundsColor);
    renderer->renderDebugMarker(glm::vec2(innerMinX, innerMaxY), cornerSize, innerBoundsColor);
    renderer->renderDebugMarker(glm::vec2(innerMaxX, innerMaxY), cornerSize, innerBoundsColor);
}
