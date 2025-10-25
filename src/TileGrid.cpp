#include "TileGrid.hpp"
#include "Renderer.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

TileGrid::TileGrid(const glm::ivec3& gridSize, float tileSize)
    : m_gridSize(gridSize), m_tileSize(tileSize) {
}

bool TileGrid::initialize() {
    m_textureCache.clear();
    m_textureAliases.clear();

    registerTextureAlias("grass", "assets/textures/grass.png");
    registerTextureAlias("road", "assets/textures/road.png");
    registerTextureAlias("wall", "assets/textures/wall.png");

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
    const size_t total = static_cast<size_t>(m_gridSize.x) * static_cast<size_t>(m_gridSize.y) * static_cast<size_t>(m_gridSize.z);
    m_tiles.reserve(total);

    for (int z = 0; z < m_gridSize.z; ++z) {
        for (int y = 0; y < m_gridSize.y; ++y) {
            for (int x = 0; x < m_gridSize.x; ++x) {
                m_tiles.push_back(std::make_unique<Tile>(glm::ivec3(x, y, z), m_tileSize));
            }
        }
    }

    updateWaterMesh();

    return true;
}

bool TileGrid::resize(const glm::ivec3& newSize) {
    if (newSize.x <= 0 || newSize.y <= 0 || newSize.z <= 0) {
        std::cerr << "TileGrid::resize: invalid grid size requested: " << newSize.x << "x" << newSize.y << "x" << newSize.z
                  << std::endl;
        return false;
    }

    if (newSize == m_gridSize) {
        return true;
    }

    const glm::ivec3 oldSize = m_gridSize;
    auto oldTiles = std::move(m_tiles);

    m_gridSize = newSize;
    if (!rebuildTiles()) {
        m_gridSize = oldSize;
        m_tiles = std::move(oldTiles);
        return false;
    }

    const int copyX = std::min(oldSize.x, newSize.x);
    const int copyY = std::min(oldSize.y, newSize.y);
    const int copyZ = std::min(oldSize.z, newSize.z);

    auto oldIndex = [&](int x, int y, int z) -> size_t {
        return static_cast<size_t>((z * oldSize.y + y) * oldSize.x + x);
    };

    for (int z = 0; z < copyZ; ++z) {
        for (int y = 0; y < copyY; ++y) {
            for (int x = 0; x < copyX; ++x) {
                const size_t idx = oldIndex(x, y, z);
                if (idx >= oldTiles.size()) {
                    continue;
                }

                const Tile* oldTile = oldTiles[idx].get();
                Tile* newTile = getTile(x, y, z);
                if (!oldTile || !newTile) {
                    continue;
                }

                newTile->copyFrom(*oldTile);
            }
        }
    }

    std::cout << "Resized tile grid: " << oldSize.x << "x" << oldSize.y << "x" << oldSize.z << " -> " << newSize.x << "x"
              << newSize.y << "x" << newSize.z << std::endl;

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

void TileGrid::updateWaterMesh() {
    if (m_gridSize.x <= 0 || m_gridSize.y <= 0) {
        m_waterMesh.reset();
        m_waterModelMatrix = glm::mat4(1.0f);
        return;
    }

    const float margin = m_tileSize;
    const float width = static_cast<float>(m_gridSize.x) * m_tileSize;
    const float depth = static_cast<float>(m_gridSize.y) * m_tileSize;
    const float halfWidth = width * 0.5f + margin;
    const float halfDepth = depth * 0.5f + margin;

    const float tileCountX = static_cast<float>(std::max(1, m_gridSize.x));
    const float tileCountY = static_cast<float>(std::max(1, m_gridSize.y));
    const glm::vec2 uvScale(tileCountX * 0.6f, tileCountY * 0.6f);

    std::vector<Vertex> vertices = {
        {{-halfWidth, -halfDepth, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ halfWidth, -halfDepth, 0.0f}, {0.0f, 0.0f, 1.0f}, {uvScale.x, 0.0f}},
        {{ halfWidth,  halfDepth, 0.0f}, {0.0f, 0.0f, 1.0f}, {uvScale.x, uvScale.y}},
        {{-halfWidth,  halfDepth, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, uvScale.y}}
    };

    std::vector<GLuint> indices = {0, 1, 2, 2, 3, 0};

    m_waterMesh = std::make_unique<Mesh>(vertices, indices);

    m_waterHeight = -m_tileSize - (m_tileSize * 0.15f);
    const float centerX = (static_cast<float>(m_gridSize.x) - 1.0f) * m_tileSize * 0.5f;
    const float centerY = (static_cast<float>(m_gridSize.y) - 1.0f) * m_tileSize * 0.5f;

    m_waterModelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(centerX, centerY, m_waterHeight));
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

    using WallConfig = Tile::WallUpdate;
    using TileConfig = Tile::Update;

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

    const auto resolveAlias = [this](const std::string& identifier) {
        return resolveTexturePath(identifier);
    };
    const auto loadTexture = [this](const std::string& path) {
        return loadTextureFromPath(path);
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

            tile->applyUpdate(config, resolveAlias, loadTexture);
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
                        tile->applyUpdate(config, resolveAlias, loadTexture);
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

    if (m_waterMesh) {
        Shader* waterShader = renderer->getShader("water");
        if (waterShader) {
            waterShader->use();
            waterShader->setMat4("model", m_waterModelMatrix);
            waterShader->setMat4("view", renderer->getViewMatrix());
            waterShader->setMat4("projection", renderer->getProjectionMatrix());
            waterShader->setVec3("lightPos", glm::vec3(10.0f, 10.0f, 10.0f));
            waterShader->setVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));
            waterShader->setVec3("cameraPos", renderer->getCameraPosition());
            waterShader->setVec3("deepColor", glm::vec3(0.0f, 0.19f, 0.35f));
            waterShader->setVec3("shallowColor", glm::vec3(0.12f, 0.55f, 0.70f));
            waterShader->setFloat("foamIntensity", 0.35f);
            waterShader->setFloat("waveStrength", 0.25f);
            waterShader->setFloat("waveFrequency", 0.35f);
            waterShader->setFloat("waveSpeed", 0.8f);
            waterShader->setFloat("time", static_cast<float>(glfwGetTime()));

            m_waterMesh->render();
            waterShader->unuse();
        }
    }

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

