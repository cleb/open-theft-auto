#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

struct VehicleSpawnDefinition {
    glm::ivec3 gridPosition{0};
    float rotationDegrees = 0.0f;
    std::string texturePath;
    glm::vec2 size = glm::vec2(1.5f, 3.0f);
};

struct LevelData {
    std::vector<VehicleSpawnDefinition> vehicleSpawns;
};
