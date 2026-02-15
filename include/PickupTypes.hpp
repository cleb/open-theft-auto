#pragma once

#include <glm/glm.hpp>
#include <cstddef>
#include <string>
#include <vector>

enum class PickupType {
    Pistol,
    MachineGun
};

inline constexpr std::size_t pickupTypeCount() {
    return 2;
}

inline constexpr std::size_t pickupTypeToIndex(PickupType type) {
    switch (type) {
        case PickupType::Pistol:
            return 0;
        case PickupType::MachineGun:
            return 1;
        default:
            return 0;
    }
}

inline const char* pickupTypeToString(PickupType type) {
    switch (type) {
        case PickupType::Pistol:
            return "pistol";
        case PickupType::MachineGun:
            return "machine_gun";
        default:
            return "pistol";
    }
}

inline const char* pickupTypeDisplayName(PickupType type) {
    switch (type) {
        case PickupType::Pistol:
            return "Pistol";
        case PickupType::MachineGun:
            return "Machine Gun";
        default:
            return "Pistol";
    }
}

inline std::string pickupTexturePath(PickupType type) {
    switch (type) {
        case PickupType::Pistol:
            return "assets/textures/pistol.png";
        case PickupType::MachineGun:
            return "assets/textures/machine-gun.png";
        default:
            return "assets/textures/pistol.png";
    }
}

inline glm::vec2 pickupDefaultSize(PickupType type) {
    switch (type) {
        case PickupType::Pistol:
            return glm::vec2(1.6f, 1.6f);
        case PickupType::MachineGun:
            return glm::vec2(1.6f, 1.6f);
        default:
            return glm::vec2(1.6f, 1.6f);
    }
}

inline const std::vector<PickupType>& getAllPickupTypes() {
    static const std::vector<PickupType> types{PickupType::Pistol, PickupType::MachineGun};
    return types;
}

inline bool pickupTypeFromString(const std::string& value, PickupType& out) {
    if (value == "pistol") {
        out = PickupType::Pistol;
        return true;
    }
    if (value == "machine_gun") {
        out = PickupType::MachineGun;
        return true;
    }
    return false;
}
