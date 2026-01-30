#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

enum class PickupType {
    Pistol
};

inline const char* pickupTypeToString(PickupType type) {
    switch (type) {
        case PickupType::Pistol:
        default:
            return "pistol";
    }
}

inline const char* pickupTypeDisplayName(PickupType type) {
    switch (type) {
        case PickupType::Pistol:
        default:
            return "Pistol";
    }
}

inline std::string pickupTexturePath(PickupType type) {
    switch (type) {
        case PickupType::Pistol:
        default:
            return "assets/textures/pistol.png";
    }
}

inline glm::vec2 pickupDefaultSize(PickupType type) {
    switch (type) {
        case PickupType::Pistol:
        default:
            return glm::vec2(1.0f, 1.0f);
    }
}

inline const std::vector<PickupType>& getAllPickupTypes() {
    static const std::vector<PickupType> types{PickupType::Pistol};
    return types;
}

inline bool pickupTypeFromString(const std::string& value, PickupType& out) {
    if (value == "pistol") {
        out = PickupType::Pistol;
        return true;
    }
    return false;
}
