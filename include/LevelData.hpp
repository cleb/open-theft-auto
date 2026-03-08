#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include "PickupTypes.hpp"

// Spawn weight for a specific vehicle type on a road tile
struct VehicleSpawnWeight {
    std::string typeId = "sedan";  // Vehicle type ID from VehicleConfig
    float weight = 1.0f;           // Relative weight for spawning (0 = never spawn this type)
};

struct VehicleSpawnDefinition {
    glm::ivec3 gridPosition{0};
    float rotationDegrees = 0.0f;
    std::string texturePath;
    glm::vec2 size = glm::vec2(1.5f, 3.0f);
    std::string vehicleTypeId = "sedan";  // Vehicle type ID from VehicleConfig
};

struct PlayerSpawnDefinition {
    glm::ivec3 gridPosition{0, 0, 0};
    float rotationDegrees = 0.0f;
    bool isSet = false;  // Whether a spawn point has been explicitly set
};

struct PickupSpawnDefinition {
    glm::ivec3 gridPosition{0, 0, 0};
    PickupType type = PickupType::Pistol;
    int ammo = 10;
};

struct PhoneBoothSpawnDefinition {
    glm::ivec3 gridPosition{0, 0, 0};
    std::string id;     // Unique identifier for this phone booth
    std::string jobId;  // Which job this booth offers
};

struct MarkerDefinition {
    glm::ivec3 gridPosition{0, 0, 0};
    std::string name;   // Unique name used to reference this marker from code
};

struct LevelData {
    std::vector<VehicleSpawnDefinition> vehicleSpawns;
    PlayerSpawnDefinition playerSpawn;
    std::vector<PickupSpawnDefinition> pickups;
    std::vector<PhoneBoothSpawnDefinition> phoneBooths;
    std::vector<MarkerDefinition> markers;
};
