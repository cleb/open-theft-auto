#include "LevelSerialization.hpp"

#include "LevelData.hpp"
#include "TileGrid.hpp"
#include "TextureManager.hpp"
#include "Heading.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace LevelSerialization {
struct GridAccess {
    static std::string resolveTexturePath(const std::string& identifier) {
        return TextureManager::instance().resolvePath(identifier);
    }

    static std::shared_ptr<Texture> loadTextureFromPath(const std::string& path) {
        return TextureManager::instance().getTextureFromPath(path);
    }
};
} // namespace LevelSerialization

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
        const std::string trimmed = trimCopy(text);
        float value = std::stof(trimmed, &processed);
        if (processed != trimmed.size()) {
            return false;
        }
        out = value;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

struct ParsedLine {
    int number = 0;
    std::string content;
};

std::string sanitizeLine(const std::string& rawLine) {
    const auto commentPos = rawLine.find('#');
    const std::string stripped = (commentPos == std::string::npos) ? rawLine : rawLine.substr(0, commentPos);
    return trimCopy(stripped);
}

struct LineLogger {
    const std::string& filePath;
    int lineNumber;

    void error(const std::string& message) const {
        std::cerr << "LevelSerialization::loadLevel(" << filePath << ":" << lineNumber << "): " << message << std::endl;
    }

    void warning(const std::string& message) const {
        std::cerr << "LevelSerialization::loadLevel(" << filePath << ":" << lineNumber << ") warning: "
                  << message << std::endl;
    }
};

bool parseIntStrict(const std::string& text, int& out) {
    const std::string trimmed = trimCopy(text);
    try {
        size_t processed = 0;
        int value = std::stoi(trimmed, &processed);
        if (processed != trimmed.size()) {
            return false;
        }
        out = value;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool parseRangeToken(const std::string& text, int& start, int& end) {
    const std::string trimmed = trimCopy(text);
    const auto dashPos = trimmed.find('-');
    if (dashPos == std::string::npos) {
        int value = 0;
        if (!parseIntStrict(trimmed, value)) {
            return false;
        }
        start = value;
        end = value;
        return true;
    }

    const std::string first = trimmed.substr(0, dashPos);
    const std::string second = trimmed.substr(dashPos + 1);
    int startValue = 0;
    int endValue = 0;
    if (!parseIntStrict(first, startValue) || !parseIntStrict(second, endValue)) {
        return false;
    }
    if (startValue > endValue) {
        std::swap(startValue, endValue);
    }
    start = startValue;
    end = endValue;
    return true;
}

int wallKeyToIndex(std::string key) {
    key = toLowerCopy(trimCopy(key));
    key.erase(std::remove_if(key.begin(), key.end(), [](char c) { return c == '_' || c == '-'; }), key.end());
    if (key.rfind("wall", 0) == 0) {
        key = key.substr(4);
    }
    if (key == "n" || key == "north") {
        return static_cast<int>(WallDirection::North);
    }
    if (key == "s" || key == "south") {
        return static_cast<int>(WallDirection::South);
    }
    if (key == "e" || key == "east") {
        return static_cast<int>(WallDirection::East);
    }
    if (key == "w" || key == "west") {
        return static_cast<int>(WallDirection::West);
    }
    return -1;
}

bool parseCarDirectionValue(const std::string& value, CarDirection& out, const LineLogger& logger) {
    const std::string lower = toLowerCopy(trimCopy(value));
    if (lower.empty() || lower == "none" || lower == "off") {
        out = CarDirection::None;
        return true;
    }
    if (lower == "north") {
        out = CarDirection::North;
        return true;
    }
    if (lower == "south") {
        out = CarDirection::South;
        return true;
    }
    if (lower == "east") {
        out = CarDirection::East;
        return true;
    }
    if (lower == "west") {
        out = CarDirection::West;
        return true;
    }
    if (lower == "southnorth" || lower == "south_north" || lower == "sn") {
        out = CarDirection::SouthNorth;
        return true;
    }
    if (lower == "westeast" || lower == "west_east" || lower == "we") {
        out = CarDirection::WestEast;
        return true;
    }
    if (lower == "northeast" || lower == "north_east" || lower == "ne") {
        out = CarDirection::NorthEast;
        return true;
    }
    if (lower == "northwest" || lower == "north_west" || lower == "nw") {
        out = CarDirection::NorthWest;
        return true;
    }
    if (lower == "southeast" || lower == "south_east" || lower == "se") {
        out = CarDirection::SouthEast;
        return true;
    }
    if (lower == "southwest" || lower == "south_west" || lower == "sw") {
        out = CarDirection::SouthWest;
        return true;
    }
    if (lower == "northeastsouthwest" || lower == "northeast_southwest" || lower == "nesw" || lower == "ne_sw") {
        out = CarDirection::NorthEastSouthWest;
        return true;
    }
    if (lower == "northwestsoutheast" || lower == "northwest_southeast" || lower == "nwse" || lower == "nw_se") {
        out = CarDirection::NorthWestSouthEast;
        return true;
    }
    if (lower == "optionalnortheast" || lower == "optional_northeast" || lower == "optional_ne" || lower == "one") {
        out = CarDirection::OptionalNorthEast;
        return true;
    }
    if (lower == "optionalnorthwest" || lower == "optional_northwest" || lower == "optional_nw" || lower == "onw") {
        out = CarDirection::OptionalNorthWest;
        return true;
    }
    if (lower == "optionalsoutheast" || lower == "optional_southeast" || lower == "optional_se" || lower == "ose") {
        out = CarDirection::OptionalSouthEast;
        return true;
    }
    if (lower == "optionalsouthwest" || lower == "optional_southwest" || lower == "optional_sw" || lower == "osw") {
        out = CarDirection::OptionalSouthWest;
        return true;
    }
    if (lower == "optionalnortheastsouthwest" || lower == "optional_northeast_southwest" || lower == "optional_nesw" || lower == "onesw") {
        out = CarDirection::OptionalNorthEastSouthWest;
        return true;
    }
    if (lower == "optionalnorthwestsoutheast" || lower == "optional_northwest_southeast" || lower == "optional_nwse" || lower == "onwse") {
        out = CarDirection::OptionalNorthWestSouthEast;
        return true;
    }

    logger.error("Unknown car direction: " + value);
    return false;
}

bool parseSidewalkDirectionValue(const std::string& value, SidewalkDirection& out, const LineLogger& logger) {
    const std::string lower = toLowerCopy(trimCopy(value));
    if (lower.empty() || lower == "none" || lower == "off") {
        out = SidewalkDirection::None;
        return true;
    }
    if (lower == "northsouth" || lower == "north_south" || lower == "ns" || lower == "n_s") {
        out = SidewalkDirection::NorthSouth;
        return true;
    }
    if (lower == "eastwest" || lower == "east_west" || lower == "ew" || lower == "e_w") {
        out = SidewalkDirection::EastWest;
        return true;
    }
    if (lower == "northeastsouthwest" || lower == "northeast_southwest" || lower == "nesw" || lower == "ne_sw") {
        out = SidewalkDirection::NorthEastSouthWest;
        return true;
    }
    if (lower == "northwestsoutheast" || lower == "northwest_southeast" || lower == "nwse" || lower == "nw_se") {
        out = SidewalkDirection::NorthWestSouthEast;
        return true;
    }

    logger.error("Unknown sidewalk direction: " + value);
    return false;
}

struct WallConfig {
    bool specified = false;
    bool walkable = true;
    std::string textureId;
};

struct TileConfig {
    bool topSpecified = false;
    bool topSolid = false;
    std::string topTextureId;
    bool carSpecified = false;
    CarDirection carDirection = CarDirection::None;
    bool sidewalkSpecified = false;
    SidewalkDirection sidewalkDirection = SidewalkDirection::None;
    bool drivabilitySpecified = false;
    float drivability = 1.0f;
    WallConfig walls[4];
    // Vehicle spawn weights for this tile
    std::vector<VehicleSpawnWeight> vehicleSpawnWeights;
    bool spawnWeightsSpecified = false;
};

bool parseWallValue(const std::string& value, WallConfig& wall, const LineLogger& logger) {
    const std::string trimmed = trimCopy(value);
    const auto colon = trimmed.find(':');
    std::string state = trimmed;
    std::string texture;
    if (colon != std::string::npos) {
        state = trimCopy(trimmed.substr(0, colon));
        texture = trimCopy(trimmed.substr(colon + 1));
    }
    const std::string lowerState = toLowerCopy(state);
    if (lowerState == "walkable" || lowerState == "open" || lowerState == "passable") {
        wall.walkable = true;
    } else if (lowerState == "solid" || lowerState == "blocked" || lowerState == "wall" || lowerState == "closed") {
        wall.walkable = false;
    } else {
        logger.error("Unknown wall state: " + state);
        return false;
    }
    wall.textureId = texture;
    wall.specified = true;
    return true;
}

bool parseTileProperty(const std::string& key, const std::string& value, TileConfig& config, const LineLogger& logger) {
    const std::string lowerKey = toLowerCopy(trimCopy(key));
    if (lowerKey == "top") {
        const std::string trimmed = trimCopy(value);
        const std::string lowerValue = toLowerCopy(trimmed);
        config.topSpecified = true;
        if (lowerValue == "none" || lowerValue == "off" || lowerValue == "false") {
            config.topSolid = false;
            config.topTextureId.clear();
            return true;
        }
        if (lowerValue.rfind("solid", 0) == 0) {
            config.topSolid = true;
            const auto colonPos = trimmed.find(':');
            if (colonPos != std::string::npos && colonPos + 1 < trimmed.size()) {
                config.topTextureId = trimCopy(trimmed.substr(colonPos + 1));
            } else {
                config.topTextureId.clear();
            }
            return true;
        }
        logger.error("Unknown top configuration: " + value);
        return false;
    }

    if (lowerKey == "car" || lowerKey == "cardirection" || lowerKey == "traffic") {
        config.carSpecified = true;
        return parseCarDirectionValue(value, config.carDirection, logger);
    }

    if (lowerKey == "sidewalk" || lowerKey == "sidewalkdirection" || lowerKey == "pedestrian") {
        config.sidewalkSpecified = true;
        return parseSidewalkDirectionValue(value, config.sidewalkDirection, logger);
    }

    // Parse drivability: drivability=0.3 (0.0-1.0, how easily vehicles can drive)
    if (lowerKey == "drivability" || lowerKey == "driv") {
        float driv = 0.0f;
        if (!parseFloat(value, driv)) {
            logger.error("Invalid drivability value: " + value);
            return false;
        }
        if (driv < 0.0f) driv = 0.0f;
        if (driv > 1.0f) driv = 1.0f;
        config.drivability = driv;
        config.drivabilitySpecified = true;
        return true;
    }

    // Parse spawn weights: spawn_sedan=1.5 or spawn_pickup=0.5
    if (lowerKey.rfind("spawn_", 0) == 0 || lowerKey.rfind("spawnweight_", 0) == 0) {
        // Extract vehicle type name from key
        std::string vehicleTypeName;
        if (lowerKey.rfind("spawn_", 0) == 0) {
            vehicleTypeName = lowerKey.substr(6);  // After "spawn_"
        } else {
            vehicleTypeName = lowerKey.substr(12);  // After "spawnweight_"
        }
        
        float weight = 0.0f;
        if (!parseFloat(value, weight)) {
            logger.error("Invalid spawn weight value: " + value);
            return false;
        }
        if (weight < 0.0f) {
            logger.warning("Spawn weight clamped to 0: " + value);
            weight = 0.0f;
        }
        
        // Check if this type already exists in the weights
        bool found = false;
        for (auto& w : config.vehicleSpawnWeights) {
            if (w.typeId == vehicleTypeName) {
                w.weight = weight;
                found = true;
                break;
            }
        }
        if (!found) {
            config.vehicleSpawnWeights.push_back({vehicleTypeName, weight});
        }
        config.spawnWeightsSpecified = true;
        return true;
    }

    const int wallIndex = wallKeyToIndex(lowerKey);
    if (wallIndex >= 0 && wallIndex < 4) {
        return parseWallValue(value, config.walls[wallIndex], logger);
    }

    logger.error("Unknown property key: " + key);
    return false;
}

void applyTileConfig(Tile& tile, const TileConfig& config) {
    if (config.topSpecified) {
        if (config.topSolid) {
            const std::string resolved = LevelSerialization::GridAccess::resolveTexturePath(config.topTextureId);
            std::shared_ptr<Texture> texture;
            if (!resolved.empty()) {
                texture = LevelSerialization::GridAccess::loadTextureFromPath(resolved);
            }
            tile.setTopSurface(true, resolved, CarDirection::None);
            if (texture) {
                tile.setTopTexture(texture);
            }
        } else {
            tile.setTopSurface(false, "", CarDirection::None);
        }
    }

    if (config.carSpecified) {
        tile.setCarDirection(config.carDirection);
    }

    if (config.sidewalkSpecified) {
        tile.setSidewalkDirection(config.sidewalkDirection);
    }

    // Apply drivability
    if (config.drivabilitySpecified) {
        tile.setDrivability(config.drivability);
    }

    // Apply vehicle spawn weights
    if (config.spawnWeightsSpecified) {
        for (const auto& weight : config.vehicleSpawnWeights) {
            tile.setVehicleSpawnWeight(weight.typeId, weight.weight);
        }
    }

    for (int i = 0; i < 4; ++i) {
        const WallConfig& wall = config.walls[i];
        if (!wall.specified) {
            continue;
        }
        const auto dir = static_cast<WallDirection>(i);

        std::string resolved;
        if (!wall.textureId.empty()) {
            resolved = LevelSerialization::GridAccess::resolveTexturePath(wall.textureId);
        }

        tile.setWall(dir, wall.walkable, resolved);

        if (!resolved.empty()) {
            auto texture = LevelSerialization::GridAccess::loadTextureFromPath(resolved);
            if (texture) {
                tile.setWallTexture(dir, texture);
            }
        }
    }
}

struct KeyValueTokens {
    std::vector<std::pair<std::string, std::string>> entries;
    bool valid = true;
};

KeyValueTokens collectKeyValueTokens(std::istringstream& stream, const LineLogger& logger) {
    KeyValueTokens result;
    std::string token;
    while (stream >> token) {
        const auto eqPos = token.find('=');
        if (eqPos == std::string::npos) {
            logger.error("Expected key=value pair but found '" + token + "'");
            result.valid = false;
            continue;
        }
        std::string key = token.substr(0, eqPos);
        std::string value = token.substr(eqPos + 1);
        result.entries.emplace_back(trimCopy(key), trimCopy(value));
    }
    return result;
}

bool parseVehicleProperty(const std::string& key,
                          const std::string& value,
                          VehicleSpawnDefinition& spawn,
                          const LineLogger& logger) {
    const std::string lowerKey = toLowerCopy(trimCopy(key));
    if (lowerKey == "rotation" || lowerKey == "angle" || lowerKey == "yaw") {
        float rotation = 0.0f;
        if (!parseFloat(value, rotation)) {
            logger.error("Invalid rotation value: " + value);
            return false;
        }
        // Levels authored before the heading refactor used the legacy convention
        // (0°=North/+Y, 90°=East/+X). Convert to the new heading convention
        // (0°=East/+X, 90°=North/+Y) on load.
        spawn.rotationDegrees = Heading::headingDegFromLegacyRotationDeg(rotation);
        return true;
    }

    if (lowerKey == "texture" || lowerKey == "tex") {
        spawn.texturePath = LevelSerialization::GridAccess::resolveTexturePath(value);
        return true;
    }

    if (lowerKey == "size" || lowerKey == "dimensions") {
        const std::string trimmed = trimCopy(value);
        const auto separator = trimmed.find_first_of("xX,");
        if (separator == std::string::npos) {
            logger.error("Invalid size format: " + value);
            return false;
        }
        const std::string first = trimCopy(trimmed.substr(0, separator));
        const std::string second = trimCopy(trimmed.substr(separator + 1));
        float width = 0.0f;
        float length = 0.0f;
        if (!parseFloat(first, width) || !parseFloat(second, length)) {
            logger.error("Invalid size values: " + value);
            return false;
        }
        if (width <= 0.0f || length <= 0.0f) {
            logger.error("Vehicle size must be positive");
            return false;
        }
        spawn.size = glm::vec2(width, length);
        return true;
    }

    if (lowerKey == "type" || lowerKey == "vehicletype" || lowerKey == "vehicle_type") {
        spawn.vehicleTypeId = toLowerCopy(trimCopy(value));
        return true;
    }

    logger.error("Unknown vehicle property: " + key);
    return false;
}

VehicleSpawnDefinition* findVehicleSpawnEntry(std::vector<VehicleSpawnDefinition>& spawns, const glm::ivec3& position) {
    auto it = std::find_if(spawns.begin(), spawns.end(), [&](const VehicleSpawnDefinition& entry) {
        return entry.gridPosition == position;
    });
    if (it == spawns.end()) {
        return nullptr;
    }
    return &(*it);
}

} // namespace

namespace LevelSerialization {

bool loadLevel(const std::string& filePath, TileGrid& grid, LevelData& data) {
    std::ifstream input(filePath);
    if (!input.is_open()) {
        std::cerr << "Failed to open level file: " << filePath << std::endl;
        return false;
    }

    std::vector<ParsedLine> lines;
    std::string rawLine;
    int lineNumber = 0;
    while (std::getline(input, rawLine)) {
        ++lineNumber;
        const std::string sanitized = sanitizeLine(rawLine);
        if (sanitized.empty()) {
            continue;
        }
        ParsedLine parsed;
        parsed.number = lineNumber;
        parsed.content = sanitized;
        lines.push_back(std::move(parsed));
    }

    data.vehicleSpawns.clear();
    data.pickups.clear();
    data.phoneBooths.clear();
    data.markers.clear();

    auto& texMgr = TextureManager::instance();
    std::unordered_map<std::string, std::string> aliasMap = texMgr.getAliases();
    glm::ivec3 parsedGrid = grid.m_gridSize;
    float parsedTileSize = grid.m_tileSize;
    bool gridSpecified = false;
    bool tileSizeSpecified = false;

    struct PendingVehicle {
        int lineNumber = 0;
        VehicleSpawnDefinition spawn;
    };

    std::vector<PendingVehicle> pendingVehicles;

    for (const ParsedLine& line : lines) {
        std::istringstream stream(line.content);
        std::string command;
        stream >> command;
        if (command.empty()) {
            continue;
        }

        const std::string lowerCmd = toLowerCopy(command);
        LineLogger logger{filePath, line.number};

        if (lowerCmd == "grid") {
            int w = 0;
            int h = 0;
            int d = 0;
            if (!(stream >> w >> h >> d)) {
                logger.error("Expected three integers after 'grid'");
                continue;
            }
            parsedGrid = glm::ivec3(w, h, d);
            gridSpecified = true;
        } else if (lowerCmd == "tile_size" || lowerCmd == "tilesize") {
            std::string valueStr;
            if (!(stream >> valueStr)) {
                logger.error("Expected a numeric value after 'tile_size'");
                continue;
            }
            float value = 0.0f;
            if (!parseFloat(valueStr, value) || value <= 0.0f) {
                logger.error("Invalid tile size value: " + valueStr);
                continue;
            }
            parsedTileSize = value;
            tileSizeSpecified = true;
        } else if (lowerCmd == "texture" || lowerCmd == "alias") {
            std::string alias;
            std::string pathValue;
            if (!(stream >> alias >> pathValue)) {
                logger.error("Expected 'texture <alias> <path>'");
                continue;
            }
            if (!alias.empty() && !pathValue.empty()) {
                aliasMap[alias] = pathValue;
            }
        }
    }

    // Register aliases with the global TextureManager
    for (const auto& entry : aliasMap) {
        texMgr.registerAlias(entry.first, entry.second);
    }
    if (tileSizeSpecified) {
        grid.m_tileSize = parsedTileSize;
    }
    if (gridSpecified) {
        grid.m_gridSize = parsedGrid;
    }

    if (!grid.rebuildTiles()) {
        return false;
    }

    for (const ParsedLine& line : lines) {
        std::istringstream stream(line.content);
        std::string command;
        stream >> command;
        if (command.empty()) {
            continue;
        }

        const std::string lowerCmd = toLowerCopy(command);
        LineLogger logger{filePath, line.number};

        if (lowerCmd == "tile") {
            int x = 0;
            int y = 0;
            int z = 0;
            if (!(stream >> x >> y >> z)) {
                logger.error("Expected coordinates after 'tile'");
                continue;
            }

            TileConfig config;
            bool parseOk = true;
            KeyValueTokens tokens = collectKeyValueTokens(stream, logger);
            if (!tokens.valid) {
                parseOk = false;
            }
            for (const auto& entry : tokens.entries) {
                if (!parseTileProperty(entry.first, entry.second, config, logger)) {
                    parseOk = false;
                }
            }

            if (!parseOk) {
                continue;
            }

            if (!grid.isValidPosition(x, y, z)) {
                logger.warning("Tile coordinates out of bounds: (" + std::to_string(x) + ", " + std::to_string(y) + ", "
                               + std::to_string(z) + ")");
                continue;
            }

            Tile* tile = grid.getTile(x, y, z);
            if (!tile) {
                continue;
            }

            applyTileConfig(*tile, config);
        } else if (lowerCmd == "fill") {
            KeyValueTokens tokens = collectKeyValueTokens(stream, logger);
            if (!tokens.valid) {
                continue;
            }

            int xStart = 0;
            int xEnd = 0;
            int yStart = 0;
            int yEnd = 0;
            int zStart = 0;
            int zEnd = 0;
            bool hasX = false;
            bool hasY = false;
            bool hasZ = false;

            TileConfig config;
            bool parseOk = true;
            for (const auto& entry : tokens.entries) {
                const std::string lowerKey = toLowerCopy(entry.first);
                if (lowerKey == "x") {
                    if (!parseRangeToken(entry.second, xStart, xEnd)) {
                        logger.error("Invalid x range: " + entry.second);
                        parseOk = false;
                    } else {
                        hasX = true;
                    }
                } else if (lowerKey == "y") {
                    if (!parseRangeToken(entry.second, yStart, yEnd)) {
                        logger.error("Invalid y range: " + entry.second);
                        parseOk = false;
                    } else {
                        hasY = true;
                    }
                } else if (lowerKey == "z") {
                    if (!parseRangeToken(entry.second, zStart, zEnd)) {
                        logger.error("Invalid z range: " + entry.second);
                        parseOk = false;
                    } else {
                        hasZ = true;
                    }
                } else {
                    if (!parseTileProperty(entry.first, entry.second, config, logger)) {
                        parseOk = false;
                    }
                }
            }

            if (!hasX || !hasY || !hasZ) {
                logger.error("Fill command requires x=, y=, and z= ranges");
                parseOk = false;
            }

            if (!parseOk) {
                continue;
            }

            for (int z = zStart; z <= zEnd; ++z) {
                for (int y = yStart; y <= yEnd; ++y) {
                    for (int x = xStart; x <= xEnd; ++x) {
                        if (!grid.isValidPosition(x, y, z)) {
                            logger.warning("Fill target out of bounds: (" + std::to_string(x) + ", " + std::to_string(y) + ", "
                                            + std::to_string(z) + ")");
                            continue;
                        }
                        Tile* tile = grid.getTile(x, y, z);
                        if (!tile) {
                            continue;
                        }
                        applyTileConfig(*tile, config);
                    }
                }
            }
        } else if (lowerCmd == "vehicle") {
            int x = 0;
            int y = 0;
            int z = 0;
            if (!(stream >> x >> y >> z)) {
                logger.error("Expected coordinates after 'vehicle'");
                continue;
            }

            VehicleSpawnDefinition spawn;
            spawn.gridPosition = glm::ivec3(x, y, z);

            bool parseOk = true;
            KeyValueTokens tokens = collectKeyValueTokens(stream, logger);
            if (!tokens.valid) {
                parseOk = false;
            }
            for (const auto& entry : tokens.entries) {
                if (!parseVehicleProperty(entry.first, entry.second, spawn, logger)) {
                    parseOk = false;
                }
            }

            if (!parseOk) {
                continue;
            }

            if (!grid.isValidPosition(spawn.gridPosition)) {
                logger.error("Vehicle coordinates out of bounds: (" + std::to_string(spawn.gridPosition.x) + ", "
                             + std::to_string(spawn.gridPosition.y) + ", " + std::to_string(spawn.gridPosition.z) + ")");
                continue;
            }
            pendingVehicles.push_back(PendingVehicle{line.number, std::move(spawn)});
        } else if (lowerCmd == "pickup" || lowerCmd == "item") {
            int x = 0;
            int y = 0;
            int z = 0;
            if (!(stream >> x >> y >> z)) {
                logger.error("Expected coordinates after 'pickup'");
                continue;
            }

            PickupSpawnDefinition spawn;
            spawn.gridPosition = glm::ivec3(x, y, z);

            bool parseOk = true;
            KeyValueTokens tokens = collectKeyValueTokens(stream, logger);
            if (!tokens.valid) {
                parseOk = false;
            }
            for (const auto& entry : tokens.entries) {
                const std::string lowerKey = toLowerCopy(entry.first);
                if (lowerKey == "type" || lowerKey == "id" || lowerKey == "pickup") {
                    PickupType parsedType = PickupType::Pistol;
                    if (!pickupTypeFromString(toLowerCopy(entry.second), parsedType)) {
                        logger.error("Unknown pickup type: " + entry.second);
                        parseOk = false;
                        continue;
                    }
                    spawn.type = parsedType;
                } else if (lowerKey == "ammo") {
                    int ammo = 0;
                    if (!parseIntStrict(entry.second, ammo)) {
                        logger.error("Invalid pickup ammo value: " + entry.second);
                        parseOk = false;
                        continue;
                    }
                    spawn.ammo = std::max(0, ammo);
                } else {
                    logger.warning("Unknown pickup property: " + entry.first);
                }
            }

            if (!parseOk) {
                continue;
            }

            if (!grid.isValidPosition(spawn.gridPosition)) {
                logger.error("Pickup coordinates out of bounds: (" + std::to_string(spawn.gridPosition.x) + ", "
                             + std::to_string(spawn.gridPosition.y) + ", " + std::to_string(spawn.gridPosition.z) + ")");
                continue;
            }

            auto& pickups = data.pickups;
            auto existing = std::find_if(pickups.begin(), pickups.end(), [&](const PickupSpawnDefinition& entry) {
                return entry.gridPosition == spawn.gridPosition;
            });
            if (existing != pickups.end()) {
                *existing = spawn;
            } else {
                pickups.push_back(spawn);
            }
        } else if (lowerCmd == "phone_booth" || lowerCmd == "phonebooth" || lowerCmd == "phone") {
            int x = 0;
            int y = 0;
            int z = 0;
            if (!(stream >> x >> y >> z)) {
                logger.error("Expected coordinates after 'phone_booth'");
                continue;
            }

            PhoneBoothSpawnDefinition spawn;
            spawn.gridPosition = glm::ivec3(x, y, z);

            KeyValueTokens tokens = collectKeyValueTokens(stream, logger);
            if (!tokens.valid) {
                continue;
            }
            for (const auto& entry : tokens.entries) {
                const std::string lowerKey = toLowerCopy(entry.first);
                if (lowerKey == "id") {
                    spawn.id = entry.second;
                } else if (lowerKey == "job" || lowerKey == "jobid") {
                    spawn.jobId = entry.second;
                } else {
                    logger.warning("Unknown phone_booth property: " + entry.first);
                }
            }

            if (!grid.isValidPosition(spawn.gridPosition)) {
                logger.error("phone_booth coordinates out of bounds");
                continue;
            }

            auto& booths = data.phoneBooths;
            auto existing = std::find_if(booths.begin(), booths.end(),
                [&](const PhoneBoothSpawnDefinition& b) {
                    return b.gridPosition == spawn.gridPosition;
                });
            if (existing != booths.end()) {
                *existing = spawn;
            } else {
                booths.push_back(spawn);
            }
        } else if (lowerCmd == "player" || lowerCmd == "player_spawn" || lowerCmd == "playerspawn") {
            int x = 0;
            int y = 0;
            int z = 0;
            if (!(stream >> x >> y >> z)) {
                logger.error("Expected coordinates after 'player'");
                continue;
            }

            if (!grid.isValidPosition(x, y, z)) {
                logger.error("Player spawn coordinates out of bounds: (" + std::to_string(x) + ", "
                             + std::to_string(y) + ", " + std::to_string(z) + ")");
                continue;
            }

            data.playerSpawn.gridPosition = glm::ivec3(x, y, z);
            data.playerSpawn.isSet = true;

            // Parse optional properties
            KeyValueTokens tokens = collectKeyValueTokens(stream, logger);
            for (const auto& entry : tokens.entries) {
                const std::string lowerKey = toLowerCopy(entry.first);
                if (lowerKey == "rotation" || lowerKey == "angle" || lowerKey == "yaw") {
                    float rotation = 0.0f;
                    if (parseFloat(entry.second, rotation)) {
                        data.playerSpawn.rotationDegrees = Heading::headingDegFromLegacyRotationDeg(rotation);
                    } else {
                        logger.warning("Invalid player rotation value: " + entry.second);
                    }
                }
            }
        } else if (lowerCmd == "marker") {
            int x = 0, y = 0, z = 0;
            if (!(stream >> x >> y >> z)) {
                logger.error("Expected coordinates after 'marker'");
                continue;
            }
            if (!grid.isValidPosition(x, y, z)) {
                logger.error("marker coordinates out of bounds");
                continue;
            }

            MarkerDefinition marker;
            marker.gridPosition = glm::ivec3(x, y, z);

            KeyValueTokens tokens = collectKeyValueTokens(stream, logger);
            for (const auto& entry : tokens.entries) {
                const std::string lowerKey = toLowerCopy(entry.first);
                if (lowerKey == "name") {
                    marker.name = entry.second;
                } else {
                    logger.warning("Unknown marker property: " + entry.first);
                }
            }

            if (marker.name.empty()) {
                logger.error("marker requires a name= property");
                continue;
            }

            // Replace existing marker with same name or add new
            auto& markers = data.markers;
            auto existing = std::find_if(markers.begin(), markers.end(),
                [&](const MarkerDefinition& m) { return m.name == marker.name; });
            if (existing != markers.end()) {
                *existing = marker;
            } else {
                markers.push_back(std::move(marker));
            }
        }
    }

    for (const PendingVehicle& pending : pendingVehicles) {
        LineLogger logger{filePath, pending.lineNumber};
        const glm::ivec3& position = pending.spawn.gridPosition;
        const Tile* supportTile = grid.getTile(position);
        if (!supportTile || !supportTile->isTopSolid()) {
            logger.error("Vehicle spawn requires a solid tile at the target position");
            continue;
        }

        if (auto* existing = findVehicleSpawnEntry(data.vehicleSpawns, position)) {
            *existing = pending.spawn;
        } else {
            data.vehicleSpawns.push_back(pending.spawn);
        }
    }

    std::cout << "Loaded level from file: " << filePath << std::endl;
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
    const auto& aliasMap = TextureManager::instance().getAliases();
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
    // rotation is a heading in degrees where 0°=+X (East) and angles increase CCW.
    output << " rotation=" << formatFloat(spawn.rotationDegrees);
        output << " type=" << spawn.vehicleTypeId;
        if (!spawn.texturePath.empty()) {
            output << " texture=" << identifierForSave(spawn.texturePath);
        }
        output << " size=" << formatFloat(spawn.size.x) << 'x' << formatFloat(spawn.size.y);
        output << std::endl;
    }

    for (const auto& pickup : data.pickups) {
        output << "pickup " << pickup.gridPosition.x << ' ' << pickup.gridPosition.y << ' ' << pickup.gridPosition.z;
        output << " type=" << pickupTypeToString(pickup.type);
        output << " ammo=" << pickup.ammo;
        output << std::endl;
    }

    for (const auto& booth : data.phoneBooths) {
        output << "phone_booth " << booth.gridPosition.x << ' ' << booth.gridPosition.y << ' ' << booth.gridPosition.z;
        if (!booth.id.empty()) {
            output << " id=" << booth.id;
        }
        if (!booth.jobId.empty()) {
            output << " job=" << booth.jobId;
        }
        output << std::endl;
    }

    // Save player spawn
    if (data.playerSpawn.isSet) {
        output << "player " << data.playerSpawn.gridPosition.x << ' ' 
               << data.playerSpawn.gridPosition.y << ' ' 
               << data.playerSpawn.gridPosition.z;
    // rotation is a heading in degrees where 0°=+X (East) and angles increase CCW.
    output << " rotation=" << formatFloat(data.playerSpawn.rotationDegrees);
        output << std::endl;
    }

    for (const auto& marker : data.markers) {
        output << "marker " << marker.gridPosition.x << ' ' << marker.gridPosition.y << ' ' << marker.gridPosition.z;
        output << " name=" << marker.name;
        output << std::endl;
    }

    auto carDirectionToString = [](CarDirection dir) -> std::string {
        switch (dir) {
            case CarDirection::North: return "north";
            case CarDirection::South: return "south";
            case CarDirection::East: return "east";
            case CarDirection::West: return "west";
            case CarDirection::SouthNorth: return "south_north";
            case CarDirection::WestEast: return "west_east";
            case CarDirection::NorthEast: return "north_east";
            case CarDirection::NorthWest: return "north_west";
            case CarDirection::SouthEast: return "south_east";
            case CarDirection::SouthWest: return "south_west";
            case CarDirection::NorthEastSouthWest: return "northeast_southwest";
            case CarDirection::NorthWestSouthEast: return "northwest_southeast";
            case CarDirection::OptionalNorthEast: return "optional_northeast";
            case CarDirection::OptionalNorthWest: return "optional_northwest";
            case CarDirection::OptionalSouthEast: return "optional_southeast";
            case CarDirection::OptionalSouthWest: return "optional_southwest";
            case CarDirection::OptionalNorthEastSouthWest: return "optional_northeast_southwest";
            case CarDirection::OptionalNorthWestSouthEast: return "optional_northwest_southeast";
            case CarDirection::None:
            default: return "none";
        }
    };

    auto sidewalkDirectionToString = [](SidewalkDirection dir) -> std::string {
        switch (dir) {
            case SidewalkDirection::NorthSouth: return "north_south";
            case SidewalkDirection::EastWest: return "east_west";
            case SidewalkDirection::NorthEastSouthWest: return "northeast_southwest";
            case SidewalkDirection::NorthWestSouthEast: return "northwest_southeast";
            case SidewalkDirection::None:
            default: return "none";
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

                if (top.sidewalkDirection != SidewalkDirection::None) {
                    properties.push_back(std::string("sidewalk=") + sidewalkDirectionToString(top.sidewalkDirection));
                }

                // Save drivability (only if different from default 1.0)
                if (std::abs(top.drivability - 1.0f) > 0.001f) {
                    std::ostringstream drivProp;
                    drivProp << "drivability=" << std::fixed << std::setprecision(2) << top.drivability;
                    properties.push_back(drivProp.str());
                }

                // Save vehicle spawn weights (only if they differ from default 1.0)
                for (const auto& weight : top.vehicleSpawnWeights) {
                    // Only save if weight is not 1.0 (default)
                    if (std::abs(weight.weight - 1.0f) > 0.001f) {
                        std::ostringstream weightProp;
                        weightProp << "spawn_" << weight.typeId << "=" << std::fixed << std::setprecision(2) << weight.weight;
                        properties.push_back(weightProp.str());
                    }
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
