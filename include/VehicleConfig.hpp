#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>

// Data structure for a single vehicle type definition
struct VehicleTypeDefinition {
    std::string id;              // Unique identifier (e.g., "sedan", "pickup")
    std::string displayName;     // Human-readable name for UI
    float maxSpeed = 24.0f;      // Base max speed off-road
    float maxSpeedVariance = 12.0f;  // Additional speed on road
    float acceleration = 12.0f;
    glm::vec2 size = glm::vec2(1.5f, 3.0f);
    std::string texturePath;
    std::string deltaTexturePath;
    
    // How much the vehicle is affected by low-drivability surfaces
    // 0.0 = completely immune to surface conditions (always full speed)
    // 1.0 = fully affected (speed = baseSpeed * drivability)
    // Values between allow partial resistance (e.g., 0.5 = half the slowdown effect)
    float drivabilityImpact = 1.0f;
};

// Singleton class to manage vehicle type definitions
class VehicleConfig {
public:
    static VehicleConfig& getInstance();
    
    // Load vehicle definitions from JSON file
    bool loadFromFile(const std::string& filepath);
    
    // Get vehicle type definition by ID
    const VehicleTypeDefinition* getDefinition(const std::string& id) const;
    
    // Get vehicle type definition by index
    const VehicleTypeDefinition* getDefinitionByIndex(int index) const;
    
    // Get all vehicle type definitions
    const std::vector<VehicleTypeDefinition>& getAllDefinitions() const { return m_definitions; }
    
    // Get index of a vehicle type by ID (-1 if not found)
    int getIndexById(const std::string& id) const;
    
    // Get ID by index (empty string if invalid)
    const std::string& getIdByIndex(int index) const;
    
    // Get the number of defined vehicle types
    int getTypeCount() const { return static_cast<int>(m_definitions.size()); }
    
    // Check if config has been loaded
    bool isLoaded() const { return m_loaded; }

private:
    VehicleConfig() = default;
    ~VehicleConfig() = default;
    VehicleConfig(const VehicleConfig&) = delete;
    VehicleConfig& operator=(const VehicleConfig&) = delete;
    
    std::vector<VehicleTypeDefinition> m_definitions;
    std::unordered_map<std::string, int> m_idToIndex;
    bool m_loaded = false;
    static const std::string s_emptyString;
};
