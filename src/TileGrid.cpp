#include "TileGrid.hpp"
#include "Renderer.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

TileGrid::TileGrid(const glm::ivec3& gridSize, float tileSize)
    : m_gridSize(gridSize), m_tileSize(tileSize) {
}

bool TileGrid::initialize() {
    m_textureCache.clear();
    m_textureAliases.clear();

    registerTextureAlias("grass", "assets/textures/grass.png");
    registerTextureAlias("road", "assets/textures/road.png");
    registerTextureAlias("wall", "assets/textures/wall.png");
    registerTextureAlias("car", "assets/textures/car.png");

    if (!rebuildTiles()) {
        std::cerr << "Failed to initialize tile grid tiles" << std::endl;
        return false;
    }

    std::cout << "Initialized tile grid: " << m_gridSize.x << "x" << m_gridSize.y << "x" << m_gridSize.z
              << " (" << m_tiles.size() << " tiles)" << std::endl;

    return true;
}

bool TileGrid::rebuildTiles() {
    if (m_gridSize.x <= 0 || m_gridSize.y <= 0 || m_gridSize.z <= 0) {
        std::cerr << "Invalid grid size: " << m_gridSize.x << "x" << m_gridSize.y << "x" << m_gridSize.z << std::endl;
        return false;
    }

    m_tiles.clear();
    m_vehicleSpawns.clear();
    const size_t total = static_cast<size_t>(m_gridSize.x) * static_cast<size_t>(m_gridSize.y) * static_cast<size_t>(m_gridSize.z);
    m_tiles.reserve(total);

    for (int z = 0; z < m_gridSize.z; ++z) {
        for (int y = 0; y < m_gridSize.y; ++y) {
            for (int x = 0; x < m_gridSize.x; ++x) {
                m_tiles.push_back(std::make_unique<Tile>(glm::ivec3(x, y, z), m_tileSize));
            }
        }
    }

    return true;
}

void TileGrid::registerTextureAlias(const std::string& alias, const std::string& path) {
    if (alias.empty() || path.empty()) {
        return;
    }
    m_textureAliases[alias] = path;
}

std::shared_ptr<Texture> TileGrid::loadTexture(const std::string& identifier) {
    const std::string resolvedPath = resolveTexturePath(identifier);
    return loadTextureFromPath(resolvedPath);
}

std::shared_ptr<Texture> TileGrid::loadTextureFromPath(const std::string& path) {
    if (path.empty()) {
        return nullptr;
    }

    auto cacheIt = m_textureCache.find(path);
    if (cacheIt != m_textureCache.end()) {
        return cacheIt->second;
    }

    auto texture = std::make_shared<Texture>();
    if (!texture->loadFromFile(path)) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        return nullptr;
    }

    m_textureCache[path] = texture;
    return texture;
}

std::string TileGrid::resolveTexturePath(const std::string& identifier) const {
    auto aliasIt = m_textureAliases.find(identifier);
    if (aliasIt != m_textureAliases.end()) {
        return aliasIt->second;
    }
    return identifier;
}

bool TileGrid::loadFromFile(const std::string& filePath) {
    std::ifstream input(filePath);
    if (!input.is_open()) {
        std::cerr << "Failed to open tile grid file: " << filePath << std::endl;
        return false;
    }

    std::vector<std::string> lines;
    std::string rawLine;
    while (std::getline(input, rawLine)) {
        lines.push_back(rawLine);
    }
    input.close();

    auto trimCopy = [](const std::string& text) {
        size_t begin = 0;
        size_t end = text.size();
        while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) {
            ++begin;
        }
        while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
            --end;
        }
        return text.substr(begin, end - begin);
    };

    auto toLowerCopy = [](std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return text;
    };

    auto logParseError = [&](int lineNumber, const std::string& message) {
        std::cerr << "TileGrid::loadFromFile(" << filePath << ":" << lineNumber << "): " << message << std::endl;
    };

    auto logParseWarning = [&](int lineNumber, const std::string& message) {
        std::cerr << "TileGrid::loadFromFile(" << filePath << ":" << lineNumber << ") warning: " << message << std::endl;
    };

    auto parseInt = [&](const std::string& text, int& out) -> bool {
        try {
            size_t processed = 0;
            int value = std::stoi(text, &processed);
            if (processed != trimCopy(text).size()) {
                return false;
            }
            out = value;
            return true;
        } catch (const std::exception&) {
            return false;
        }
    };

    auto parseFloat = [&](const std::string& text, float& out) -> bool {
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
    };

    auto parseRange = [&](const std::string& text, int& start, int& end) -> bool {
        const std::string trimmed = trimCopy(text);
        const auto dashPos = trimmed.find('-');
        if (dashPos == std::string::npos) {
            int value = 0;
            if (!parseInt(trimmed, value)) {
                return false;
            }
            start = value;
            end = value;
            return true;
        }

        const std::string first = trimCopy(trimmed.substr(0, dashPos));
        const std::string second = trimCopy(trimmed.substr(dashPos + 1));
        int startValue = 0;
        int endValue = 0;
        if (!parseInt(first, startValue) || !parseInt(second, endValue)) {
            return false;
        }
        if (startValue > endValue) {
            std::swap(startValue, endValue);
        }
        start = startValue;
        end = endValue;
        return true;
    };

    std::unordered_map<std::string, std::string> aliasMap = m_textureAliases;
    glm::ivec3 parsedGrid = m_gridSize;
    float parsedTileSize = m_tileSize;
    bool gridSpecified = false;
    bool tileSizeSpecified = false;

    for (size_t idx = 0; idx < lines.size(); ++idx) {
        const int lineNumber = static_cast<int>(idx + 1);
        std::string line = lines[idx];
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
        if (command.empty()) {
            continue;
        }

        const std::string lowerCmd = toLowerCopy(command);

        if (lowerCmd == "grid") {
            int w = 0;
            int h = 0;
            int d = 0;
            if (!(iss >> w >> h >> d)) {
                logParseError(lineNumber, "Expected three integers after 'grid'");
                continue;
            }
            parsedGrid = glm::ivec3(w, h, d);
            gridSpecified = true;
        } else if (lowerCmd == "tile_size" || lowerCmd == "tilesize") {
            std::string valueStr;
            if (!(iss >> valueStr)) {
                logParseError(lineNumber, "Expected a numeric value after 'tile_size'");
                continue;
            }
            float value = 0.0f;
            if (!parseFloat(valueStr, value) || value <= 0.0f) {
                logParseError(lineNumber, "Invalid tile size value: " + valueStr);
                continue;
            }
            parsedTileSize = value;
            tileSizeSpecified = true;
        } else if (lowerCmd == "texture" || lowerCmd == "alias") {
            std::string alias;
            std::string path;
            if (!(iss >> alias >> path)) {
                logParseError(lineNumber, "Expected 'texture <alias> <path>'");
                continue;
            }
            aliasMap[alias] = path;
        }
    }

    m_textureAliases = aliasMap;

    if (tileSizeSpecified) {
        m_tileSize = parsedTileSize;
    }
    if (gridSpecified) {
        m_gridSize = parsedGrid;
    }

    if (!rebuildTiles()) {
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
        WallConfig walls[4];
    };

    struct VehicleConfig {
        bool rotationSpecified = false;
        float rotation = 0.0f;
        bool textureSpecified = false;
        std::string textureId;
        bool sizeSpecified = false;
        glm::vec2 size = glm::vec2(1.5f, 3.0f);
    };

    auto parseCarDirection = [&](const std::string& value, CarDirection& out, int lineNumber) -> bool {
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
        if (lower == "northsouth" || lower == "north_south" || lower == "ns") {
            out = CarDirection::NorthSouth;
            return true;
        }
        if (lower == "eastwest" || lower == "east_west" || lower == "ew") {
            out = CarDirection::EastWest;
            return true;
        }
        logParseError(lineNumber, "Unknown car direction: " + value);
        return false;
    };

    auto wallKeyToIndex = [&](std::string key) -> int {
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
    };

    auto parseWallValue = [&](const std::string& value, WallConfig& wall, int lineNumber) -> bool {
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
            logParseError(lineNumber, "Unknown wall state: " + state);
            return false;
        }
        wall.textureId = texture;
        wall.specified = true;
        return true;
    };

    auto parseTileProperty = [&](const std::string& key, const std::string& value, TileConfig& config, int lineNumber) -> bool {
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
            logParseError(lineNumber, "Unknown top configuration: " + value);
            return false;
        }
        if (lowerKey == "car" || lowerKey == "cardirection" || lowerKey == "traffic") {
            config.carSpecified = true;
            return parseCarDirection(value, config.carDirection, lineNumber);
        }
        const int wallIndex = wallKeyToIndex(lowerKey);
        if (wallIndex >= 0 && wallIndex < 4) {
            return parseWallValue(value, config.walls[wallIndex], lineNumber);
        }
        logParseError(lineNumber, "Unknown property key: " + key);
        return false;
    };

    auto parseVehicleProperty = [&](const std::string& key, const std::string& value, VehicleConfig& config, int lineNumber) -> bool {
        const std::string lowerKey = toLowerCopy(trimCopy(key));
        if (lowerKey == "rotation" || lowerKey == "angle" || lowerKey == "yaw") {
            float rotation = 0.0f;
            if (!parseFloat(value, rotation)) {
                logParseError(lineNumber, "Invalid rotation value: " + value);
                return false;
            }
            config.rotation = rotation;
            config.rotationSpecified = true;
            return true;
        }

        if (lowerKey == "texture" || lowerKey == "tex") {
            config.textureId = trimCopy(value);
            config.textureSpecified = true;
            return true;
        }

        if (lowerKey == "size" || lowerKey == "dimensions") {
            const std::string trimmed = trimCopy(value);
            auto separator = trimmed.find('x');
            if (separator == std::string::npos) {
                separator = trimmed.find(',');
            }
            if (separator == std::string::npos) {
                logParseError(lineNumber, "Invalid size format: " + value);
                return false;
            }
            const std::string first = trimCopy(trimmed.substr(0, separator));
            const std::string second = trimCopy(trimmed.substr(separator + 1));
            float width = 0.0f;
            float length = 0.0f;
            if (!parseFloat(first, width) || !parseFloat(second, length)) {
                logParseError(lineNumber, "Invalid size values: " + value);
                return false;
            }
            if (width <= 0.0f || length <= 0.0f) {
                logParseError(lineNumber, "Vehicle size must be positive");
                return false;
            }
            config.size = glm::vec2(width, length);
            config.sizeSpecified = true;
            return true;
        }

        logParseError(lineNumber, "Unknown vehicle property: " + key);
        return false;
    };

    auto applyTileConfig = [&](Tile& tile, const TileConfig& config) {
        if (config.topSpecified) {
            if (config.topSolid) {
                const std::string resolved = resolveTexturePath(config.topTextureId);
                std::shared_ptr<Texture> texture;
                if (!resolved.empty()) {
                    texture = loadTextureFromPath(resolved);
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

        for (int i = 0; i < 4; ++i) {
            const WallConfig& wall = config.walls[i];
            if (!wall.specified) {
                continue;
            }
            const auto dir = static_cast<WallDirection>(i);

            std::string resolved;
            if (!wall.textureId.empty()) {
                resolved = resolveTexturePath(wall.textureId);
            }

            tile.setWall(dir, wall.walkable, resolved);

            if (!resolved.empty()) {
                auto texture = loadTextureFromPath(resolved);
                if (texture) {
                    tile.setWallTexture(dir, texture);
                }
            }
        }
    };

    for (size_t idx = 0; idx < lines.size(); ++idx) {
        const int lineNumber = static_cast<int>(idx + 1);
        std::string line = lines[idx];
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
        if (command.empty()) {
            continue;
        }

        const std::string lowerCmd = toLowerCopy(command);
        if (lowerCmd == "tile") {
            int x = 0;
            int y = 0;
            int z = 0;
            if (!(iss >> x >> y >> z)) {
                logParseError(lineNumber, "Expected coordinates after 'tile'");
                continue;
            }

            TileConfig config;
            bool parseOk = true;
            std::string token;
            while (iss >> token) {
                const auto eqPos = token.find('=');
                if (eqPos == std::string::npos) {
                    logParseError(lineNumber, "Expected key=value pair but found '" + token + "'");
                    parseOk = false;
                    continue;
                }
                const std::string key = token.substr(0, eqPos);
                const std::string value = token.substr(eqPos + 1);
                if (!parseTileProperty(key, value, config, lineNumber)) {
                    parseOk = false;
                }
            }

            if (!parseOk) {
                continue;
            }

            if (!isValidPosition(x, y, z)) {
                logParseWarning(lineNumber, "Tile coordinates out of bounds: (" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")");
                continue;
            }

            Tile* tile = getTile(x, y, z);
            if (!tile) {
                continue;
            }

            applyTileConfig(*tile, config);
        } else if (lowerCmd == "vehicle") {
            int x = 0;
            int y = 0;
            int z = 0;
            if (!(iss >> x >> y >> z)) {
                logParseError(lineNumber, "Expected coordinates after 'vehicle'");
                continue;
            }

            VehicleConfig config;
            bool parseOk = true;
            std::string token;
            while (iss >> token) {
                const auto eqPos = token.find('=');
                if (eqPos == std::string::npos) {
                    logParseError(lineNumber, "Expected key=value pair but found '" + token + "'");
                    parseOk = false;
                    continue;
                }
                const std::string key = token.substr(0, eqPos);
                const std::string value = token.substr(eqPos + 1);
                if (!parseVehicleProperty(key, value, config, lineNumber)) {
                    parseOk = false;
                }
            }

            if (!parseOk) {
                continue;
            }

            if (!isValidPosition(x, y, z)) {
                logParseWarning(lineNumber, "Vehicle coordinates out of bounds: (" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")");
                continue;
            }

            VehicleSpawn spawn;
            spawn.gridPosition = glm::ivec3(x, y, z);
            spawn.rotationDegrees = config.rotation;
            spawn.size = config.size;

            std::string textureIdentifier = config.textureId;
            if (textureIdentifier.empty()) {
                textureIdentifier = "car";
            }
            spawn.texturePath = resolveTexturePath(textureIdentifier);

            addOrUpdateVehicleSpawn(spawn);
        } else if (lowerCmd == "fill") {
            int xStart = 0;
            int xEnd = 0;
            int yStart = 0;
            int yEnd = 0;
            int zStart = 0;
            int zEnd = 0;
            bool hasX = false;
            bool hasY = false;
            bool hasZ = false;

            std::vector<std::pair<std::string, std::string>> properties;
            std::string token;
            while (iss >> token) {
                const auto eqPos = token.find('=');
                if (eqPos == std::string::npos) {
                    logParseError(lineNumber, "Expected key=value pair but found '" + token + "'");
                    continue;
                }
                const std::string key = token.substr(0, eqPos);
                const std::string value = token.substr(eqPos + 1);
                const std::string lowerKey = toLowerCopy(trimCopy(key));

                if (lowerKey == "x") {
                    if (!parseRange(value, xStart, xEnd)) {
                        logParseError(lineNumber, "Invalid x range: " + value);
                    } else {
                        hasX = true;
                    }
                } else if (lowerKey == "y") {
                    if (!parseRange(value, yStart, yEnd)) {
                        logParseError(lineNumber, "Invalid y range: " + value);
                    } else {
                        hasY = true;
                    }
                } else if (lowerKey == "z") {
                    if (!parseRange(value, zStart, zEnd)) {
                        logParseError(lineNumber, "Invalid z range: " + value);
                    } else {
                        hasZ = true;
                    }
                } else {
                    properties.emplace_back(key, value);
                }
            }

            if (!hasX || !hasY || !hasZ) {
                logParseError(lineNumber, "Fill command requires x=, y=, and z= ranges");
                continue;
            }

            TileConfig config;
            bool parseOk = true;
            for (const auto& property : properties) {
                if (!parseTileProperty(property.first, property.second, config, lineNumber)) {
                    parseOk = false;
                }
            }
            if (!parseOk) {
                continue;
            }

            for (int z = zStart; z <= zEnd; ++z) {
                for (int y = yStart; y <= yEnd; ++y) {
                    for (int x = xStart; x <= xEnd; ++x) {
                        if (!isValidPosition(x, y, z)) {
                            logParseWarning(lineNumber, "Fill target out of bounds: (" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")");
                            continue;
                        }
                        Tile* tile = getTile(x, y, z);
                        if (!tile) {
                            continue;
                        }
                        applyTileConfig(*tile, config);
                    }
                }
            }
        }
    }

    std::cout << "Loaded tile grid from file: " << filePath << std::endl;
    return true;
}

void TileGrid::render(Renderer* renderer) {
    if (!renderer) return;
    
    for (auto& tile : m_tiles) {
        if (tile) {
            tile->render(renderer);
        }
    }
}

Tile* TileGrid::getTile(int x, int y, int z) {
    if (!isValidPosition(x, y, z)) {
        return nullptr;
    }
    
    int index = getIndex(x, y, z);
    return m_tiles[index].get();
}

Tile* TileGrid::getTile(const glm::ivec3& gridPos) {
    return getTile(gridPos.x, gridPos.y, gridPos.z);
}

const Tile* TileGrid::getTile(int x, int y, int z) const {
    if (!isValidPosition(x, y, z)) {
        return nullptr;
    }

    int index = getIndex(x, y, z);
    return m_tiles[index].get();
}

const Tile* TileGrid::getTile(const glm::ivec3& gridPos) const {
    return getTile(gridPos.x, gridPos.y, gridPos.z);
}

bool TileGrid::isValidPosition(int x, int y, int z) const {
    return x >= 0 && x < m_gridSize.x &&
           y >= 0 && y < m_gridSize.y &&
           z >= 0 && z < m_gridSize.z;
}

bool TileGrid::isValidPosition(const glm::ivec3& gridPos) const {
    return isValidPosition(gridPos.x, gridPos.y, gridPos.z);
}

int TileGrid::getIndex(int x, int y, int z) const {
    return x + y * m_gridSize.x + z * m_gridSize.x * m_gridSize.y;
}

bool TileGrid::hasGroundSupport(const glm::ivec3& tilePos) const {
    int groundZ = tilePos.z - 1;
    if (groundZ < 0) {
        return false;
    }

    const Tile* groundTile = getTile(tilePos.x, tilePos.y, groundZ);
    if (!groundTile) {
        return false;
    }

    return groundTile->isTopSolid();
}

glm::vec3 TileGrid::gridToWorld(const glm::ivec3& gridPos) const {
    return glm::vec3(
        gridPos.x * m_tileSize,
        gridPos.y * m_tileSize,
        (gridPos.z - 1) * m_tileSize
    );
}

glm::ivec3 TileGrid::worldToGrid(const glm::vec3& worldPos) const {
    const float halfSize = m_tileSize * 0.5f;
    return glm::ivec3(
        static_cast<int>(std::floor((worldPos.x + halfSize) / m_tileSize)),
        static_cast<int>(std::floor((worldPos.y + halfSize) / m_tileSize)),
        static_cast<int>(std::floor((worldPos.z + m_tileSize) / m_tileSize))
    );
}

bool TileGrid::canOccupy(const glm::vec3& startPos, const glm::vec3& endPos) const {
    glm::ivec3 startTile = worldToGrid(startPos);
    glm::ivec3 endTile = worldToGrid(endPos);

    if (!isValidPosition(startTile.x, startTile.y, startTile.z)) {
        return false;
    }

    if (!isValidPosition(endTile.x, endTile.y, endTile.z)) {
        return false;
    }

    if (startTile == endTile) {
        return hasGroundSupport(endTile);
    }

    glm::ivec3 diff = endTile - startTile;
    if (diff.z != 0) {
        return false;
    }

    int manhattan = glm::abs(diff.x) + glm::abs(diff.y);
    if (manhattan > 1) {
        return false;
    }

    WallDirection fromDir;
    WallDirection toDir;

    if (diff.x == 1) {
        fromDir = WallDirection::West;
        toDir = WallDirection::East;
    } else if (diff.x == -1) {
        fromDir = WallDirection::East;
        toDir = WallDirection::West;
    } else if (diff.y == 1) {
        fromDir = WallDirection::South;
        toDir = WallDirection::North;
    } else if (diff.y == -1) {
        fromDir = WallDirection::North;
        toDir = WallDirection::South;
    } else {
        return hasGroundSupport(endTile);
    }

    const Tile* fromTile = getTile(startTile);
    const Tile* toTile = getTile(endTile);

    if (!fromTile || !toTile) {
        return false;
    }

    if (!fromTile->isWallWalkable(fromDir) || !toTile->isWallWalkable(toDir)) {
        return false;
    }

    return hasGroundSupport(endTile);
}

bool TileGrid::isRoadTile(const glm::vec3& worldPos) const {
    glm::ivec3 gridPos = worldToGrid(worldPos);
    if (!isValidPosition(gridPos)) {
        return false;
    }
    return isRoadTile(gridPos);
}

bool TileGrid::isRoadTile(const glm::ivec3& gridPos) const {
    if (!isValidPosition(gridPos)) {
        return false;
    }

    const Tile* tile = getTile(gridPos);
    if (!tile) {
        return false;
    }

    return tile->getCarDirection() != CarDirection::None;
}

TileGrid::VehicleSpawn* TileGrid::findVehicleSpawn(const glm::ivec3& gridPos) {
    auto it = std::find_if(
        m_vehicleSpawns.begin(),
        m_vehicleSpawns.end(),
        [&](const VehicleSpawn& spawn) { return spawn.gridPosition == gridPos; });
    if (it == m_vehicleSpawns.end()) {
        return nullptr;
    }
    return &(*it);
}

const TileGrid::VehicleSpawn* TileGrid::findVehicleSpawn(const glm::ivec3& gridPos) const {
    return const_cast<TileGrid*>(this)->findVehicleSpawn(gridPos);
}

void TileGrid::addOrUpdateVehicleSpawn(const VehicleSpawn& spawn) {
    if (!isValidPosition(spawn.gridPosition)) {
        return;
    }

    VehicleSpawn normalized = spawn;
    if (normalized.texturePath.empty()) {
        normalized.texturePath = resolveTexturePath("car");
    }
    while (normalized.rotationDegrees < 0.0f) {
        normalized.rotationDegrees += 360.0f;
    }
    while (normalized.rotationDegrees >= 360.0f) {
        normalized.rotationDegrees -= 360.0f;
    }
    if (normalized.size.x <= 0.0f) {
        normalized.size.x = 1.5f;
    }
    if (normalized.size.y <= 0.0f) {
        normalized.size.y = 3.0f;
    }
    auto* existing = findVehicleSpawn(spawn.gridPosition);
    if (existing) {
        *existing = normalized;
        return;
    }

    m_vehicleSpawns.push_back(normalized);
}

bool TileGrid::removeVehicleSpawnAt(const glm::ivec3& gridPos) {
    auto it = std::find_if(
        m_vehicleSpawns.begin(),
        m_vehicleSpawns.end(),
        [&](const VehicleSpawn& spawn) { return spawn.gridPosition == gridPos; });
    if (it == m_vehicleSpawns.end()) {
        return false;
    }
    m_vehicleSpawns.erase(it);
    return true;
}

bool TileGrid::saveToFile(const std::string& filePath) const {
    std::ofstream output(filePath);
    if (!output.is_open()) {
        std::cerr << "Failed to save tile grid to file: " << filePath << std::endl;
        return false;
    }

    output << "# Tile grid exported by editor" << std::endl;
    output << "grid " << m_gridSize.x << ' ' << m_gridSize.y << ' ' << m_gridSize.z << std::endl;
    output << "tile_size " << m_tileSize << std::endl;

    std::vector<std::pair<std::string, std::string>> aliasEntries(m_textureAliases.begin(), m_textureAliases.end());
    std::sort(aliasEntries.begin(), aliasEntries.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first < rhs.first;
    });

    for (const auto& alias : aliasEntries) {
        if (alias.first.empty() || alias.second.empty()) {
            continue;
        }
        output << "texture " << alias.first << ' ' << alias.second << std::endl;
    }

    std::unordered_map<std::string, std::string> pathToAlias;
    for (const auto& alias : aliasEntries) {
        if (!alias.second.empty()) {
            pathToAlias[alias.second] = alias.first;
        }
    }

    auto identifierForSave = [&](const std::string& value) -> std::string {
        if (value.empty()) {
            return std::string();
        }
        if (m_textureAliases.find(value) != m_textureAliases.end()) {
            return value;
        }
        auto aliasIt = pathToAlias.find(value);
        if (aliasIt != pathToAlias.end()) {
            return aliasIt->second;
        }
        return value;
    };

    auto formatFloat = [](float value) -> std::string {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << value;
        return oss.str();
    };

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

    for (const auto& spawn : m_vehicleSpawns) {
        output << "vehicle " << spawn.gridPosition.x << ' ' << spawn.gridPosition.y << ' ' << spawn.gridPosition.z;
        output << " rotation=" << formatFloat(spawn.rotationDegrees);
        if (!spawn.texturePath.empty()) {
            output << " texture=" << identifierForSave(spawn.texturePath);
        }
        output << " size=" << formatFloat(spawn.size.x) << 'x' << formatFloat(spawn.size.y);
        output << std::endl;
    }

    for (int z = 0; z < m_gridSize.z; ++z) {
        for (int y = 0; y < m_gridSize.y; ++y) {
            for (int x = 0; x < m_gridSize.x; ++x) {
                const Tile* tile = getTile(x, y, z);
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

    std::cout << "Saved tile grid to file: " << filePath << std::endl;
    return true;
}

