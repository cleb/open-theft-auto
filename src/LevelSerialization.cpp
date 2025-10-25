#include "LevelSerialization.hpp"

#include "LevelData.hpp"
#include "TileGrid.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace {
std::string trimCopy(const std::string& text) {
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return text.substr(begin, end - begin);
}

std::string toLowerCopy(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

bool parseFloat(const std::string& text, float& out) {
    try {
        size_t processed = 0;
        float value = std::stof(text, &processed);
        if (processed != trimCopy(text).size()) {
            return false;
        }
        out = value;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace

namespace LevelSerialization {

bool loadLevel(const std::string& filePath, TileGrid& grid, LevelData& data) {
    if (!grid.loadFromFile(filePath)) {
        return false;
    }

    data.vehicleSpawns.clear();

    std::ifstream input(filePath);
    if (!input.is_open()) {
        std::cerr << "Failed to reopen level file for vehicle parsing: " << filePath << std::endl;
        return false;
    }

    const auto& aliases = grid.getTextureAliases();
    auto resolveTexture = [&](const std::string& identifier) -> std::string {
        if (identifier.empty()) {
            return std::string();
        }
        auto aliasIt = aliases.find(identifier);
        if (aliasIt != aliases.end()) {
            return aliasIt->second;
        }
        return identifier;
    };

    std::string rawLine;
    int lineNumber = 0;
    while (std::getline(input, rawLine)) {
        ++lineNumber;
        std::string line = rawLine;
        const auto commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }
        line = trimCopy(line);
        if (line.empty()) {
            continue;
        }

        std::istringstream iss(line);
        std::string command;
        iss >> command;
        if (toLowerCopy(command) != "vehicle") {
            continue;
        }

        int x = 0;
        int y = 0;
        int z = 0;
        if (!(iss >> x >> y >> z)) {
            std::cerr << "LevelSerialization::loadLevel(" << filePath << ":" << lineNumber
                      << ") expected coordinates after 'vehicle'" << std::endl;
            continue;
        }

        VehicleSpawnDefinition spawn;
        spawn.gridPosition = glm::ivec3(x, y, z);

        std::string token;
        bool parseOk = true;
        while (iss >> token) {
            const auto eqPos = token.find('=');
            if (eqPos == std::string::npos) {
                std::cerr << "LevelSerialization::loadLevel(" << filePath << ":" << lineNumber
                          << ") expected key=value pair but found '" << token << "'" << std::endl;
                parseOk = false;
                continue;
            }

            const std::string key = toLowerCopy(trimCopy(token.substr(0, eqPos)));
            const std::string value = trimCopy(token.substr(eqPos + 1));

            if (key == "rotation" || key == "angle" || key == "yaw") {
                float rotation = 0.0f;
                if (!parseFloat(value, rotation)) {
                    std::cerr << "LevelSerialization::loadLevel(" << filePath << ":" << lineNumber
                              << ") invalid rotation value: " << value << std::endl;
                    parseOk = false;
                    continue;
                }
                spawn.rotationDegrees = rotation;
                continue;
            }

            if (key == "texture" || key == "tex") {
                spawn.texturePath = resolveTexture(value);
                continue;
            }

            if (key == "size" || key == "dimensions") {
                const std::string trimmed = trimCopy(value);
                auto separator = trimmed.find('x');
                if (separator == std::string::npos) {
                    separator = trimmed.find(',');
                }
                if (separator == std::string::npos) {
                    std::cerr << "LevelSerialization::loadLevel(" << filePath << ":" << lineNumber
                              << ") invalid size format: " << value << std::endl;
                    parseOk = false;
                    continue;
                }
                const std::string first = trimCopy(trimmed.substr(0, separator));
                const std::string second = trimCopy(trimmed.substr(separator + 1));
                float width = 0.0f;
                float length = 0.0f;
                if (!parseFloat(first, width) || !parseFloat(second, length)) {
                    std::cerr << "LevelSerialization::loadLevel(" << filePath << ":" << lineNumber
                              << ") invalid size values: " << value << std::endl;
                    parseOk = false;
                    continue;
                }
                if (width <= 0.0f || length <= 0.0f) {
                    std::cerr << "LevelSerialization::loadLevel(" << filePath << ":" << lineNumber
                              << ") vehicle size must be positive" << std::endl;
                    parseOk = false;
                    continue;
                }
                spawn.size = glm::vec2(width, length);
                continue;
            }

            std::cerr << "LevelSerialization::loadLevel(" << filePath << ":" << lineNumber
                      << ") unknown vehicle property: " << key << std::endl;
            parseOk = false;
        }

        if (!parseOk) {
            continue;
        }

        if (!grid.isValidPosition(spawn.gridPosition)) {
            std::cerr << "LevelSerialization::loadLevel(" << filePath << ":" << lineNumber
                      << ") vehicle coordinates out of bounds: (" << spawn.gridPosition.x << ", "
                      << spawn.gridPosition.y << ", " << spawn.gridPosition.z << ")" << std::endl;
            continue;
        }

        auto existing = std::find_if(data.vehicleSpawns.begin(), data.vehicleSpawns.end(), [&](const VehicleSpawnDefinition& entry) {
            return entry.gridPosition == spawn.gridPosition;
        });
        if (existing != data.vehicleSpawns.end()) {
            *existing = spawn;
        } else {
            data.vehicleSpawns.push_back(std::move(spawn));
        }
    }

    return true;
}

bool saveLevel(const std::string& filePath, const TileGrid& grid, const LevelData& data) {
    std::ofstream output(filePath);
    if (!output.is_open()) {
        std::cerr << "Failed to save level to file: " << filePath << std::endl;
        return false;
    }

    output << "# Tile grid exported by editor" << std::endl;
    const glm::ivec3& gridSize = grid.getGridSize();
    output << "grid " << gridSize.x << ' ' << gridSize.y << ' ' << gridSize.z << std::endl;
    output << "tile_size " << grid.getTileSize() << std::endl;

    std::vector<std::pair<std::string, std::string>> aliasEntries;
    const auto& aliasMap = grid.getTextureAliases();
    aliasEntries.reserve(aliasMap.size());
    for (const auto& entry : aliasMap) {
        if (!entry.first.empty() && !entry.second.empty()) {
            aliasEntries.emplace_back(entry.first, entry.second);
        }
    }

    std::sort(aliasEntries.begin(), aliasEntries.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first < rhs.first;
    });

    for (const auto& alias : aliasEntries) {
        output << "texture " << alias.first << ' ' << alias.second << std::endl;
    }

    std::unordered_map<std::string, std::string> pathToAlias;
    for (const auto& alias : aliasEntries) {
        pathToAlias[alias.second] = alias.first;
    }

    auto identifierForSave = [&](const std::string& value) -> std::string {
        if (value.empty()) {
            return std::string();
        }
        auto aliasIt = aliasMap.find(value);
        if (aliasIt != aliasMap.end()) {
            return aliasIt->first;
        }
        auto pathIt = pathToAlias.find(value);
        if (pathIt != pathToAlias.end()) {
            return pathIt->second;
        }
        return value;
    };

    auto formatFloat = [](float value) -> std::string {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << value;
        return oss.str();
    };

    for (const auto& spawn : data.vehicleSpawns) {
        output << "vehicle " << spawn.gridPosition.x << ' ' << spawn.gridPosition.y << ' ' << spawn.gridPosition.z;
        output << " rotation=" << formatFloat(spawn.rotationDegrees);
        if (!spawn.texturePath.empty()) {
            output << " texture=" << identifierForSave(spawn.texturePath);
        }
        output << " size=" << formatFloat(spawn.size.x) << 'x' << formatFloat(spawn.size.y);
        output << std::endl;
    }

    auto carDirectionToString = [](CarDirection dir) -> std::string {
        switch (dir) {
            case CarDirection::North: return "north";
            case CarDirection::South: return "south";
            case CarDirection::East: return "east";
            case CarDirection::West: return "west";
            case CarDirection::NorthSouth: return "north_south";
            case CarDirection::EastWest: return "east_west";
            case CarDirection::None: default: return "none";
        }
    };

    auto wallKey = [](WallDirection dir) -> const char* {
        switch (dir) {
            case WallDirection::North: return "north";
            case WallDirection::South: return "south";
            case WallDirection::East: return "east";
            case WallDirection::West: return "west";
        }
        return "north";
    };

    const glm::ivec3 size = grid.getGridSize();
    for (int z = 0; z < size.z; ++z) {
        for (int y = 0; y < size.y; ++y) {
            for (int x = 0; x < size.x; ++x) {
                const Tile* tile = grid.getTile(x, y, z);
                if (!tile) {
                    continue;
                }

                std::vector<std::string> properties;
                const TopSurfaceData& top = tile->getTopSurface();

                if (top.solid) {
                    std::string topProp = "top=solid";
                    const std::string topId = identifierForSave(top.texturePath);
                    if (!topId.empty()) {
                        topProp += ':' + topId;
                    }
                    properties.push_back(std::move(topProp));
                }

                if (top.carDirection != CarDirection::None) {
                    properties.push_back(std::string("car=") + carDirectionToString(top.carDirection));
                }

                for (int dirIndex = 0; dirIndex < 4; ++dirIndex) {
                    const WallDirection dir = static_cast<WallDirection>(dirIndex);
                    const WallData& wall = tile->getWall(dir);
                    if (wall.walkable && wall.texturePath.empty()) {
                        continue;
                    }

                    std::string entry = std::string(wallKey(dir)) + '=' + (wall.walkable ? "walkable" : "solid");
                    const std::string wallId = identifierForSave(wall.texturePath);
                    if (!wallId.empty()) {
                        entry += ':' + wallId;
                    }
                    properties.push_back(std::move(entry));
                }

                if (properties.empty()) {
                    continue;
                }

                output << "tile " << x << ' ' << y << ' ' << z;
                for (const auto& prop : properties) {
                    output << ' ' << prop;
                }
                output << std::endl;
            }
        }
    }

    std::cout << "Saved level to file: " << filePath << std::endl;
    return true;
}

} // namespace LevelSerialization
