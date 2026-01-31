#include "VehicleConfig.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

const std::string VehicleConfig::s_emptyString;

VehicleConfig& VehicleConfig::getInstance() {
    static VehicleConfig instance;
    return instance;
}

bool VehicleConfig::loadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "VehicleConfig: Failed to open file: " << filepath << std::endl;
        return false;
    }
    
    try {
        json j;
        file >> j;
        
        m_definitions.clear();
        m_idToIndex.clear();
        
        if (!j.contains("vehicleTypes") || !j["vehicleTypes"].is_array()) {
            std::cerr << "VehicleConfig: Invalid format - missing vehicleTypes array" << std::endl;
            return false;
        }
        
        for (const auto& vt : j["vehicleTypes"]) {
            VehicleTypeDefinition def;
            
            // Required fields
            if (!vt.contains("id") || !vt["id"].is_string()) {
                std::cerr << "VehicleConfig: Vehicle type missing 'id' field" << std::endl;
                continue;
            }
            def.id = vt["id"].get<std::string>();
            
            // Optional fields with defaults
            if (vt.contains("displayName") && vt["displayName"].is_string()) {
                def.displayName = vt["displayName"].get<std::string>();
            } else {
                def.displayName = def.id;  // Use ID as display name if not specified
            }
            
            if (vt.contains("maxSpeed") && vt["maxSpeed"].is_number()) {
                def.maxSpeed = vt["maxSpeed"].get<float>();
            }
            
            if (vt.contains("maxSpeedVariance") && vt["maxSpeedVariance"].is_number()) {
                def.maxSpeedVariance = vt["maxSpeedVariance"].get<float>();
            }
            
            if (vt.contains("acceleration") && vt["acceleration"].is_number()) {
                def.acceleration = vt["acceleration"].get<float>();
            }
            
            if (vt.contains("size") && vt["size"].is_array() && vt["size"].size() == 2) {
                def.size.x = vt["size"][0].get<float>();
                def.size.y = vt["size"][1].get<float>();
            }
            
            if (vt.contains("texture") && vt["texture"].is_string()) {
                def.texturePath = vt["texture"].get<std::string>();
            }
            
            if (vt.contains("deltaTexture") && vt["deltaTexture"].is_string()) {
                def.deltaTexturePath = vt["deltaTexture"].get<std::string>();
            }

            if (vt.contains("maxHealth") && vt["maxHealth"].is_number_integer()) {
                def.maxHealth = vt["maxHealth"].get<int>();
            }
            
            // Parse drivability impact (how much surface conditions affect this vehicle)
            if (vt.contains("drivabilityImpact") && vt["drivabilityImpact"].is_number()) {
                def.drivabilityImpact = vt["drivabilityImpact"].get<float>();
            }
            
            // Add to collection
            m_idToIndex[def.id] = static_cast<int>(m_definitions.size());
            m_definitions.push_back(std::move(def));
        }
        
        m_loaded = true;
        std::cout << "VehicleConfig: Loaded " << m_definitions.size() << " vehicle types from " << filepath << std::endl;
        return true;
        
    } catch (const json::exception& e) {
        std::cerr << "VehicleConfig: JSON parse error: " << e.what() << std::endl;
        return false;
    }
}

const VehicleTypeDefinition* VehicleConfig::getDefinition(const std::string& id) const {
    auto it = m_idToIndex.find(id);
    if (it != m_idToIndex.end()) {
        return &m_definitions[it->second];
    }
    return nullptr;
}

const VehicleTypeDefinition* VehicleConfig::getDefinitionByIndex(int index) const {
    if (index >= 0 && index < static_cast<int>(m_definitions.size())) {
        return &m_definitions[index];
    }
    return nullptr;
}

int VehicleConfig::getIndexById(const std::string& id) const {
    auto it = m_idToIndex.find(id);
    if (it != m_idToIndex.end()) {
        return it->second;
    }
    return -1;
}

const std::string& VehicleConfig::getIdByIndex(int index) const {
    if (index >= 0 && index < static_cast<int>(m_definitions.size())) {
        return m_definitions[index].id;
    }
    return s_emptyString;
}
