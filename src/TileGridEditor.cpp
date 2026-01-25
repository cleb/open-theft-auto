#include "TileGridEditor.hpp"

#include "TileGrid.hpp"
#include "TextureManager.hpp"
#include "Tile.hpp"
#include "Renderer.hpp"
#include "InputManager.hpp"
#include "Mesh.hpp"
#include "Texture.hpp"
#include "LevelData.hpp"
#include "LevelSerialization.hpp"
#include "VehicleConfig.hpp"
#include "Window.hpp"
#include "Camera.hpp"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace {
constexpr const char* kWallLabels[4] = {"North", "South", "East", "West"};
constexpr const char* kDefaultVehicleTexture = "assets/textures/sedan.png";
float normalizeDegrees(float value) {
    if (!std::isfinite(value)) {
        return 0.0f;
    }
    float normalized = std::fmod(value, 360.0f);
    if (normalized < 0.0f) {
        normalized += 360.0f;
    }
    return normalized;
}

const char* carDirectionToString(CarDirection dir) {
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
        case CarDirection::None:
        default: return "none";
    }
}

const char* wallDirectionToString(WallDirection dir) {
    switch (dir) {
        case WallDirection::North: return "north";
        case WallDirection::South: return "south";
        case WallDirection::East: return "east";
        case WallDirection::West: return "west";
    }
    return "north";
}

int carDirectionToIndex(CarDirection dir) {
    switch (dir) {
        case CarDirection::None: return 0;
        case CarDirection::North: return 1;
        case CarDirection::South: return 2;
        case CarDirection::East: return 3;
        case CarDirection::West: return 4;
        case CarDirection::SouthNorth: return 5;
        case CarDirection::WestEast: return 6;
        case CarDirection::NorthEast: return 7;
        case CarDirection::NorthWest: return 8;
        case CarDirection::SouthEast: return 9;
        case CarDirection::SouthWest: return 10;
        case CarDirection::NorthEastSouthWest: return 11;
        case CarDirection::NorthWestSouthEast: return 12;
    }
    return 0;
}

CarDirection indexToCarDirection(int index) {
    switch (index) {
        case 1: return CarDirection::North;
        case 2: return CarDirection::South;
        case 3: return CarDirection::East;
        case 4: return CarDirection::West;
        case 5: return CarDirection::SouthNorth;
        case 6: return CarDirection::WestEast;
        case 7: return CarDirection::NorthEast;
        case 8: return CarDirection::NorthWest;
        case 9: return CarDirection::SouthEast;
        case 10: return CarDirection::SouthWest;
        case 11: return CarDirection::NorthEastSouthWest;
        case 12: return CarDirection::NorthWestSouthEast;
        default: return CarDirection::None;
    }
}

const char* sidewalkDirectionToString(SidewalkDirection dir) {
    switch (dir) {
        case SidewalkDirection::NorthSouth: return "north_south";
        case SidewalkDirection::EastWest: return "east_west";
        case SidewalkDirection::NorthEastSouthWest: return "northeast_southwest";
        case SidewalkDirection::NorthWestSouthEast: return "northwest_southeast";
        case SidewalkDirection::None:
        default: return "none";
    }
}

int sidewalkDirectionToIndex(SidewalkDirection dir) {
    switch (dir) {
        case SidewalkDirection::None: return 0;
        case SidewalkDirection::NorthSouth: return 1;
        case SidewalkDirection::EastWest: return 2;
        case SidewalkDirection::NorthEastSouthWest: return 3;
        case SidewalkDirection::NorthWestSouthEast: return 4;
    }
    return 0;
}

SidewalkDirection indexToSidewalkDirection(int index) {
    switch (index) {
        case 1: return SidewalkDirection::NorthSouth;
        case 2: return SidewalkDirection::EastWest;
        case 3: return SidewalkDirection::NorthEastSouthWest;
        case 4: return SidewalkDirection::NorthWestSouthEast;
        default: return SidewalkDirection::None;
    }
}

std::string trimCopy(const std::string& value) {
    auto begin = value.begin();
    while (begin != value.end() && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }

    auto end = value.end();
    while (end != begin && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }

    return std::string(begin, end);
}
}

TileGridEditor::TileGridEditor()
    : m_grid(nullptr)
    , m_levelData(nullptr)
    , m_window(nullptr)
    , m_renderer(nullptr)
    , m_enabled(false)
    , m_cursor(0)
    , m_lastAnnouncedCursor(
          std::numeric_limits<int>::min(),
          std::numeric_limits<int>::min(),
          std::numeric_limits<int>::min())
    , m_brush(BrushType::Grass)
    , m_lastAnnouncedBrush(BrushType::Empty)
    , m_roadDirection(CarDirection::SouthNorth)
    , m_sidewalkDirection(SidewalkDirection::NorthSouth)
    , m_cursorColor(0.3f, 0.9f, 0.3f)
    , m_arrowColor(0.95f, 0.7f, 0.1f)
    , m_isSelecting(false)
    , m_selectionStart(0)
    , m_selectionEnd(0)
    , m_selectionColor(0.2f, 0.6f, 0.9f)
    , m_moveMode(false)
    , m_moveOffset(0)
    , m_hasHoverTile(false)
    , m_hoverTile(0)
    , m_hoverColor(1.0f, 1.0f, 0.3f)
    , m_hoverLayerOffset(0)
    , m_edgeScrollSpeed(15.0f)
    , m_edgeScrollMargin(50.0f)
    , m_helpPrinted(false)
    , m_pendingGridSize(0)
    , m_gridResizeError()
    , m_uiPlayerSpawnState()
    , m_playerSpawnColor(0.2f, 0.8f, 1.0f)
    , m_showNewLevelDialog(false)
    , m_showLoadLevelDialog(false)
    , m_showSaveAsDialog(false)
    , m_newLevelSize(16, 16, 4)
    , m_newLevelTileSize(3.0f)
    , m_filePathBuffer{}
    , m_fileDialogError() {
    std::snprintf(m_uiVehicleState.texture.data(), m_uiVehicleState.texture.size(), "%s", kDefaultVehicleTexture);
    std::snprintf(m_filePathBuffer.data(), m_filePathBuffer.size(), "assets/levels/new_level.tg");
}

TileGridEditor::~TileGridEditor() = default;

void TileGridEditor::initialize(TileGrid* grid, LevelData* levelData) {
    m_grid = grid;
    m_levelData = levelData;
    m_cursorMesh.reset();
    m_arrowMesh.reset();
    m_selectionMesh.reset();
    m_playerSpawnMesh.reset();
    clearSelection();
    clampCursor();
    ensureCursorMesh();
    ensureSelectionMesh();
    refreshCursorColor();
    rebuildAliasList();
    refreshUiStateFromTile();
    m_selectedPrefabIndex = -1;
    syncPendingGridSizeFromGrid();
    m_gridResizeError.clear();
}

void TileGridEditor::setLevelPath(const std::string& path) {
    m_levelPath = path;
    rebuildAliasList();
}

void TileGridEditor::setCursor(const glm::ivec3& gridPos) {
    m_cursor = gridPos;
    clampCursor();
    announceCursor();
    refreshUiStateFromTile();
}

void TileGridEditor::setEnabled(bool enabled) {
    if (enabled == m_enabled) {
        return;
    }

    m_enabled = enabled;
    if (m_enabled) {
        ensureCursorMesh();
        refreshCursorColor();
        m_lastAnnouncedCursor = glm::ivec3(
            std::numeric_limits<int>::min(),
            std::numeric_limits<int>::min(),
            std::numeric_limits<int>::min());
        announceCursor();
        m_lastAnnouncedBrush = BrushType::Empty;
        announceBrush();
        rebuildAliasList();
        refreshUiStateFromTile();
        if (m_newPrefabName[0] == '\0') {
            std::snprintf(m_newPrefabName.data(), m_newPrefabName.size(), "Prefab %d", m_prefabAutoNameCounter);
        }
        syncPendingGridSizeFromGrid();
        m_gridResizeError.clear();
        if (!m_helpPrinted) {
            printHelp();
            m_helpPrinted = true;
        }
    }
}

void TileGridEditor::update(float deltaTime) {
    (void)deltaTime;
    if (!m_enabled) {
        return;
    }
    clampCursor();
}

void TileGridEditor::processInput(InputManager* input, float deltaTime) {
    if (!m_enabled || !m_grid || !input) {
        return;
    }

    handleSaveHotkey(input);

    ImGuiIO& io = ImGui::GetIO();
    const bool captureKeyboard = io.WantCaptureKeyboard;
    const bool captureMouse = io.WantCaptureMouse;

    // Edge scrolling - move camera when mouse is near window edges (but inside window)
    if (!captureMouse && m_window && m_renderer && input->isCursorInWindow()) {
        const int windowWidth = m_window->getWidth();
        const int windowHeight = m_window->getHeight();
        const double mouseX = input->getMouseX();
        const double mouseY = input->getMouseY();
        
        glm::vec3 scrollDir(0.0f);
        
        if (mouseX < m_edgeScrollMargin) {
            scrollDir.x = -1.0f;
        } else if (mouseX > windowWidth - m_edgeScrollMargin) {
            scrollDir.x = 1.0f;
        }
        
        if (mouseY < m_edgeScrollMargin) {
            scrollDir.y = 1.0f;  // Y is inverted in screen coords
        } else if (mouseY > windowHeight - m_edgeScrollMargin) {
            scrollDir.y = -1.0f;
        }
        
        if (scrollDir != glm::vec3(0.0f)) {
            Camera* camera = m_renderer->getCamera();
            if (camera) {
                camera->move(scrollDir * m_edgeScrollSpeed * deltaTime);
            }
        }
    }

    if (!captureKeyboard) {
        handleBrushHotkeys(input);
        handleWallHotkeys(input);
        handlePrefabHotkeys(input);
        handleSelectionHotkeys(input);

        // Don't handle cursor movement or layer changes in move mode
        if (!m_moveMode) {
            if (input->isKeyPressed(GLFW_KEY_UP) || input->isKeyPressed(GLFW_KEY_W)) {
                moveCursor(0, 1);
            }
            if (input->isKeyPressed(GLFW_KEY_DOWN) || input->isKeyPressed(GLFW_KEY_S)) {
                moveCursor(0, -1);
            }
            if (input->isKeyPressed(GLFW_KEY_LEFT) || input->isKeyPressed(GLFW_KEY_A)) {
                moveCursor(-1, 0);
            }
            if (input->isKeyPressed(GLFW_KEY_RIGHT) || input->isKeyPressed(GLFW_KEY_D)) {
                moveCursor(1, 0);
            }

            if (input->isKeyPressed(GLFW_KEY_Q)) {
                changeLayer(-1);
            }
            if (input->isKeyPressed(GLFW_KEY_E)) {
                changeLayer(1);
            }
        }

        if (m_brush == BrushType::Road && input->isKeyPressed(GLFW_KEY_R)) {
            switch (m_roadDirection) {
                case CarDirection::SouthNorth:
                    m_roadDirection = CarDirection::WestEast;
                    break;
                case CarDirection::WestEast:
                    m_roadDirection = CarDirection::North;
                    break;
                case CarDirection::North:
                    m_roadDirection = CarDirection::South;
                    break;
                case CarDirection::South:
                    m_roadDirection = CarDirection::East;
                    break;
                case CarDirection::East:
                    m_roadDirection = CarDirection::West;
                    break;
                case CarDirection::West:
                    m_roadDirection = CarDirection::NorthEast;
                    break;
                case CarDirection::NorthEast:
                    m_roadDirection = CarDirection::NorthWest;
                    break;
                case CarDirection::NorthWest:
                    m_roadDirection = CarDirection::SouthEast;
                    break;
                case CarDirection::SouthEast:
                    m_roadDirection = CarDirection::SouthWest;
                    break;
                case CarDirection::SouthWest:
                    m_roadDirection = CarDirection::NorthEastSouthWest;
                    break;
                case CarDirection::NorthEastSouthWest:
                    m_roadDirection = CarDirection::NorthWestSouthEast;
                    break;
                case CarDirection::NorthWestSouthEast:
                default:
                    m_roadDirection = CarDirection::SouthNorth;
                    break;
            }
            announceBrush();
        } else if (m_brush == BrushType::Sidewalk && input->isKeyPressed(GLFW_KEY_R)) {
            switch (m_sidewalkDirection) {
                case SidewalkDirection::NorthSouth:
                    m_sidewalkDirection = SidewalkDirection::EastWest;
                    break;
                case SidewalkDirection::EastWest:
                    m_sidewalkDirection = SidewalkDirection::NorthEastSouthWest;
                    break;
                case SidewalkDirection::NorthEastSouthWest:
                    m_sidewalkDirection = SidewalkDirection::NorthWestSouthEast;
                    break;
                case SidewalkDirection::NorthWestSouthEast:
                default:
                    m_sidewalkDirection = SidewalkDirection::NorthSouth;
                    break;
            }
            announceBrush();
        } else if (m_brush == BrushType::Vehicle && input->isKeyPressed(GLFW_KEY_R)) {
            m_uiVehicleState.rotationDegrees = normalizeDegrees(m_uiVehicleState.rotationDegrees + 90.0f);
            announceBrush();
        } else if (m_brush == BrushType::PlayerSpawn && input->isKeyPressed(GLFW_KEY_R)) {
            m_uiPlayerSpawnState.rotationDegrees = normalizeDegrees(m_uiPlayerSpawnState.rotationDegrees + 90.0f);
            announceBrush();
        }
        if (m_brush == BrushType::Vehicle && input->isKeyPressed(GLFW_KEY_DELETE)) {
            removeVehicleAtCursor();
        }

        // Delete key clears tile contents at cursor (any brush mode)
        if (input->isKeyPressed(GLFW_KEY_DELETE)) {
            clearTileAtCursor();
        }
    }

    // Handle mouse selection
    if (!captureMouse) {
        // Update hover tile
        m_hasHoverTile = getTileAtScreenPosition(input->getMouseX(), input->getMouseY(), m_hoverTile);
        
        // Apply hover layer offset from scroll wheel
        if (m_hasHoverTile && m_grid) {
            const glm::ivec3& gridSize = m_grid->getGridSize();
            m_hoverTile.z = std::clamp(m_hoverTile.z + m_hoverLayerOffset, 0, gridSize.z - 1);
        }
        
        // Handle scroll wheel to change hover layer offset
        double scrollY = input->getScrollDeltaY();
        if (scrollY != 0.0) {
            if (m_grid) {
                const glm::ivec3& gridSize = m_grid->getGridSize();
                if (scrollY > 0) {
                    m_hoverLayerOffset = std::min(m_hoverLayerOffset + 1, gridSize.z - 1);
                } else {
                    m_hoverLayerOffset = std::max(m_hoverLayerOffset - 1, -(gridSize.z - 1));
                }
            }
        }
        
        static int hoverDebugCounter = 0;
        if (++hoverDebugCounter % 60 == 0) {
            if (m_hasHoverTile) {
                std::cout << "Hover tile: (" << m_hoverTile.x << ", " << m_hoverTile.y << ", " << m_hoverTile.z << ")" << std::endl;
            } else {
                std::cout << "No hover tile detected. Window: " << (m_window ? "OK" : "NULL") 
                          << ", Renderer: " << (m_renderer ? "OK" : "NULL") 
                          << ", Camera: " << (m_renderer && m_renderer->getCamera() ? "OK" : "NULL") << std::endl;
            }
        }
        
        handleMouseSelection(input);
    }

    const bool shiftDown = input->isKeyDown(GLFW_KEY_LEFT_SHIFT) || input->isKeyDown(GLFW_KEY_RIGHT_SHIFT);
    const bool ctrlDown = input->isKeyDown(GLFW_KEY_LEFT_CONTROL) || input->isKeyDown(GLFW_KEY_RIGHT_CONTROL);
    const bool applySpace = !captureKeyboard && input->isKeyPressed(GLFW_KEY_SPACE);
    
    // Left click without modifiers: if a prefab is selected, apply it; otherwise navigate to tile
    const bool plainClick = !captureMouse && input->isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT) 
                            && !m_isSelecting && !shiftDown && !ctrlDown;
    if (plainClick && m_hasHoverTile) {
        if (m_selectedPrefabIndex >= 0 && m_selectedPrefabIndex < static_cast<int>(m_prefabs.size())) {
            // Prefab selected: navigate and apply
            m_cursor = m_hoverTile;
            clampCursor();
            announceCursor();
            refreshUiStateFromTile();
            applyPrefab(static_cast<std::size_t>(m_selectedPrefabIndex));
        } else {
            // No prefab selected: just navigate to the tile (like keyboard navigation)
            m_cursor = m_hoverTile;
            clampCursor();
            announceCursor();
            refreshUiStateFromTile();
        }
    }
    
    // Space always applies the current brush at the cursor position
    if (applySpace) {
        if (shiftDown) {
            applyBucketFill();
        } else {
            applyBrush();
        }
    }
}

void TileGridEditor::render(Renderer* renderer) {
    if (!m_enabled || !m_grid || !renderer) {
        return;
    }
    ensureCursorMesh();
    ensureArrowMesh();
    ensureSelectionMesh();
    ensurePlayerSpawnMesh();

    if (m_arrowMesh) {
        const glm::ivec3 gridSize = m_grid->getGridSize();
        const float tileSize = m_grid->getTileSize();
        const float heightOffset = tileSize * 0.03f;

        for (int z = 0; z < gridSize.z; ++z) {
            for (int y = 0; y < gridSize.y; ++y) {
                for (int x = 0; x < gridSize.x; ++x) {
                    Tile* tile = m_grid->getTile(x, y, z);
                    if (!tile) {
                        continue;
                    }

                    const TopSurfaceData& top = tile->getTopSurface();
                    if (top.carDirection == CarDirection::None) {
                        continue;
                    }

                    glm::vec3 base = m_grid->gridToWorld(glm::ivec3(x, y, z));
                    base.z += tileSize + heightOffset;

                    auto renderArrow = [&](float rotation) {
                        glm::mat4 model = glm::translate(glm::mat4(1.0f), base);
                        model = glm::rotate(model, rotation, glm::vec3(0.0f, 0.0f, 1.0f));
                        renderer->renderMesh(*m_arrowMesh, model, "model", m_arrowColor);
                    };

                    switch (top.carDirection) {
                        case CarDirection::North:
                            renderArrow(0.0f);
                            break;
                        case CarDirection::South:
                            renderArrow(glm::pi<float>());
                            break;
                        case CarDirection::East:
                            renderArrow(-glm::half_pi<float>());
                            break;
                        case CarDirection::West:
                            renderArrow(glm::half_pi<float>());
                            break;
                        case CarDirection::SouthNorth:
                            renderArrow(0.0f);
                            renderArrow(glm::pi<float>());
                            break;
                        case CarDirection::WestEast:
                            renderArrow(glm::half_pi<float>());
                            renderArrow(-glm::half_pi<float>());
                            break;
                        case CarDirection::NorthEast:
                            renderArrow(-glm::quarter_pi<float>());
                            break;
                        case CarDirection::NorthWest:
                            renderArrow(glm::quarter_pi<float>());
                            break;
                        case CarDirection::SouthEast:
                            renderArrow(-glm::pi<float>() + glm::quarter_pi<float>());
                            break;
                        case CarDirection::SouthWest:
                            renderArrow(glm::pi<float>() - glm::quarter_pi<float>());
                            break;
                        case CarDirection::NorthEastSouthWest:
                            renderArrow(-glm::quarter_pi<float>());
                            renderArrow(glm::pi<float>() - glm::quarter_pi<float>());
                            break;
                        case CarDirection::NorthWestSouthEast:
                            renderArrow(glm::quarter_pi<float>());
                            renderArrow(-glm::pi<float>() + glm::quarter_pi<float>());
                            break;
                        case CarDirection::None:
                        default:
                            break;
                    }
                }
            }
        }
    }

    // Render selection boxes
    renderSelection(renderer);

    // Render hover tile with cursor mesh (more visible)
    if (m_hasHoverTile && m_cursorMesh && !isSelected(m_hoverTile)) {
        const glm::vec3 worldPos = m_grid->gridToWorld(m_hoverTile);
        const float offset = m_grid->getTileSize() * 0.08f; // Higher than selection
        glm::mat4 model = glm::translate(glm::mat4(1.0f), worldPos + glm::vec3(0.0f, 0.0f, offset));
        renderer->renderMesh(*m_cursorMesh, model, "model", m_hoverColor);
    }

    // Render player spawn indicator
    if (m_playerSpawnMesh && m_levelData && m_levelData->playerSpawn.isSet) {
        const glm::vec3 worldPos = m_grid->gridToWorld(m_levelData->playerSpawn.gridPosition);
        const float tileSize = m_grid->getTileSize();
        const float offset = tileSize + 0.05f;  // Just above the tile surface
        glm::mat4 model = glm::translate(glm::mat4(1.0f), worldPos + glm::vec3(0.0f, 0.0f, offset));
        model = glm::rotate(model, glm::radians(m_levelData->playerSpawn.rotationDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
        renderer->renderMesh(*m_playerSpawnMesh, model, "model", m_playerSpawnColor);
    }

    if (m_cursorMesh) {
        const glm::vec3 base = m_grid->gridToWorld(m_cursor);
        const float offset = m_grid->getTileSize() * 0.02f;
        glm::mat4 model = glm::translate(glm::mat4(1.0f), base + glm::vec3(0.0f, 0.0f, offset));
        renderer->renderMesh(*m_cursorMesh, model, "model", m_cursorColor);
    }
}

void TileGridEditor::drawGui() {
    if (!ImGui::Begin("Map Editor")) {
        ImGui::End();
        return;
    }

    if (!m_grid) {
        ImGui::TextUnformatted("Tile grid unavailable.");
        ImGui::End();
        return;
    }

    if (!m_levelPath.empty()) {
        ImGui::Text("Level: %s", m_levelPath.c_str());
    } else {
        ImGui::TextDisabled("Level: (unsaved)");
    }

    ImGui::Text("Mode: %s", m_enabled ? "Edit" : "Gameplay");

    if (!m_enabled) {
        ImGui::Separator();
        ImGui::TextUnformatted("Press F1 to enter edit mode.");
        ImGui::End();
        return;
    }

    ImGui::Separator();
    ImGui::Text("Cursor: (%d, %d, %d)", m_cursor.x, m_cursor.y, m_cursor.z);

    drawFileManagementControls();
    drawGridControls();
    drawBrushControls();
    drawSelectionControls();
    drawPrefabControls();

    drawTileFaceTabs();

    // Draw modal dialogs
    drawNewLevelDialog();
    drawLoadLevelDialog();
    drawSaveAsDialog();

    ImGui::End();
}

Tile* TileGridEditor::currentTile() {
    return m_grid ? m_grid->getTile(m_cursor) : nullptr;
}

const Tile* TileGridEditor::currentTile() const {
    return m_grid ? m_grid->getTile(m_cursor) : nullptr;
}

VehicleSpawnDefinition* TileGridEditor::findVehicleSpawn(const glm::ivec3& gridPos) {
    if (!m_levelData) {
        return nullptr;
    }

    auto& spawns = m_levelData->vehicleSpawns;
    auto it = std::find_if(spawns.begin(), spawns.end(), [&](const VehicleSpawnDefinition& spawn) {
        return spawn.gridPosition == gridPos;
    });
    if (it == spawns.end()) {
        return nullptr;
    }
    return &(*it);
}

const VehicleSpawnDefinition* TileGridEditor::findVehicleSpawn(const glm::ivec3& gridPos) const {
    return const_cast<TileGridEditor*>(this)->findVehicleSpawn(gridPos);
}

TileGridEditor::VehiclePlacementStatus TileGridEditor::evaluateVehiclePlacement(const glm::ivec3& position) const {
    if (!m_grid || !m_grid->isValidPosition(position)) {
        return VehiclePlacementStatus::OutOfBounds;
    }

    const Tile* tile = m_grid->getTile(position);
    if (!tile || !tile->isTopSolid()) {
        return VehiclePlacementStatus::MissingSupport;
    }

    return VehiclePlacementStatus::Valid;
}

void TileGridEditor::ensureCursorMesh() {
    if (!m_grid || m_cursorMesh) {
        return;
    }

    const float tileSize = m_grid->getTileSize();
    const float halfSize = tileSize * 0.5f;
    const float height = tileSize;
    const float lineWidth = tileSize * 0.06f;

    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;

    // Helper to add a face outline (4 edges as thin quads)
    auto addFaceOutline = [&](const glm::vec3& corner0, const glm::vec3& corner1, 
                               const glm::vec3& corner2, const glm::vec3& corner3,
                               const glm::vec3& normal) {
        // Edge 0-1
        GLuint base = static_cast<GLuint>(vertices.size());
        glm::vec3 dir01 = glm::normalize(corner1 - corner0);
        glm::vec3 perp01 = glm::cross(normal, dir01) * lineWidth * 0.5f;
        vertices.push_back({corner0 - perp01, normal, {0.0f, 0.0f}});
        vertices.push_back({corner0 + perp01, normal, {0.0f, 1.0f}});
        vertices.push_back({corner1 + perp01, normal, {1.0f, 1.0f}});
        vertices.push_back({corner1 - perp01, normal, {1.0f, 0.0f}});
        indices.insert(indices.end(), {base, base+1, base+2, base+2, base+3, base});

        // Edge 1-2
        base = static_cast<GLuint>(vertices.size());
        glm::vec3 dir12 = glm::normalize(corner2 - corner1);
        glm::vec3 perp12 = glm::cross(normal, dir12) * lineWidth * 0.5f;
        vertices.push_back({corner1 - perp12, normal, {0.0f, 0.0f}});
        vertices.push_back({corner1 + perp12, normal, {0.0f, 1.0f}});
        vertices.push_back({corner2 + perp12, normal, {1.0f, 1.0f}});
        vertices.push_back({corner2 - perp12, normal, {1.0f, 0.0f}});
        indices.insert(indices.end(), {base, base+1, base+2, base+2, base+3, base});

        // Edge 2-3
        base = static_cast<GLuint>(vertices.size());
        glm::vec3 dir23 = glm::normalize(corner3 - corner2);
        glm::vec3 perp23 = glm::cross(normal, dir23) * lineWidth * 0.5f;
        vertices.push_back({corner2 - perp23, normal, {0.0f, 0.0f}});
        vertices.push_back({corner2 + perp23, normal, {0.0f, 1.0f}});
        vertices.push_back({corner3 + perp23, normal, {1.0f, 1.0f}});
        vertices.push_back({corner3 - perp23, normal, {1.0f, 0.0f}});
        indices.insert(indices.end(), {base, base+1, base+2, base+2, base+3, base});

        // Edge 3-0
        base = static_cast<GLuint>(vertices.size());
        glm::vec3 dir30 = glm::normalize(corner0 - corner3);
        glm::vec3 perp30 = glm::cross(normal, dir30) * lineWidth * 0.5f;
        vertices.push_back({corner3 - perp30, normal, {0.0f, 0.0f}});
        vertices.push_back({corner3 + perp30, normal, {0.0f, 1.0f}});
        vertices.push_back({corner0 + perp30, normal, {1.0f, 1.0f}});
        vertices.push_back({corner0 - perp30, normal, {1.0f, 0.0f}});
        indices.insert(indices.end(), {base, base+1, base+2, base+2, base+3, base});
    };

    // Define the 8 corners of the cube
    glm::vec3 v0(-halfSize, -halfSize, 0.0f);      // bottom front left
    glm::vec3 v1( halfSize, -halfSize, 0.0f);      // bottom front right
    glm::vec3 v2( halfSize,  halfSize, 0.0f);      // bottom back right
    glm::vec3 v3(-halfSize,  halfSize, 0.0f);      // bottom back left
    glm::vec3 v4(-halfSize, -halfSize, height);    // top front left
    glm::vec3 v5( halfSize, -halfSize, height);    // top front right
    glm::vec3 v6( halfSize,  halfSize, height);    // top back right
    glm::vec3 v7(-halfSize,  halfSize, height);    // top back left

    // Top face (Z+)
    addFaceOutline(v4, v5, v6, v7, glm::vec3(0.0f, 0.0f, 1.0f));
    // Bottom face (Z-)
    addFaceOutline(v0, v3, v2, v1, glm::vec3(0.0f, 0.0f, -1.0f));
    // Front face (Y-)
    addFaceOutline(v0, v1, v5, v4, glm::vec3(0.0f, -1.0f, 0.0f));
    // Back face (Y+)
    addFaceOutline(v2, v3, v7, v6, glm::vec3(0.0f, 1.0f, 0.0f));
    // Left face (X-)
    addFaceOutline(v3, v0, v4, v7, glm::vec3(-1.0f, 0.0f, 0.0f));
    // Right face (X+)
    addFaceOutline(v1, v2, v6, v5, glm::vec3(1.0f, 0.0f, 0.0f));

    m_cursorMesh = std::make_unique<Mesh>(vertices, indices);
    if (!m_cursorTexture) {
        m_cursorTexture = std::make_shared<Texture>();
        m_cursorTexture->createSolidColor(255, 255, 255, 96);
    }
    m_cursorMesh->setTexture(m_cursorTexture);
}

void TileGridEditor::ensureArrowMesh() {
    if (!m_grid || m_arrowMesh) {
        return;
    }

    const float tileSize = m_grid->getTileSize();
    const float arrowLength = tileSize * 0.7f;
    const float tailLength = arrowLength * 0.55f;

    const float tailStart = -arrowLength * 0.5f;
    const float tailEnd = tailStart + tailLength;
    const float tipY = tailStart + arrowLength;

    const float halfTailWidth = tileSize * 0.09f;
    const float halfHeadWidth = tileSize * 0.22f;

    const auto makeVertex = [&](float x, float y) {
        const float u = (x + halfHeadWidth) / (2.0f * halfHeadWidth);
        const float v = (y - tailStart) / arrowLength;
        return Vertex{{x, y, 0.0f}, {0.0f, 0.0f, 1.0f}, {u, v}};
    };

    std::vector<Vertex> vertices = {
        makeVertex(-halfTailWidth, tailStart),    // 0: tail bottom-left
        makeVertex(halfTailWidth, tailStart),     // 1: tail bottom-right
        makeVertex(-halfTailWidth, tailEnd),      // 2: tail top-left
        makeVertex(halfTailWidth, tailEnd),       // 3: tail top-right
        makeVertex(-halfHeadWidth, tailEnd),      // 4: head left corner
        makeVertex(halfHeadWidth, tailEnd),       // 5: head right corner
        makeVertex(0.0f, tipY),                   // 6: arrow tip
    };

    std::vector<GLuint> indices = {
        0, 1, 2,  // tail rect - bottom triangle
        1, 3, 2,  // tail rect - top triangle
        4, 5, 6,  // arrowhead triangle
    };

    m_arrowMesh = std::make_unique<Mesh>(vertices, indices);
}

void TileGridEditor::ensureSelectionMesh() {
    if (!m_grid || m_selectionMesh) {
        return;
    }

    const float tileSize = m_grid->getTileSize();
    const float halfSize = tileSize * 0.5f;
    const float height = tileSize;
    const float lineWidth = tileSize * 0.06f;

    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;

    // Helper to add a face outline (4 edges as thin quads)
    auto addFaceOutline = [&](const glm::vec3& corner0, const glm::vec3& corner1, 
                               const glm::vec3& corner2, const glm::vec3& corner3,
                               const glm::vec3& normal) {
        // Edge 0-1
        GLuint base = static_cast<GLuint>(vertices.size());
        glm::vec3 dir01 = glm::normalize(corner1 - corner0);
        glm::vec3 perp01 = glm::cross(normal, dir01) * lineWidth * 0.5f;
        vertices.push_back({corner0 - perp01, normal, {0.0f, 0.0f}});
        vertices.push_back({corner0 + perp01, normal, {0.0f, 1.0f}});
        vertices.push_back({corner1 + perp01, normal, {1.0f, 1.0f}});
        vertices.push_back({corner1 - perp01, normal, {1.0f, 0.0f}});
        indices.insert(indices.end(), {base, base+1, base+2, base+2, base+3, base});

        // Edge 1-2
        base = static_cast<GLuint>(vertices.size());
        glm::vec3 dir12 = glm::normalize(corner2 - corner1);
        glm::vec3 perp12 = glm::cross(normal, dir12) * lineWidth * 0.5f;
        vertices.push_back({corner1 - perp12, normal, {0.0f, 0.0f}});
        vertices.push_back({corner1 + perp12, normal, {0.0f, 1.0f}});
        vertices.push_back({corner2 + perp12, normal, {1.0f, 1.0f}});
        vertices.push_back({corner2 - perp12, normal, {1.0f, 0.0f}});
        indices.insert(indices.end(), {base, base+1, base+2, base+2, base+3, base});

        // Edge 2-3
        base = static_cast<GLuint>(vertices.size());
        glm::vec3 dir23 = glm::normalize(corner3 - corner2);
        glm::vec3 perp23 = glm::cross(normal, dir23) * lineWidth * 0.5f;
        vertices.push_back({corner2 - perp23, normal, {0.0f, 0.0f}});
        vertices.push_back({corner2 + perp23, normal, {0.0f, 1.0f}});
        vertices.push_back({corner3 + perp23, normal, {1.0f, 1.0f}});
        vertices.push_back({corner3 - perp23, normal, {1.0f, 0.0f}});
        indices.insert(indices.end(), {base, base+1, base+2, base+2, base+3, base});

        // Edge 3-0
        base = static_cast<GLuint>(vertices.size());
        glm::vec3 dir30 = glm::normalize(corner0 - corner3);
        glm::vec3 perp30 = glm::cross(normal, dir30) * lineWidth * 0.5f;
        vertices.push_back({corner3 - perp30, normal, {0.0f, 0.0f}});
        vertices.push_back({corner3 + perp30, normal, {0.0f, 1.0f}});
        vertices.push_back({corner0 + perp30, normal, {1.0f, 1.0f}});
        vertices.push_back({corner0 - perp30, normal, {1.0f, 0.0f}});
        indices.insert(indices.end(), {base, base+1, base+2, base+2, base+3, base});
    };

    // Define the 8 corners of the cube
    glm::vec3 v0(-halfSize, -halfSize, 0.0f);      // bottom front left
    glm::vec3 v1( halfSize, -halfSize, 0.0f);      // bottom front right
    glm::vec3 v2( halfSize,  halfSize, 0.0f);      // bottom back right
    glm::vec3 v3(-halfSize,  halfSize, 0.0f);      // bottom back left
    glm::vec3 v4(-halfSize, -halfSize, height);    // top front left
    glm::vec3 v5( halfSize, -halfSize, height);    // top front right
    glm::vec3 v6( halfSize,  halfSize, height);    // top back right
    glm::vec3 v7(-halfSize,  halfSize, height);    // top back left

    // Top face (Z+)
    addFaceOutline(v4, v5, v6, v7, glm::vec3(0.0f, 0.0f, 1.0f));
    // Bottom face (Z-)
    addFaceOutline(v0, v3, v2, v1, glm::vec3(0.0f, 0.0f, -1.0f));
    // Front face (Y-)
    addFaceOutline(v0, v1, v5, v4, glm::vec3(0.0f, -1.0f, 0.0f));
    // Back face (Y+)
    addFaceOutline(v2, v3, v7, v6, glm::vec3(0.0f, 1.0f, 0.0f));
    // Left face (X-)
    addFaceOutline(v3, v0, v4, v7, glm::vec3(-1.0f, 0.0f, 0.0f));
    // Right face (X+)
    addFaceOutline(v1, v2, v6, v5, glm::vec3(1.0f, 0.0f, 0.0f));

    m_selectionMesh = std::make_unique<Mesh>(vertices, indices);
    auto selectionTexture = std::make_shared<Texture>();
    selectionTexture->createSolidColor(255, 255, 255, 128);
    m_selectionMesh->setTexture(selectionTexture);
}

void TileGridEditor::refreshCursorColor() {
    if (m_brush == BrushType::Vehicle) {
        if (m_uiVehicleState.removeMode) {
            m_cursorColor = m_uiVehicleState.cursorHasVehicle ? glm::vec3(0.9f, 0.6f, 0.2f)
                                                              : glm::vec3(0.6f, 0.6f, 0.6f);
        } else {
            const VehiclePlacementStatus status = evaluateVehiclePlacement(m_cursor);
            m_cursorColor = (status == VehiclePlacementStatus::Valid) ? glm::vec3(0.3f, 0.3f, 0.9f)
                                                                      : glm::vec3(0.9f, 0.2f, 0.2f);
        }
        return;
    }

    switch (m_brush) {
        case BrushType::Grass:
            m_cursorColor = glm::vec3(0.3f, 0.9f, 0.3f);
            break;
        case BrushType::Road:
            m_cursorColor = glm::vec3(0.9f, 0.9f, 0.2f);
            break;
        case BrushType::Sidewalk:
            m_cursorColor = glm::vec3(0.7f, 0.7f, 0.7f);
            break;
        case BrushType::Empty:
            m_cursorColor = glm::vec3(0.9f, 0.3f, 0.3f);
            break;
        case BrushType::Vehicle:
            m_cursorColor = glm::vec3(0.3f, 0.3f, 0.9f);
            break;
        case BrushType::PlayerSpawn:
            m_cursorColor = glm::vec3(0.2f, 0.8f, 1.0f);
            break;
    }
}

void TileGridEditor::syncPendingGridSizeFromGrid() {
    if (m_grid) {
        m_pendingGridSize = m_grid->getGridSize();
    } else {
        m_pendingGridSize = glm::ivec3(0);
    }
}

void TileGridEditor::announceCursor() {
    if (!m_grid) {
        return;
    }

    if (m_cursor == m_lastAnnouncedCursor) {
        return;
    }

    m_lastAnnouncedCursor = m_cursor;
    const Tile* tile = m_grid->getTile(m_cursor);
    if (!tile) {
        std::cout << "Cursor at (" << m_cursor.x << ", " << m_cursor.y << ", " << m_cursor.z << ")" << std::endl;
        return;
    }

    const TopSurfaceData& top = tile->getTopSurface();
    std::cout << "Cursor (" << m_cursor.x << ", " << m_cursor.y << ", " << m_cursor.z << ")"
              << " top=" << (top.solid ? "solid" : "empty");
    if (!top.texturePath.empty()) {
        std::cout << " texture=" << top.texturePath;
    }
    if (top.carDirection != CarDirection::None) {
        std::cout << " car=" << carDirectionToString(top.carDirection);
    }
    if (top.sidewalkDirection != SidewalkDirection::None) {
        std::cout << " sidewalk=" << sidewalkDirectionToString(top.sidewalkDirection);
    }

    bool anyWalls = false;
    for (int i = 0; i < 4; ++i) {
        const WallDirection dir = static_cast<WallDirection>(i);
        const WallData& wall = tile->getWall(dir);
        if (!wall.walkable || !wall.texturePath.empty()) {
            if (!anyWalls) {
                std::cout << " walls:";
                anyWalls = true;
            }
            std::cout << ' ' << wallDirectionToString(dir) << '=' << (wall.walkable ? "open" : "blocked");
        }
    }
    if (const auto* spawn = findVehicleSpawn(m_cursor)) {
        std::cout << " vehicle rotation=" << spawn->rotationDegrees
                  << " size=" << spawn->size.x << 'x' << spawn->size.y;
        if (!spawn->texturePath.empty()) {
            std::cout << " texture=" << spawn->texturePath;
        }
    }
    std::cout << std::endl;
}

void TileGridEditor::announceBrush() {
    if (m_brush == m_lastAnnouncedBrush && m_brush != BrushType::Road && m_brush != BrushType::Sidewalk && m_brush != BrushType::Vehicle) {
        return;
    }

    m_lastAnnouncedBrush = m_brush;
    std::cout << "Brush set to ";
    switch (m_brush) {
        case BrushType::Grass:
            std::cout << "grass";
            break;
        case BrushType::Road:
            std::cout << "road (direction=" << carDirectionToString(m_roadDirection) << ")";
            break;
        case BrushType::Sidewalk:
            std::cout << "sidewalk (direction=" << sidewalkDirectionToString(m_sidewalkDirection) << ")";
            break;
        case BrushType::Empty:
            std::cout << "empty";
            break;
        case BrushType::Vehicle:
            std::cout << "vehicle";
            if (m_uiVehicleState.removeMode) {
                std::cout << " (remove)";
            } else {
                std::cout << " (rotation=" << normalizeDegrees(m_uiVehicleState.rotationDegrees)
                          << " size=" << std::max(0.1f, m_uiVehicleState.size.x)
                          << 'x' << std::max(0.1f, m_uiVehicleState.size.y) << ')';
            }
            break;
        case BrushType::PlayerSpawn:
            std::cout << "player spawn (rotation=" << normalizeDegrees(m_uiPlayerSpawnState.rotationDegrees) << ")";
            break;
    }
    std::cout << std::endl;
    refreshCursorColor();
}

void TileGridEditor::printHelp() const {
    std::cout << "Edit mode controls:\n"
              << "  Arrow keys / WASD: move cursor\n"
              << "  Q / E: change layer\n"
              << "  Scroll wheel: change hover layer offset\n"
              << "  Mouse near edge: scroll camera\n"
              << "  1: grass brush\n"
              << "  2: road brush\n"
              << "  3: sidewalk brush\n"
              << "  4: empty brush\n"
              << "  5: vehicle brush\n"
              << "  6: player spawn brush\n"
              << "  R: cycle road/sidewalk direction / rotate vehicle or player\n"
              << "  Delete: clear tile contents at cursor\n"
              << "  I/J/K/L: toggle wall (north/west/south/east)\n"
              << "  Space: apply brush at cursor\n"
              << "  Shift+Space: bucket fill brush or selected prefab on current layer\n"
              << "  Left Click: navigate to tile (or apply prefab if selected)\n"
              << "  Shift+Drag (mouse): select area of tiles\n"
              << "  Ctrl+Click (mouse): toggle individual tile selection\n"
              << "  Ctrl+A: select all\n"
              << "  M: move selected tiles\n"
              << "  Escape: clear selection / cancel move / deselect prefab\n"
              << "  Ctrl+1-9: apply prefab\n"
              << "  Ctrl+N: new level\n"
              << "  Ctrl+O: open level\n"
              << "  Ctrl+S: save level\n"
              << "  Ctrl+Shift+S: save level as\n"
              << "  F1: exit edit mode" << std::endl;
}

void TileGridEditor::drawBrushControls() {
    ImGui::SeparatorText("Brush");

    bool changed = false;
    if (ImGui::RadioButton("Grass", m_brush == BrushType::Grass)) {
        m_brush = BrushType::Grass;
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Road", m_brush == BrushType::Road)) {
        m_brush = BrushType::Road;
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Sidewalk", m_brush == BrushType::Sidewalk)) {
        m_brush = BrushType::Sidewalk;
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Empty", m_brush == BrushType::Empty)) {
        m_brush = BrushType::Empty;
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Vehicle", m_brush == BrushType::Vehicle)) {
        m_brush = BrushType::Vehicle;
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Player", m_brush == BrushType::PlayerSpawn)) {
        m_brush = BrushType::PlayerSpawn;
        changed = true;
    }

    if (changed) {
        announceBrush();
    }
    ImGui::TextDisabled("Shift+Space: bucket fill brush or selected prefab");

    if (m_brush == BrushType::Road) {
        int directionIndex = carDirectionToIndex(m_roadDirection);
        const char* directionLabels[] = {"None", "North", "South", "East", "West", "North-South", "East-West",
                                         "NorthEast", "NorthWest", "SouthEast", "SouthWest",
                                         "NE-SW", "NW-SE"};
        if (ImGui::Combo("Road Direction", &directionIndex, directionLabels, IM_ARRAYSIZE(directionLabels))) {
            m_roadDirection = indexToCarDirection(directionIndex);
            if (m_roadDirection == CarDirection::None) {
                m_roadDirection = CarDirection::SouthNorth;
            }
            announceBrush();
        }
    } else if (m_brush == BrushType::Sidewalk) {
        int directionIndex = sidewalkDirectionToIndex(m_sidewalkDirection);
        const char* directionLabels[] = {"None", "North-South", "East-West", "NE-SW", "NW-SE"};
        if (ImGui::Combo("Sidewalk Direction", &directionIndex, directionLabels, IM_ARRAYSIZE(directionLabels))) {
            m_sidewalkDirection = indexToSidewalkDirection(directionIndex);
            if (m_sidewalkDirection == SidewalkDirection::None) {
                m_sidewalkDirection = SidewalkDirection::NorthSouth;
            }
            announceBrush();
        }
    } else if (m_brush == BrushType::Vehicle) {
        drawVehicleBrushControls();
    } else if (m_brush == BrushType::PlayerSpawn) {
        drawPlayerSpawnBrushControls();
    }
}

void TileGridEditor::drawPrefabControls() {
    ImGui::SeparatorText("Prefabs");

    const bool hasTile = m_uiTileState.hasTile && currentTile() != nullptr;
    ImGui::InputText("Name##prefab", m_newPrefabName.data(), m_newPrefabName.size());
    ImGui::SameLine();
    ImGui::BeginDisabled(!hasTile);
    if (ImGui::Button("Save Prefab")) {
        savePrefab(std::string(m_newPrefabName.data()));
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("Ctrl+1-9 to apply");

    // Show selected prefab and allow deselection
    if (m_selectedPrefabIndex >= 0 && m_selectedPrefabIndex < static_cast<int>(m_prefabs.size())) {
        ImGui::Text("Selected: %s", m_prefabs[static_cast<std::size_t>(m_selectedPrefabIndex)].name.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Deselect")) {
            m_selectedPrefabIndex = -1;
        }
        ImGui::TextDisabled("Click tile to apply prefab, or deselect to navigate");
    } else {
        ImGui::TextDisabled("No prefab selected (click navigates to tile)");
    }

    ImVec2 listSize = ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 6.0f);
    if (ImGui::BeginChild("PrefabList", listSize, true)) {
        if (m_prefabs.empty()) {
            ImGui::TextDisabled("No prefabs saved yet.");
        } else if (ImGui::BeginTable("PrefabTable", 3, ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.6f);
            ImGui::TableSetupColumn("Apply", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Delete", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            for (std::size_t i = 0; i < m_prefabs.size(); ++i) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::PushID(static_cast<int>(i));
                const bool selected = static_cast<int>(i) == m_selectedPrefabIndex;
                if (ImGui::Selectable(m_prefabs[i].name.c_str(), selected)) {
                    m_selectedPrefabIndex = static_cast<int>(i);
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        applyPrefab(i);
                    }
                }

                ImGui::TableSetColumnIndex(1);
                if (ImGui::SmallButton("Apply")) {
                    applyPrefab(i);
                }

                ImGui::TableSetColumnIndex(2);
                if (ImGui::SmallButton("Delete")) {
                    deletePrefab(i);
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
}

void TileGridEditor::drawGridControls() {
    if (!m_grid) {
        return;
    }

    ImGui::SeparatorText("Grid");

    const glm::ivec3 currentSize = m_grid->getGridSize();
    ImGui::Text("Current Size: %d x %d x %d", currentSize.x, currentSize.y, currentSize.z);

    int pendingSize[3] = {m_pendingGridSize.x, m_pendingGridSize.y, m_pendingGridSize.z};
    if (ImGui::InputInt3("New Size", pendingSize)) {
        m_pendingGridSize = glm::ivec3(pendingSize[0], pendingSize[1], pendingSize[2]);
        m_gridResizeError.clear();
    }

    const bool pendingValid = m_pendingGridSize.x > 0 && m_pendingGridSize.y > 0 && m_pendingGridSize.z > 0;
    if (!pendingValid) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "All grid dimensions must be greater than zero.");
    }

    ImGui::BeginDisabled(!pendingValid || m_pendingGridSize == currentSize);
    if (ImGui::Button("Apply Grid Size")) {
        if (pendingValid) {
            if (m_grid->resize(m_pendingGridSize)) {
                syncPendingGridSizeFromGrid();
                clampCursor();
                announceCursor();
                refreshUiStateFromTile();
                refreshCursorColor();
                m_gridResizeError.clear();
                
                // Notify listeners that the level has changed (grid size affects spawns, etc.)
                if (m_levelChangedCallback) {
                    m_levelChangedCallback();
                }
            } else {
                m_gridResizeError = "Failed to resize grid.";
            }
        }
    }
    ImGui::EndDisabled();

    if (!m_gridResizeError.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "%s", m_gridResizeError.c_str());
    }
}

void TileGridEditor::drawTileFaceTabs() {
    ImGui::SeparatorText("Tile Faces");

    if (!m_uiTileState.hasTile) {
        ImGui::TextDisabled("Cursor outside grid bounds.");
        return;
    }

    Tile* tile = currentTile();
    if (!tile) {
        ImGui::TextDisabled("Tile unavailable.");
        return;
    }

    if (ImGui::BeginTabBar("TileFaceTabs")) {
        if (ImGui::BeginTabItem("Top")) {
            drawTopFaceControls(tile);
            ImGui::EndTabItem();
        }
        for (int wallIndex = 0; wallIndex < 4; ++wallIndex) {
            if (ImGui::BeginTabItem(kWallLabels[wallIndex])) {
                drawWallControls(tile, static_cast<WallDirection>(wallIndex), wallIndex);
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
}

void TileGridEditor::drawTopFaceControls(Tile* tile) {
    (void)tile;

    bool solid = m_uiTileState.topSolid;
    if (ImGui::Checkbox("Solid##top", &solid)) {
        m_uiTileState.topSolid = solid;
        if (!solid) {
            m_uiTileState.topCarDirection = CarDirection::None;
            m_uiTileState.topSidewalkDirection = SidewalkDirection::None;
        }
        applyTopSurfaceFromUi();
    }

    if (!m_uiTileState.topSolid) {
        ImGui::TextDisabled("Top surface disabled");
        return;
    }

    int directionIndex = carDirectionToIndex(m_uiTileState.topCarDirection);
    const char* directionLabels[] = {"None", "North", "South", "East", "West", "South-North", "West-East",
                                     "NorthEast", "NorthWest", "SouthEast", "SouthWest",
                                     "NE-SW", "NW-SE"};
    if (ImGui::Combo("Traffic Direction", &directionIndex, directionLabels, IM_ARRAYSIZE(directionLabels))) {
        m_uiTileState.topCarDirection = indexToCarDirection(directionIndex);
        applyTopSurfaceFromUi();
    }

    // Show vehicle spawn weights when this is a road tile
    if (m_uiTileState.topCarDirection != CarDirection::None) {
        ImGui::SeparatorText("Vehicle Spawn Weights");
        ImGui::TextDisabled("Adjust likelihood of each vehicle type spawning on this road");
        
        const auto& config = VehicleConfig::getInstance();
        const auto& definitions = config.getAllDefinitions();
        
        // Ensure spawn weights vector is properly sized
        if (m_uiTileState.spawnWeights.size() != definitions.size()) {
            m_uiTileState.spawnWeights.resize(definitions.size(), 1.0f);
        }
        
        bool weightsChanged = false;
        for (size_t i = 0; i < definitions.size(); ++i) {
            std::string label = definitions[i].displayName + " Weight";
            if (ImGui::SliderFloat(label.c_str(), &m_uiTileState.spawnWeights[i], 0.0f, 5.0f, "%.2f")) {
                weightsChanged = true;
            }
        }
        
        if (weightsChanged) {
            applySpawnWeightsFromUi();
        }
        
        if (ImGui::Button("Reset to Default##SpawnWeights")) {
            for (size_t i = 0; i < m_uiTileState.spawnWeights.size(); ++i) {
                m_uiTileState.spawnWeights[i] = 1.0f;
            }
            applySpawnWeightsFromUi();
        }
    }

    int sidewalkIndex = sidewalkDirectionToIndex(m_uiTileState.topSidewalkDirection);
    const char* sidewalkLabels[] = {"None", "North-South", "East-West", "NE-SW", "NW-SE"};
    if (ImGui::Combo("Sidewalk Direction", &sidewalkIndex, sidewalkLabels, IM_ARRAYSIZE(sidewalkLabels))) {
        m_uiTileState.topSidewalkDirection = indexToSidewalkDirection(sidewalkIndex);
        applyTopSurfaceFromUi();
    }

    if (ImGui::InputText("Texture Path##top", m_uiTileState.topTexture.data(), m_uiTileState.topTexture.size())) {
        applyTopSurfaceFromUi();
    }

    if (drawTexturePicker("top", m_uiTileState.topTexture)) {
        applyTopSurfaceFromUi();
    }

    if (ImGui::Button("Clear Texture##top")) {
        m_uiTileState.topTexture[0] = '\0';
        applyTopSurfaceFromUi();
    }
}

void TileGridEditor::drawWallControls(Tile* tile, WallDirection direction, int wallIndex) {
    (void)tile;

    const std::string checkboxLabel = std::string("Walkable##") + kWallLabels[wallIndex];
    bool walkable = m_uiTileState.wallWalkable[wallIndex];
    if (ImGui::Checkbox(checkboxLabel.c_str(), &walkable)) {
        m_uiTileState.wallWalkable[wallIndex] = walkable;
        if (walkable) {
            m_uiTileState.wallTextures[wallIndex][0] = '\0';
        }
        applyWallFromUi(wallIndex, direction);
    }

    if (m_uiTileState.wallWalkable[wallIndex]) {
        ImGui::TextWrapped("This side is open. Assigning a texture will automatically make it solid.");
    }

    const std::string inputLabel = std::string("Texture Path##") + kWallLabels[wallIndex];
    bool textureChanged = false;
    if (ImGui::InputText(inputLabel.c_str(),
                         m_uiTileState.wallTextures[wallIndex].data(),
                         m_uiTileState.wallTextures[wallIndex].size())) {
        textureChanged = true;
    }

    if (drawTexturePicker(kWallLabels[wallIndex], m_uiTileState.wallTextures[wallIndex])) {
        textureChanged = true;
    }

    if (textureChanged) {
        if (m_uiTileState.wallTextures[wallIndex][0] != '\0' && m_uiTileState.wallWalkable[wallIndex]) {
            m_uiTileState.wallWalkable[wallIndex] = false;
        }
        applyWallFromUi(wallIndex, direction);
    }

    const std::string clearLabel = std::string("Clear Texture##") + kWallLabels[wallIndex];
    if (ImGui::Button(clearLabel.c_str())) {
        m_uiTileState.wallTextures[wallIndex][0] = '\0';
        applyWallFromUi(wallIndex, direction);
    }
}

bool TileGridEditor::drawTexturePicker(const char* label, std::array<char, TextureBufferSize>& buffer) {
    if (m_aliasEntries.empty()) {
        return false;
    }

    std::string preview = findAliasForPath(buffer.data());
    std::string comboId = std::string("Aliases##") + label;
    bool changed = false;

    if (ImGui::BeginCombo(comboId.c_str(), preview.c_str())) {
        bool noneSelected = buffer[0] == '\0';
        if (ImGui::Selectable("None", noneSelected)) {
            if (!noneSelected) {
                buffer[0] = '\0';
                changed = true;
            }
        }
        for (const auto& entry : m_aliasEntries) {
            const bool selected = std::strcmp(buffer.data(), entry.path.c_str()) == 0;
            const std::string display = entry.name + " (" + entry.path + ")";
            ImGui::PushID(entry.name.c_str());
            if (ImGui::Selectable(display.c_str(), selected)) {
                std::snprintf(buffer.data(), buffer.size(), "%s", entry.path.c_str());
                changed = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }

    return changed;
}

std::string TileGridEditor::findAliasForPath(const std::string& path) const {
    if (path.empty()) {
        return "(none)";
    }

    for (const auto& entry : m_aliasEntries) {
        if (entry.path == path) {
            return entry.name;
        }
    }

    return "(manual)";
}

void TileGridEditor::applyTopSurfaceFromUi() {
    Tile* tile = currentTile();
    if (!tile) {
        return;
    }

    if (!m_uiTileState.topSolid) {
        tile->setTopSurface(false, "", CarDirection::None);
        tile->setCarDirection(CarDirection::None);
        tile->setSidewalkDirection(SidewalkDirection::None);
    } else {
        const std::string texturePath = m_uiTileState.topTexture.data();
        tile->setTopSurface(true, texturePath, m_uiTileState.topCarDirection);
        tile->setCarDirection(m_uiTileState.topCarDirection);
        tile->setSidewalkDirection(m_uiTileState.topSidewalkDirection);
    }

    announceCursor();
    refreshUiStateFromTile();
}

void TileGridEditor::applySpawnWeightsFromUi() {
    Tile* tile = currentTile();
    if (!tile) {
        return;
    }

    const auto& config = VehicleConfig::getInstance();
    const auto& definitions = config.getAllDefinitions();
    
    for (size_t i = 0; i < definitions.size() && i < m_uiTileState.spawnWeights.size(); ++i) {
        tile->setVehicleSpawnWeight(definitions[i].id, m_uiTileState.spawnWeights[i]);
    }
}

void TileGridEditor::applyWallFromUi(int wallIndex, WallDirection direction) {
    Tile* tile = currentTile();
    if (!tile) {
        return;
    }

    if (m_uiTileState.wallWalkable[wallIndex]) {
        tile->setWall(direction, true);
        m_uiTileState.wallTextures[wallIndex][0] = '\0';
    } else {
        std::string texturePath = m_uiTileState.wallTextures[wallIndex].data();
        if (texturePath.empty()) {
            texturePath = "assets/textures/wall.png";
            std::snprintf(m_uiTileState.wallTextures[wallIndex].data(),
                         m_uiTileState.wallTextures[wallIndex].size(),
                         "%s",
                         texturePath.c_str());
        }
        tile->setWall(direction, false, texturePath);
    }

    announceCursor();
    refreshUiStateFromTile();
}

void TileGridEditor::applyVehicleBrush() {
    if (!m_grid || !m_levelData) {
        return;
    }

    if (m_uiVehicleState.removeMode) {
        auto& spawns = m_levelData->vehicleSpawns;
        auto it = std::find_if(spawns.begin(), spawns.end(), [&](const VehicleSpawnDefinition& spawn) {
            return spawn.gridPosition == m_cursor;
        });
        if (it != spawns.end()) {
            spawns.erase(it);
            std::cout << "Removed vehicle at (" << m_cursor.x << ", " << m_cursor.y << ", " << m_cursor.z << ")" << std::endl;
        } else {
            std::cout << "No vehicle to remove at (" << m_cursor.x << ", " << m_cursor.y << ", " << m_cursor.z << ")" << std::endl;
        }
        announceCursor();
        refreshUiStateFromTile();
        return;
    }

    const VehiclePlacementStatus placement = evaluateVehiclePlacement(m_cursor);
    if (placement != VehiclePlacementStatus::Valid) {
        if (placement == VehiclePlacementStatus::OutOfBounds) {
            std::cout << "Cannot place vehicle outside of grid bounds at (" << m_cursor.x << ", "
                      << m_cursor.y << ", " << m_cursor.z << ")" << std::endl;
        } else {
            std::cout << "Cannot place vehicle without solid ground at (" << m_cursor.x << ", "
                      << m_cursor.y << ", " << m_cursor.z << ")" << std::endl;
        }
        return;
    }

    // Get vehicle type info from config
    const auto& config = VehicleConfig::getInstance();
    const auto* typeDef = config.getDefinitionByIndex(m_uiVehicleState.vehicleTypeIndex);
    std::string vehicleTypeId = typeDef ? typeDef->id : "sedan";

    VehicleSpawnDefinition spawn;
    spawn.gridPosition = m_cursor;
    spawn.rotationDegrees = normalizeDegrees(m_uiVehicleState.rotationDegrees);
    spawn.size = glm::vec2(
        std::max(0.1f, m_uiVehicleState.size.x),
        std::max(0.1f, m_uiVehicleState.size.y));
    spawn.vehicleTypeId = vehicleTypeId;

    std::string texturePath = m_uiVehicleState.texture.data();
    if (texturePath.empty() && typeDef) {
        texturePath = typeDef->texturePath;
    }
    spawn.texturePath = texturePath;

    auto& spawns = m_levelData->vehicleSpawns;
    auto existing = std::find_if(spawns.begin(), spawns.end(), [&](const VehicleSpawnDefinition& entry) {
        return entry.gridPosition == spawn.gridPosition;
    });
    if (existing != spawns.end()) {
        *existing = spawn;
    } else {
        spawns.push_back(spawn);
    }
    std::cout << "Placed " << vehicleTypeId << " at (" << m_cursor.x << ", " << m_cursor.y << ", " << m_cursor.z
              << ") rotation=" << spawn.rotationDegrees
              << " size=" << spawn.size.x << "x" << spawn.size.y
              << " texture=" << spawn.texturePath << std::endl;

    announceCursor();
    refreshUiStateFromTile();
}

void TileGridEditor::removeVehicleAtCursor() {
    if (!m_grid || !m_levelData) {
        return;
    }
    auto& spawns = m_levelData->vehicleSpawns;
    auto it = std::find_if(spawns.begin(), spawns.end(), [&](const VehicleSpawnDefinition& spawn) {
        return spawn.gridPosition == m_cursor;
    });
    if (it != spawns.end()) {
        spawns.erase(it);
        std::cout << "Removed vehicle at (" << m_cursor.x << ", " << m_cursor.y << ", " << m_cursor.z << ")" << std::endl;
        announceCursor();
        refreshUiStateFromTile();
    }
}

void TileGridEditor::clearTileAtCursor() {
    if (!m_grid) {
        return;
    }

    Tile* tile = m_grid->getTile(m_cursor);
    if (!tile) {
        return;
    }

    // Clear the top surface
    tile->setTopSurface(false, "", CarDirection::None);
    tile->setCarDirection(CarDirection::None);

    // Clear all walls (make them walkable/open)
    for (int i = 0; i < 4; ++i) {
        tile->setWall(static_cast<WallDirection>(i), true);
    }

    // Also remove any vehicle spawn at this position
    if (m_levelData) {
        auto& spawns = m_levelData->vehicleSpawns;
        auto it = std::find_if(spawns.begin(), spawns.end(), [&](const VehicleSpawnDefinition& spawn) {
            return spawn.gridPosition == m_cursor;
        });
        if (it != spawns.end()) {
            spawns.erase(it);
        }

        // Clear player spawn if it's at this position
        if (m_levelData->playerSpawn.isSet && m_levelData->playerSpawn.gridPosition == m_cursor) {
            m_levelData->playerSpawn.isSet = false;
        }
    }

    std::cout << "Cleared tile at (" << m_cursor.x << ", " << m_cursor.y << ", " << m_cursor.z << ")" << std::endl;
    announceCursor();
    refreshUiStateFromTile();
}

void TileGridEditor::refreshUiStateFromTile() {
    m_uiTileState.position = m_cursor;

    const auto& config = VehicleConfig::getInstance();
    
    m_uiVehicleState.cursorHasVehicle = false;
    if (const auto* spawn = findVehicleSpawn(m_cursor)) {
        m_uiVehicleState.cursorHasVehicle = true;
        m_uiVehicleState.rotationDegrees = normalizeDegrees(spawn->rotationDegrees);
        m_uiVehicleState.size = spawn->size;
        m_uiVehicleState.vehicleTypeIndex = config.getIndexById(spawn->vehicleTypeId);
        if (m_uiVehicleState.vehicleTypeIndex < 0) {
            m_uiVehicleState.vehicleTypeIndex = 0;  // Default to first type
        }
        std::snprintf(m_uiVehicleState.texture.data(),
                      m_uiVehicleState.texture.size(),
                      "%s",
                      spawn->texturePath.c_str());
    }
    if (!m_uiVehicleState.cursorHasVehicle && m_uiVehicleState.texture[0] == '\0') {
        // Default texture based on current vehicle type selection
        const auto* typeDef = config.getDefinitionByIndex(m_uiVehicleState.vehicleTypeIndex);
        const char* defaultTexture = typeDef ? typeDef->texturePath.c_str() : "assets/textures/car.png";
        std::snprintf(m_uiVehicleState.texture.data(), m_uiVehicleState.texture.size(), "%s", defaultTexture);
    }

    Tile* tile = currentTile();
    if (!tile) {
        m_uiTileState.hasTile = false;
        m_uiTileState.topSolid = false;
        m_uiTileState.topCarDirection = CarDirection::None;
        m_uiTileState.topSidewalkDirection = SidewalkDirection::None;
        m_uiTileState.topTexture.fill('\0');
        for (auto& flags : m_uiTileState.wallWalkable) {
            flags = true;
        }
        for (auto& texture : m_uiTileState.wallTextures) {
            texture.fill('\0');
        }
        refreshCursorColor();
        return;
    }

    m_uiTileState.hasTile = true;

    const TopSurfaceData& top = tile->getTopSurface();
    m_uiTileState.topSolid = top.solid;
    m_uiTileState.topCarDirection = top.carDirection;
    m_uiTileState.topSidewalkDirection = top.sidewalkDirection;
    std::snprintf(m_uiTileState.topTexture.data(), m_uiTileState.topTexture.size(), "%s", top.texturePath.c_str());
    
    // Load spawn weights for all defined vehicle types
    const auto& definitions = config.getAllDefinitions();
    m_uiTileState.spawnWeights.resize(definitions.size());
    for (size_t i = 0; i < definitions.size(); ++i) {
        m_uiTileState.spawnWeights[i] = top.getSpawnWeight(definitions[i].id);
    }

    for (int i = 0; i < 4; ++i) {
        const WallDirection direction = static_cast<WallDirection>(i);
        const WallData& wall = tile->getWall(direction);
        m_uiTileState.wallWalkable[i] = wall.walkable;
        std::snprintf(m_uiTileState.wallTextures[i].data(),
                      m_uiTileState.wallTextures[i].size(),
                      "%s",
                      wall.texturePath.c_str());
    }

    refreshCursorColor();
}

void TileGridEditor::rebuildAliasList() {
    m_aliasEntries.clear();
    if (!m_grid) {
        return;
    }

    const auto& aliases = TextureManager::instance().getAliases();
    m_aliasEntries.reserve(aliases.size());
    for (const auto& entry : aliases) {
        AliasEntry ae;
        ae.name = entry.first;
        ae.path = entry.second;
        m_aliasEntries.push_back(ae);
    }

    std::sort(m_aliasEntries.begin(), m_aliasEntries.end(), [](const AliasEntry& a, const AliasEntry& b) {
        return a.name < b.name;
    });
}

void TileGridEditor::applyBrush() {
    if (!m_grid) {
        return;
    }

    Tile* tile = m_grid->getTile(m_cursor);
    if (!tile) {
        return;
    }

    switch (m_brush) {
        case BrushType::Grass:
            tile->setTopSurface(true, "assets/textures/grass.png", CarDirection::None);
            tile->setCarDirection(CarDirection::None);
            tile->setSidewalkDirection(SidewalkDirection::None);
            break;
        case BrushType::Road:
            tile->setTopSurface(true, "assets/textures/road.png", m_roadDirection);
            tile->setCarDirection(m_roadDirection);
            tile->setSidewalkDirection(SidewalkDirection::None);
            break;
        case BrushType::Sidewalk:
            tile->setTopSurface(true, "assets/textures/sidewalk.jpeg", CarDirection::None);
            tile->setCarDirection(CarDirection::None);
            tile->setSidewalkDirection(m_sidewalkDirection);
            break;
        case BrushType::Empty:
            tile->setTopSurface(false, "", CarDirection::None);
            tile->setCarDirection(CarDirection::None);
            tile->setSidewalkDirection(SidewalkDirection::None);
            break;
        case BrushType::Vehicle:
            applyVehicleBrush();
            return;
        case BrushType::PlayerSpawn:
            applyPlayerSpawnBrush();
            return;
    }

    announceCursor();
    refreshUiStateFromTile();
}

void TileGridEditor::applyBucketFill() {
    if (!m_grid) {
        return;
    }

    const PrefabEntry* prefab = nullptr;
    if (m_selectedPrefabIndex >= 0 && m_selectedPrefabIndex < static_cast<int>(m_prefabs.size())) {
        const auto& candidate = m_prefabs[static_cast<std::size_t>(m_selectedPrefabIndex)];
        if (candidate.tile) {
            prefab = &candidate;
        }
    }

    if (!prefab && (m_brush == BrushType::Vehicle || m_brush == BrushType::PlayerSpawn)) {
        applyBrush();
        return;
    }

    Tile* seedTile = m_grid->getTile(m_cursor);
    if (!seedTile) {
        return;
    }

    const TopSurfaceData seedTop = seedTile->getTopSurface();
    bool targetSolid = seedTop.solid;
    std::string targetTexture = seedTop.texturePath;
    CarDirection targetDirection = seedTop.carDirection;

    if (!prefab) {
        switch (m_brush) {
            case BrushType::Grass:
                targetSolid = true;
                targetTexture = "assets/textures/grass.png";
                targetDirection = CarDirection::None;
                break;
            case BrushType::Road:
                targetSolid = true;
                targetTexture = "assets/textures/road.png";
                targetDirection = m_roadDirection;
                break;
            case BrushType::Sidewalk:
                targetSolid = true;
                targetTexture = "assets/textures/sidewalk.jpeg";
                targetDirection = CarDirection::None;
                break;
            case BrushType::Empty:
                targetSolid = false;
                targetTexture.clear();
                targetDirection = CarDirection::None;
                break;
            case BrushType::Vehicle:
            case BrushType::PlayerSpawn:
                break;
        }
    }

    if (!prefab) {
        if (seedTop.solid == targetSolid
            && seedTop.texturePath == targetTexture
            && seedTop.carDirection == targetDirection) {
            return;
        }
    }

    auto matchesSeed = [&](const Tile* tile) {
        if (!tile) {
            return false;
        }
        const TopSurfaceData& top = tile->getTopSurface();
        return top.solid == seedTop.solid
            && top.texturePath == seedTop.texturePath
            && top.carDirection == seedTop.carDirection;
    };

    auto applyToTile = [&](Tile* tile) {
        if (prefab) {
            tile->copyFrom(*prefab->tile);
            return;
        }
        switch (m_brush) {
            case BrushType::Grass:
                tile->setTopSurface(true, "assets/textures/grass.png", CarDirection::None);
                tile->setCarDirection(CarDirection::None);
                tile->setSidewalkDirection(SidewalkDirection::None);
                break;
            case BrushType::Road:
                tile->setTopSurface(true, "assets/textures/road.png", m_roadDirection);
                tile->setCarDirection(m_roadDirection);
                tile->setSidewalkDirection(SidewalkDirection::None);
                break;
            case BrushType::Sidewalk:
                tile->setTopSurface(true, "assets/textures/sidewalk.jpeg", CarDirection::None);
                tile->setCarDirection(CarDirection::None);
                tile->setSidewalkDirection(m_sidewalkDirection);
                break;
            case BrushType::Empty:
                tile->setTopSurface(false, "", CarDirection::None);
                tile->setCarDirection(CarDirection::None);
                tile->setSidewalkDirection(SidewalkDirection::None);
                break;
            case BrushType::Vehicle:
            case BrushType::PlayerSpawn:
                break;
        }
    };

    const glm::ivec3 gridSize = m_grid->getGridSize();
    const int layerZ = m_cursor.z;
    const int layerTileCount = gridSize.x * gridSize.y;
    if (layerTileCount <= 0) {
        return;
    }

    std::vector<bool> visited(static_cast<std::size_t>(layerTileCount), false);
    std::vector<glm::ivec3> stack;
    stack.reserve(static_cast<std::size_t>(layerTileCount));

    auto pushIfMatch = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= gridSize.x || y >= gridSize.y) {
            return;
        }
        const int index = y * gridSize.x + x;
        if (visited[static_cast<std::size_t>(index)]) {
            return;
        }
        visited[static_cast<std::size_t>(index)] = true;
        const glm::ivec3 pos(x, y, layerZ);
        Tile* tile = m_grid->getTile(pos);
        if (!matchesSeed(tile)) {
            return;
        }
        stack.push_back(pos);
    };

    pushIfMatch(m_cursor.x, m_cursor.y);
    std::size_t filledCount = 0;

    while (!stack.empty()) {
        const glm::ivec3 pos = stack.back();
        stack.pop_back();

        Tile* tile = m_grid->getTile(pos);
        if (!tile) {
            continue;
        }
        applyToTile(tile);
        ++filledCount;

        pushIfMatch(pos.x + 1, pos.y);
        pushIfMatch(pos.x - 1, pos.y);
        pushIfMatch(pos.x, pos.y + 1);
        pushIfMatch(pos.x, pos.y - 1);
    }

    if (filledCount > 0) {
        std::cout << "Bucket filled " << filledCount << " tiles on layer " << layerZ << std::endl;
    }
    announceCursor();
    refreshUiStateFromTile();
}

void TileGridEditor::savePrefab(const std::string& name) {
    if (!m_grid) {
        return;
    }

    Tile* tile = currentTile();
    if (!tile) {
        return;
    }

    std::string trimmed = trimCopy(name);
    if (trimmed.empty()) {
        trimmed = "Prefab " + std::to_string(m_prefabAutoNameCounter);
    }

    auto existing = std::find_if(m_prefabs.begin(), m_prefabs.end(), [&trimmed](const PrefabEntry& candidate) {
        return candidate.name == trimmed;
    });
    if (existing != m_prefabs.end()) {
        existing->name = trimmed;
        if (!existing->tile) {
            existing->tile = std::make_unique<Tile>(tile->getGridPosition(), tile->getTileSize());
        }
        existing->tile->copyFrom(*tile);
        m_selectedPrefabIndex = static_cast<int>(existing - m_prefabs.begin());
    } else {
        PrefabEntry entry;
        entry.name = trimmed;
        entry.tile = std::make_unique<Tile>(tile->getGridPosition(), tile->getTileSize());
        entry.tile->copyFrom(*tile);
        m_prefabs.push_back(std::move(entry));
        m_selectedPrefabIndex = static_cast<int>(m_prefabs.size()) - 1;
        ++m_prefabAutoNameCounter;
        std::snprintf(m_newPrefabName.data(), m_newPrefabName.size(), "Prefab %d", m_prefabAutoNameCounter);
    }
}

void TileGridEditor::applyPrefab(std::size_t index) {
    if (!m_grid || index >= m_prefabs.size()) {
        return;
    }

    Tile* tile = currentTile();
    if (!tile) {
        return;
    }

    const PrefabEntry& entry = m_prefabs[index];
    if (!entry.tile) {
        return;
    }

    tile->copyFrom(*entry.tile);
    m_selectedPrefabIndex = static_cast<int>(index);

    announceCursor();
    refreshUiStateFromTile();
}

void TileGridEditor::deletePrefab(std::size_t index) {
    if (index >= m_prefabs.size()) {
        return;
    }

    m_prefabs.erase(m_prefabs.begin() + static_cast<long>(index));
    if (m_prefabs.empty()) {
        m_selectedPrefabIndex = -1;
        return;
    }

    if (m_selectedPrefabIndex >= static_cast<int>(m_prefabs.size())) {
        m_selectedPrefabIndex = static_cast<int>(m_prefabs.size()) - 1;
    }
}

void TileGridEditor::toggleWall(WallDirection direction) {
    if (!m_grid) {
        return;
    }

    Tile* tile = m_grid->getTile(m_cursor);
    if (!tile) {
        return;
    }

    const WallData& wall = tile->getWall(direction);
    const bool newWalkable = !wall.walkable;
    std::string texturePath = wall.texturePath;
    if (!newWalkable && texturePath.empty()) {
        texturePath = "assets/textures/wall.png";
    }
    if (newWalkable) {
        texturePath.clear();
    }
    tile->setWall(direction, newWalkable, texturePath);
    announceCursor();
    refreshUiStateFromTile();
}

void TileGridEditor::changeLayer(int delta) {
    if (!m_grid || delta == 0) {
        return;
    }

    m_cursor.z += delta;
    clampCursor();
    announceCursor();
    refreshUiStateFromTile();
}

void TileGridEditor::moveCursor(int dx, int dy) {
    if (!m_grid) {
        return;
    }

    m_cursor.x += dx;
    m_cursor.y += dy;
    clampCursor();
    announceCursor();
    refreshUiStateFromTile();
}

void TileGridEditor::clampCursor() {
    if (!m_grid) {
        return;
    }

    const glm::ivec3& size = m_grid->getGridSize();
    m_cursor.x = std::clamp(m_cursor.x, 0, size.x - 1);
    m_cursor.y = std::clamp(m_cursor.y, 0, size.y - 1);
    m_cursor.z = std::clamp(m_cursor.z, 0, size.z - 1);
}

void TileGridEditor::handleBrushHotkeys(InputManager* input) {
    const bool ctrlDown = input->isKeyDown(GLFW_KEY_LEFT_CONTROL) || input->isKeyDown(GLFW_KEY_RIGHT_CONTROL);
    if (ctrlDown) {
        return;
    }

    if (input->isKeyPressed(GLFW_KEY_1)) {
        m_brush = BrushType::Grass;
        announceBrush();
    }
    if (input->isKeyPressed(GLFW_KEY_2)) {
        m_brush = BrushType::Road;
        announceBrush();
    }
    if (input->isKeyPressed(GLFW_KEY_3)) {
        m_brush = BrushType::Sidewalk;
        announceBrush();
    }
    if (input->isKeyPressed(GLFW_KEY_4)) {
        m_brush = BrushType::Empty;
        announceBrush();
    }
    if (input->isKeyPressed(GLFW_KEY_5)) {
        m_brush = BrushType::Vehicle;
        announceBrush();
    }
    if (input->isKeyPressed(GLFW_KEY_6)) {
        m_brush = BrushType::PlayerSpawn;
        announceBrush();
    }
}

void TileGridEditor::handleWallHotkeys(InputManager* input) {
    if (input->isKeyPressed(GLFW_KEY_I)) {
        toggleWall(WallDirection::South);
    }
    if (input->isKeyPressed(GLFW_KEY_K)) {
        toggleWall(WallDirection::North);
    }
    if (input->isKeyPressed(GLFW_KEY_L)) {
        toggleWall(WallDirection::West);
    }
    if (input->isKeyPressed(GLFW_KEY_J)) {
        toggleWall(WallDirection::East);
    }
}

void TileGridEditor::handlePrefabHotkeys(InputManager* input) {
    if (m_prefabs.empty()) {
        return;
    }

    const bool ctrlDown = input->isKeyDown(GLFW_KEY_LEFT_CONTROL) || input->isKeyDown(GLFW_KEY_RIGHT_CONTROL);
    if (!ctrlDown) {
        return;
    }

    const std::size_t maxHotkeyPrefabs = std::min<std::size_t>(9, m_prefabs.size());
    for (std::size_t i = 0; i < maxHotkeyPrefabs; ++i) {
        int key = GLFW_KEY_1 + static_cast<int>(i);
        if (input->isKeyPressed(key)) {
            applyPrefab(i);
        }
    }
}

void TileGridEditor::handleSaveHotkey(InputManager* input) {
    if (!m_grid || !m_levelData) {
        return;
    }

    const bool ctrl = input->isKeyDown(GLFW_KEY_LEFT_CONTROL) || input->isKeyDown(GLFW_KEY_RIGHT_CONTROL);
    const bool shift = input->isKeyDown(GLFW_KEY_LEFT_SHIFT) || input->isKeyDown(GLFW_KEY_RIGHT_SHIFT);
    
    // Ctrl+N: New Level
    if (ctrl && input->isKeyPressed(GLFW_KEY_N)) {
        m_showNewLevelDialog = true;
        m_newLevelSize = glm::ivec3(16, 16, 4);
        m_newLevelTileSize = 3.0f;
        m_fileDialogError.clear();
        return;
    }
    
    // Ctrl+O: Open Level
    if (ctrl && input->isKeyPressed(GLFW_KEY_O)) {
        m_showLoadLevelDialog = true;
        scanAvailableLevelFiles();
        m_fileDialogError.clear();
        return;
    }
    
    if (ctrl && shift && input->isKeyPressed(GLFW_KEY_S)) {
        // Ctrl+Shift+S: Save As
        m_showSaveAsDialog = true;
        if (m_levelPath.empty()) {
            std::snprintf(m_filePathBuffer.data(), m_filePathBuffer.size(), "assets/levels/new_level.tg");
        } else {
            std::snprintf(m_filePathBuffer.data(), m_filePathBuffer.size(), "%s", m_levelPath.c_str());
        }
        scanAvailableLevelFiles();
        m_fileDialogError.clear();
    } else if (ctrl && input->isKeyPressed(GLFW_KEY_S)) {
        // Ctrl+S: Quick Save (or Save As if no path)
        if (m_levelPath.empty()) {
            m_showSaveAsDialog = true;
            std::snprintf(m_filePathBuffer.data(), m_filePathBuffer.size(), "assets/levels/new_level.tg");
            scanAvailableLevelFiles();
            m_fileDialogError.clear();
        } else if (!LevelSerialization::saveLevel(m_levelPath, *m_grid, *m_levelData)) {
            std::cerr << "Failed to save level to " << m_levelPath << std::endl;
        }
    }
}

void TileGridEditor::drawVehicleBrushControls() {
    ImGui::SeparatorText("Vehicle Settings");

    ImGui::Text("Cursor: %s", m_uiVehicleState.cursorHasVehicle ? "vehicle present" : "empty");

    bool removeMode = m_uiVehicleState.removeMode;
    if (ImGui::Checkbox("Remove Vehicle", &removeMode)) {
        m_uiVehicleState.removeMode = removeMode;
        announceBrush();
    }

    if (!m_uiVehicleState.removeMode) {
        // Vehicle type selection - dynamically built from VehicleConfig
        const auto& config = VehicleConfig::getInstance();
        const auto& definitions = config.getAllDefinitions();
        
        if (!definitions.empty()) {
            // Build labels for combo box
            std::vector<const char*> typeLabels;
            typeLabels.reserve(definitions.size());
            for (const auto& def : definitions) {
                typeLabels.push_back(def.displayName.c_str());
            }
            
            if (ImGui::Combo("Vehicle Type", &m_uiVehicleState.vehicleTypeIndex, typeLabels.data(), static_cast<int>(typeLabels.size()))) {
                // Update texture path and size to match the vehicle type
                const auto* typeDef = config.getDefinitionByIndex(m_uiVehicleState.vehicleTypeIndex);
                if (typeDef) {
                    std::snprintf(m_uiVehicleState.texture.data(), m_uiVehicleState.texture.size(), "%s", typeDef->texturePath.c_str());
                    m_uiVehicleState.size = typeDef->size;
                }
                announceBrush();
            }
        } else {
            ImGui::TextDisabled("No vehicle types loaded");
        }

        float rotation = m_uiVehicleState.rotationDegrees;
        if (ImGui::SliderFloat("Rotation", &rotation, 0.0f, 360.0f, "%.1f deg")) {
            m_uiVehicleState.rotationDegrees = normalizeDegrees(rotation);
            announceBrush();
        }

        // Heading convention: 0°=East, 90°=North, 180°=West, 270°=South
        if (ImGui::Button("North##VehicleRot")) {
            m_uiVehicleState.rotationDegrees = 90.0f;
            announceBrush();
        }
        ImGui::SameLine();
        if (ImGui::Button("East##VehicleRot")) {
            m_uiVehicleState.rotationDegrees = 0.0f;
            announceBrush();
        }
        ImGui::SameLine();
        if (ImGui::Button("South##VehicleRot")) {
            m_uiVehicleState.rotationDegrees = 270.0f;
            announceBrush();
        }
        ImGui::SameLine();
        if (ImGui::Button("West##VehicleRot")) {
            m_uiVehicleState.rotationDegrees = 180.0f;
            announceBrush();
        }

        glm::vec2 size = m_uiVehicleState.size;
        if (ImGui::DragFloat2("Size (W x L)", &size.x, 0.05f, 0.5f, 10.0f, "%.2f")) {
            size.x = std::max(0.1f, size.x);
            size.y = std::max(0.1f, size.y);
            m_uiVehicleState.size = size;
            announceBrush();
        }

        if (ImGui::InputText("Texture Path##vehicle", m_uiVehicleState.texture.data(), m_uiVehicleState.texture.size())) {
            announceBrush();
        }

        if (drawTexturePicker("vehicle", m_uiVehicleState.texture)) {
            announceBrush();
        }

        const char* applyLabel = m_uiVehicleState.cursorHasVehicle ? "Update Vehicle" : "Place Vehicle";
        if (ImGui::Button(applyLabel)) {
            applyVehicleBrush();
        }
        if (m_uiVehicleState.cursorHasVehicle) {
            ImGui::SameLine();
            if (ImGui::Button("Remove Vehicle Here")) {
                bool previousRemove = m_uiVehicleState.removeMode;
                m_uiVehicleState.removeMode = true;
                applyVehicleBrush();
                m_uiVehicleState.removeMode = previousRemove;
            }
        }
    } else {
        if (ImGui::Button("Remove Vehicle")) {
            applyVehicleBrush();
        }
    }
}

void TileGridEditor::drawPlayerSpawnBrushControls() {
    ImGui::SeparatorText("Player Spawn Settings");

    if (m_levelData && m_levelData->playerSpawn.isSet) {
        const auto& spawn = m_levelData->playerSpawn;
        ImGui::Text("Current spawn: (%d, %d, %d)", spawn.gridPosition.x, spawn.gridPosition.y, spawn.gridPosition.z);
        ImGui::Text("Rotation: %.1f°", spawn.rotationDegrees);
    } else {
        ImGui::TextDisabled("No player spawn set");
    }

    ImGui::Separator();

    float rotation = m_uiPlayerSpawnState.rotationDegrees;
    if (ImGui::SliderFloat("Rotation##PlayerSpawn", &rotation, 0.0f, 360.0f, "%.1f deg")) {
        m_uiPlayerSpawnState.rotationDegrees = normalizeDegrees(rotation);
        announceBrush();
    }

    // Heading convention: 0°=East, 90°=North, 180°=West, 270°=South
    if (ImGui::Button("North##PlayerRot")) {
        m_uiPlayerSpawnState.rotationDegrees = 90.0f;
        announceBrush();
    }
    ImGui::SameLine();
    if (ImGui::Button("East##PlayerRot")) {
        m_uiPlayerSpawnState.rotationDegrees = 0.0f;
        announceBrush();
    }
    ImGui::SameLine();
    if (ImGui::Button("South##PlayerRot")) {
        m_uiPlayerSpawnState.rotationDegrees = 270.0f;
        announceBrush();
    }
    ImGui::SameLine();
    if (ImGui::Button("West##PlayerRot")) {
        m_uiPlayerSpawnState.rotationDegrees = 180.0f;
        announceBrush();
    }

    ImGui::Separator();

    if (ImGui::Button("Set Player Spawn Here")) {
        applyPlayerSpawnBrush();
    }

    if (m_levelData && m_levelData->playerSpawn.isSet) {
        ImGui::SameLine();
        if (ImGui::Button("Go to Spawn")) {
            m_cursor = m_levelData->playerSpawn.gridPosition;
            clampCursor();
            announceCursor();
            refreshUiStateFromTile();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Spawn")) {
            m_levelData->playerSpawn.isSet = false;
            std::cout << "Cleared player spawn" << std::endl;
        }
    }
}

void TileGridEditor::applyPlayerSpawnBrush() {
    if (!m_grid || !m_levelData) {
        return;
    }

    // Check if cursor position is valid and has ground support
    if (!m_grid->isValidPosition(m_cursor)) {
        std::cout << "Cannot place player spawn outside of grid bounds" << std::endl;
        return;
    }

    const Tile* tile = m_grid->getTile(m_cursor);
    if (!tile || !tile->isTopSolid()) {
        std::cout << "Cannot place player spawn without solid ground at (" 
                  << m_cursor.x << ", " << m_cursor.y << ", " << m_cursor.z << ")" << std::endl;
        return;
    }

    m_levelData->playerSpawn.gridPosition = m_cursor;
    m_levelData->playerSpawn.rotationDegrees = normalizeDegrees(m_uiPlayerSpawnState.rotationDegrees);
    m_levelData->playerSpawn.isSet = true;

    std::cout << "Set player spawn at (" << m_cursor.x << ", " << m_cursor.y << ", " << m_cursor.z 
              << ") rotation=" << m_levelData->playerSpawn.rotationDegrees << std::endl;
}

void TileGridEditor::ensurePlayerSpawnMesh() {
    if (!m_grid || m_playerSpawnMesh) {
        return;
    }

    const float tileSize = m_grid->getTileSize();
    const float size = tileSize * 0.4f;
    const float height = tileSize * 0.1f;

    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;

    // Create a simple arrow/triangle pointing in the forward direction (positive Y)
    // This represents the player spawn position and facing direction
    
    // Arrow body (pointing up in Y direction)
    const float arrowLength = size * 0.8f;
    const float arrowWidth = size * 0.3f;
    const float headWidth = size * 0.5f;
    const float headLength = size * 0.4f;
    
    const glm::vec3 normal(0.0f, 0.0f, 1.0f);
    
    // Arrow tail (rectangle part)
    vertices.push_back({{-arrowWidth, -size * 0.5f, height}, normal, {0.0f, 0.0f}});       // 0
    vertices.push_back({{ arrowWidth, -size * 0.5f, height}, normal, {1.0f, 0.0f}});       // 1
    vertices.push_back({{ arrowWidth, arrowLength - headLength, height}, normal, {1.0f, 0.5f}}); // 2
    vertices.push_back({{-arrowWidth, arrowLength - headLength, height}, normal, {0.0f, 0.5f}}); // 3
    
    // Arrow head (triangle)
    vertices.push_back({{-headWidth, arrowLength - headLength, height}, normal, {0.0f, 0.5f}});  // 4
    vertices.push_back({{ headWidth, arrowLength - headLength, height}, normal, {1.0f, 0.5f}});  // 5
    vertices.push_back({{ 0.0f, size * 0.5f, height}, normal, {0.5f, 1.0f}});                    // 6
    
    // Tail rectangle
    indices.insert(indices.end(), {0, 1, 2, 2, 3, 0});
    // Arrow head triangle
    indices.insert(indices.end(), {4, 5, 6});

    m_playerSpawnMesh = std::make_unique<Mesh>(vertices, indices);
    auto spawnTexture = std::make_shared<Texture>();
    spawnTexture->createSolidColor(255, 255, 255, 200);
    m_playerSpawnMesh->setTexture(spawnTexture);
}

// Selection methods

void TileGridEditor::clearSelection() {
    m_selectedTiles.clear();
    m_isSelecting = false;
    m_moveMode = false;
}

void TileGridEditor::addToSelection(const glm::ivec3& pos) {
    if (!m_grid || !m_grid->isValidPosition(pos)) {
        return;
    }
    if (!isSelected(pos)) {
        m_selectedTiles.push_back(pos);
    }
}

void TileGridEditor::removeFromSelection(const glm::ivec3& pos) {
    auto it = std::find(m_selectedTiles.begin(), m_selectedTiles.end(), pos);
    if (it != m_selectedTiles.end()) {
        m_selectedTiles.erase(it);
    }
}

bool TileGridEditor::isSelected(const glm::ivec3& pos) const {
    return std::find(m_selectedTiles.begin(), m_selectedTiles.end(), pos) != m_selectedTiles.end();
}

void TileGridEditor::selectArea(const glm::ivec3& start, const glm::ivec3& end) {
    if (!m_grid) {
        return;
    }

    const glm::ivec3 minPos = glm::min(start, end);
    const glm::ivec3 maxPos = glm::max(start, end);

    for (int z = minPos.z; z <= maxPos.z; ++z) {
        for (int y = minPos.y; y <= maxPos.y; ++y) {
            for (int x = minPos.x; x <= maxPos.x; ++x) {
                const glm::ivec3 pos(x, y, z);
                if (m_grid->isValidPosition(pos)) {
                    addToSelection(pos);
                }
            }
        }
    }

    std::cout << "Selected " << m_selectedTiles.size() << " tiles" << std::endl;
}

void TileGridEditor::selectAll() {
    if (!m_grid) {
        return;
    }

    clearSelection();
    const glm::ivec3& size = m_grid->getGridSize();
    for (int z = 0; z < size.z; ++z) {
        for (int y = 0; y < size.y; ++y) {
            for (int x = 0; x < size.x; ++x) {
                m_selectedTiles.push_back(glm::ivec3(x, y, z));
            }
        }
    }
    std::cout << "Selected all " << m_selectedTiles.size() << " tiles" << std::endl;
}

void TileGridEditor::handleMouseSelection(InputManager* input) {
    if (!m_grid) {
        return;
    }

    const bool shiftDown = input->isKeyDown(GLFW_KEY_LEFT_SHIFT) || input->isKeyDown(GLFW_KEY_RIGHT_SHIFT);
    const bool ctrlDown = input->isKeyDown(GLFW_KEY_LEFT_CONTROL) || input->isKeyDown(GLFW_KEY_RIGHT_CONTROL);

    // Get tile at mouse cursor position
    glm::ivec3 mouseTile;
    const bool hasMouseTile = getTileAtScreenPosition(input->getMouseX(), input->getMouseY(), mouseTile);

    if (!hasMouseTile) {
        return; // Mouse not pointing at any valid tile
    }

    // Start selection with Shift+Left Mouse Button
    if (shiftDown && input->isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        m_isSelecting = true;
        m_selectionStart = mouseTile;
        m_selectionEnd = mouseTile;
        if (!ctrlDown) {
            clearSelection();
        }
    }

    // Update selection area while dragging (Shift must still be held)
    if (m_isSelecting && shiftDown && input->isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT)) {
        m_selectionEnd = mouseTile;
    }

    // Complete selection on release or when Shift is released
    if (m_isSelecting && (!input->isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT) || !shiftDown)) {
        if (input->isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT) || 
            m_selectionStart != m_selectionEnd) {
            selectArea(m_selectionStart, m_selectionEnd);
        }
        m_isSelecting = false;
    }

    // Single tile selection with Ctrl+Click (not during Shift selection)
    if (ctrlDown && !shiftDown && input->isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT) && !m_isSelecting) {
        if (isSelected(mouseTile)) {
            removeFromSelection(mouseTile);
            std::cout << "Removed tile (" << mouseTile.x << ", " << mouseTile.y << ", " << mouseTile.z 
                      << ") from selection. Total: " << m_selectedTiles.size() << std::endl;
        } else {
            addToSelection(mouseTile);
            std::cout << "Added tile (" << mouseTile.x << ", " << mouseTile.y << ", " << mouseTile.z 
                      << ") to selection. Total: " << m_selectedTiles.size() << std::endl;
        }
    }
}

void TileGridEditor::startMove() {
    if (m_selectedTiles.empty()) {
        std::cout << "No tiles selected to move" << std::endl;
        return;
    }

    m_moveMode = true;
    m_moveOffset = glm::ivec3(0);
    std::cout << "Move mode activated. Use arrow keys to move, Enter to apply, Escape to cancel" << std::endl;
}

void TileGridEditor::applyMove(const glm::ivec3& offset) {
    if (!m_grid || m_selectedTiles.empty() || offset == glm::ivec3(0)) {
        return;
    }

    // Store original tiles data
    std::vector<std::pair<glm::ivec3, std::unique_ptr<Tile>>> tileData;
    for (const auto& pos : m_selectedTiles) {
        Tile* tile = m_grid->getTile(pos);
        if (tile) {
            auto copy = std::make_unique<Tile>(pos, tile->getTileSize());
            copy->copyFrom(*tile);
            tileData.push_back({pos, std::move(copy)});
        }
    }

    // Clear original tiles
    for (const auto& pos : m_selectedTiles) {
        Tile* tile = m_grid->getTile(pos);
        if (tile) {
            tile->setTopSurface(false, "", CarDirection::None);
            for (int i = 0; i < 4; ++i) {
                tile->setWall(static_cast<WallDirection>(i), true);
            }
        }
    }

    // Apply tiles to new positions
    std::vector<glm::ivec3> newSelection;
    for (const auto& [oldPos, tilePtr] : tileData) {
        const glm::ivec3 newPos = oldPos + offset;
        if (m_grid->isValidPosition(newPos)) {
            Tile* destTile = m_grid->getTile(newPos);
            if (destTile) {
                destTile->copyFrom(*tilePtr);
                newSelection.push_back(newPos);
            }
        }
    }

    m_selectedTiles = newSelection;
    m_moveMode = false;
    m_moveOffset = glm::ivec3(0);

    std::cout << "Moved " << newSelection.size() << " tiles" << std::endl;
}

void TileGridEditor::renderSelection(Renderer* renderer) {
    if (!m_grid || !renderer || !m_selectionMesh) {
        return;
    }

    const float tileSize = m_grid->getTileSize();
    const float offset = tileSize * 0.04f;

    // Render selection preview during drag
    if (m_isSelecting) {
        const glm::ivec3 minPos = glm::min(m_selectionStart, m_selectionEnd);
        const glm::ivec3 maxPos = glm::max(m_selectionStart, m_selectionEnd);

        for (int z = minPos.z; z <= maxPos.z; ++z) {
            for (int y = minPos.y; y <= maxPos.y; ++y) {
                for (int x = minPos.x; x <= maxPos.x; ++x) {
                    const glm::ivec3 pos(x, y, z);
                    if (m_grid->isValidPosition(pos)) {
                        const glm::vec3 worldPos = m_grid->gridToWorld(pos);
                        glm::mat4 model = glm::translate(glm::mat4(1.0f), worldPos + glm::vec3(0.0f, 0.0f, offset));
                        renderer->renderMesh(*m_selectionMesh, model, "model", glm::vec3(1.0f, 1.0f, 0.5f));
                    }
                }
            }
        }
    }

    // Render selected tiles
    glm::vec3 color = m_moveMode ? glm::vec3(0.9f, 0.5f, 0.2f) : m_selectionColor;
    for (const auto& pos : m_selectedTiles) {
        glm::vec3 worldPos = m_grid->gridToWorld(pos);
        if (m_moveMode) {
            worldPos += glm::vec3(m_moveOffset.x * tileSize, m_moveOffset.y * tileSize, m_moveOffset.z * tileSize);
        }
        glm::mat4 model = glm::translate(glm::mat4(1.0f), worldPos + glm::vec3(0.0f, 0.0f, offset));
        renderer->renderMesh(*m_selectionMesh, model, "model", color);
    }
}

void TileGridEditor::drawSelectionControls() {
    ImGui::SeparatorText("Selection");

    ImGui::Text("Selected: %zu tiles", m_selectedTiles.size());

    if (ImGui::Button("Select All")) {
        selectAll();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Selection")) {
        clearSelection();
    }

    if (m_selectedTiles.empty()) {
        ImGui::TextDisabled("Mouse: Shift+Drag to select area");
        ImGui::TextDisabled("Mouse: Ctrl+Click to toggle selection");
        return;
    }

    ImGui::Separator();

    if (m_moveMode) {
        ImGui::Text("Move Mode Active");
        ImGui::Text("Offset: (%d, %d, %d)", m_moveOffset.x, m_moveOffset.y, m_moveOffset.z);
        
        if (ImGui::Button("Apply Move")) {
            applyMove(m_moveOffset);
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            m_moveMode = false;
            m_moveOffset = glm::ivec3(0);
        }
    } else {
        if (ImGui::Button("Move Selected")) {
            startMove();
        }
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Batch Edit");

    static bool applyTopSurface = false;
    static bool applyTopSolid = true;
    static std::array<char, 256> applyTopTexture = {};
    static int applyCarDirection = 0;

    ImGui::Checkbox("Modify Top Surface", &applyTopSurface);
    if (applyTopSurface) {
        ImGui::Indent();
        ImGui::Checkbox("Solid##BatchTop", &applyTopSolid);
        if (applyTopSolid) {
            ImGui::InputText("Texture##BatchTop", applyTopTexture.data(), applyTopTexture.size());
            const char* directionLabels[] = {"None", "North", "South", "East", "West", "North-South", "East-West",
                                             "NorthEast", "NorthWest", "SouthEast", "SouthWest",
                                             "NE-SW", "NW-SE"};
            ImGui::Combo("Traffic##Batch", &applyCarDirection, directionLabels, IM_ARRAYSIZE(directionLabels));
        }
        ImGui::Unindent();
    }

    static bool applyWalls = false;
    static bool applyWallWalkable = true;
    static std::array<char, 256> applyWallTexture = {};

    ImGui::Checkbox("Modify All Walls", &applyWalls);
    if (applyWalls) {
        ImGui::Indent();
        ImGui::Checkbox("Walkable##BatchWall", &applyWallWalkable);
        if (!applyWallWalkable) {
            ImGui::InputText("Texture##BatchWall", applyWallTexture.data(), applyWallTexture.size());
        }
        ImGui::Unindent();
    }

    if (ImGui::Button("Apply to Selection")) {
        for (const auto& pos : m_selectedTiles) {
            Tile* tile = m_grid->getTile(pos);
            if (!tile) continue;

            if (applyTopSurface) {
                if (applyTopSolid) {
                    tile->setTopSurface(true, std::string(applyTopTexture.data()), indexToCarDirection(applyCarDirection));
                } else {
                    tile->setTopSurface(false, "", CarDirection::None);
                }
            }

            if (applyWalls) {
                std::string wallTexture = applyWallWalkable ? "" : std::string(applyWallTexture.data());
                for (int i = 0; i < 4; ++i) {
                    tile->setWall(static_cast<WallDirection>(i), applyWallWalkable, wallTexture);
                }
            }
        }
        std::cout << "Applied batch edits to " << m_selectedTiles.size() << " tiles" << std::endl;
    }
}

bool TileGridEditor::getTileAtScreenPosition(double mouseX, double mouseY, glm::ivec3& outTilePos) const {
    if (!m_grid || !m_window || !m_renderer) {
        return false;
    }

    const int windowWidth = m_window->getWidth();
    const int windowHeight = m_window->getHeight();
    const glm::ivec3& gridSize = m_grid->getGridSize();
    const float tileSize = m_grid->getTileSize();

    // Debug output (only log occasionally to avoid spam)
    static int debugCounter = 0;
    if (++debugCounter % 60 == 0) {
        std::cout << "Checking mouse position: (" << mouseX << ", " << mouseY << ")" << std::endl;
    }

    // Check layers from top to bottom to find the first valid tile
    // Also track the bottom layer position in case we don't find any solid tiles
    glm::ivec3 bottomLayerPos(-1);
    bool foundBottomLayerPos = false;
    
    for (int z = gridSize.z - 1; z >= 0; --z) {
        // Calculate the Z height of the top surface of tiles at this layer
        const float planeZ = static_cast<float>(z) * tileSize;
        
        // Use renderer to convert screen position to world position at this Z height
        glm::vec3 worldPos;
        if (!m_renderer->screenToWorldPosition(mouseX, mouseY, windowWidth, windowHeight, planeZ, worldPos)) {
            continue; // Ray doesn't intersect this plane
        }

        // Convert world position to grid coordinates
        // Grid cells are centered at their grid position, so we need to account for tile size
        const float halfTile = tileSize * 0.5f;
        const int gridX = static_cast<int>(std::floor((worldPos.x + halfTile) / tileSize));
        const int gridY = static_cast<int>(std::floor((worldPos.y + halfTile) / tileSize));

        // Check if the calculated grid position is within bounds
        if (gridX < 0 || gridX >= gridSize.x || gridY < 0 || gridY >= gridSize.y) {
            continue; // Outside grid bounds at this layer
        }

        const glm::ivec3 gridPos(gridX, gridY, z);
        const Tile* tile = m_grid->getTile(gridPos);
        
        if (!tile) {
            continue;
        }
        
        // Track the bottom layer position for fallback (allows selecting empty tiles at z=0)
        if (z == 0 && !foundBottomLayerPos) {
            bottomLayerPos = gridPos;
            foundBottomLayerPos = true;
        }
        
        // Check if tile has solid top surface
        if (tile->isTopSolid()) {
            outTilePos = gridPos;
            if (debugCounter % 60 == 0) {
                std::cout << "  Found tile at (" << gridPos.x << ", " << gridPos.y << ", " << gridPos.z 
                          << ") with solid top" << std::endl;
            }
            return true;
        }
        
        // Check if tile has any walls
        for (int i = 0; i < 4; ++i) {
            if (!tile->getWall(static_cast<WallDirection>(i)).walkable) {
                outTilePos = gridPos;
                if (debugCounter % 60 == 0) {
                    std::cout << "  Found tile at (" << gridPos.x << ", " << gridPos.y << ", " << gridPos.z 
                              << ") with walls" << std::endl;
                }
                return true;
            }
        }
    }

    // If no solid tile found, but we have a valid bottom layer position, return that
    // This allows selecting empty tiles at the ground level in the editor
    if (foundBottomLayerPos) {
        outTilePos = bottomLayerPos;
        if (debugCounter % 60 == 0) {
            std::cout << "  Falling back to bottom layer tile at (" << bottomLayerPos.x << ", " 
                      << bottomLayerPos.y << ", " << bottomLayerPos.z << ")" << std::endl;
        }
        return true;
    }

    return false;
}

void TileGridEditor::handleSelectionHotkeys(InputManager* input) {
    const bool ctrlDown = input->isKeyDown(GLFW_KEY_LEFT_CONTROL) || input->isKeyDown(GLFW_KEY_RIGHT_CONTROL);

    // Ctrl+A to select all
    if (ctrlDown && input->isKeyPressed(GLFW_KEY_A)) {
        selectAll();
    }

    // Escape to clear selection, cancel move, or deselect prefab
    if (input->isKeyPressed(GLFW_KEY_ESCAPE)) {
        if (m_moveMode) {
            m_moveMode = false;
            m_moveOffset = glm::ivec3(0);
            std::cout << "Move cancelled" << std::endl;
        } else if (!m_selectedTiles.empty()) {
            clearSelection();
            std::cout << "Selection cleared" << std::endl;
        } else if (m_selectedPrefabIndex >= 0) {
            m_selectedPrefabIndex = -1;
            std::cout << "Prefab deselected" << std::endl;
        }
    }

    // M to start move mode
    if (input->isKeyPressed(GLFW_KEY_M) && !m_selectedTiles.empty() && !m_moveMode) {
        startMove();
    }

    // Arrow keys to adjust move offset in move mode
    if (m_moveMode) {
        if (input->isKeyPressed(GLFW_KEY_UP) || input->isKeyPressed(GLFW_KEY_W)) {
            m_moveOffset.y++;
        }
        if (input->isKeyPressed(GLFW_KEY_DOWN) || input->isKeyPressed(GLFW_KEY_S)) {
            m_moveOffset.y--;
        }
        if (input->isKeyPressed(GLFW_KEY_LEFT) || input->isKeyPressed(GLFW_KEY_A)) {
            m_moveOffset.x--;
        }
        if (input->isKeyPressed(GLFW_KEY_RIGHT) || input->isKeyPressed(GLFW_KEY_D)) {
            m_moveOffset.x++;
        }
        if (input->isKeyPressed(GLFW_KEY_Q)) {
            m_moveOffset.z--;
        }
        if (input->isKeyPressed(GLFW_KEY_E)) {
            m_moveOffset.z++;
        }

        // Enter to apply move
        if (input->isKeyPressed(GLFW_KEY_ENTER)) {
            applyMove(m_moveOffset);
        }
    }
}

// Level management methods

bool TileGridEditor::newLevel(const glm::ivec3& gridSize, float tileSize) {
    if (!m_grid || !m_levelData) {
        return false;
    }

    if (gridSize.x <= 0 || gridSize.y <= 0 || gridSize.z <= 0 || tileSize <= 0.0f) {
        return false;
    }

    // Clear existing level data
    m_levelData->vehicleSpawns.clear();
    m_levelData->playerSpawn = PlayerSpawnDefinition();  // Reset to default (isSet = false)
    
    // Resize the grid if needed
    if (gridSize != m_grid->getGridSize()) {
        if (!m_grid->resize(gridSize)) {
            return false;
        }
    }
    
    // Reset to default state (clears all tiles and fills bottom with grass)
    m_grid->reset();

    // Clear the level path since this is a new unsaved level
    m_levelPath.clear();

    // Reset editor state
    m_cursor = glm::ivec3(0);
    clampCursor();
    clearSelection();
    m_prefabs.clear();
    m_selectedPrefabIndex = -1;
    m_prefabAutoNameCounter = 0;
    std::snprintf(m_newPrefabName.data(), m_newPrefabName.size(), "Prefab %d", m_prefabAutoNameCounter);

    // Reinitialize editor state
    m_cursorMesh.reset();
    m_arrowMesh.reset();
    m_selectionMesh.reset();
    ensureCursorMesh();
    ensureSelectionMesh();
    refreshCursorColor();
    rebuildAliasList();
    refreshUiStateFromTile();
    syncPendingGridSizeFromGrid();

    std::cout << "Created new level with size " << gridSize.x << "x" << gridSize.y << "x" << gridSize.z << std::endl;
    
    // Notify listeners that the level has changed
    if (m_levelChangedCallback) {
        m_levelChangedCallback();
    }
    
    return true;
}

bool TileGridEditor::loadLevel(const std::string& path) {
    if (!m_grid || !m_levelData || path.empty()) {
        return false;
    }

    if (!LevelSerialization::loadLevel(path, *m_grid, *m_levelData)) {
        std::cerr << "Failed to load level from " << path << std::endl;
        return false;
    }

    m_levelPath = path;

    // Reset editor state
    m_cursor = glm::ivec3(0);
    clampCursor();
    clearSelection();
    m_prefabs.clear();
    m_selectedPrefabIndex = -1;
    m_prefabAutoNameCounter = 0;
    std::snprintf(m_newPrefabName.data(), m_newPrefabName.size(), "Prefab %d", m_prefabAutoNameCounter);

    // Reinitialize editor state
    m_cursorMesh.reset();
    m_arrowMesh.reset();
    m_selectionMesh.reset();
    ensureCursorMesh();
    ensureSelectionMesh();
    refreshCursorColor();
    rebuildAliasList();
    refreshUiStateFromTile();
    syncPendingGridSizeFromGrid();

    std::cout << "Loaded level from " << path << std::endl;
    
    // Notify listeners that the level has changed
    if (m_levelChangedCallback) {
        m_levelChangedCallback();
    }
    
    return true;
}

bool TileGridEditor::saveLevel() {
    if (m_levelPath.empty()) {
        return false;
    }
    return saveLevelAs(m_levelPath);
}

bool TileGridEditor::saveLevelAs(const std::string& path) {
    if (!m_grid || !m_levelData || path.empty()) {
        return false;
    }

    // Create directory if it doesn't exist
    std::filesystem::path filePath(path);
    std::filesystem::path parentDir = filePath.parent_path();
    if (!parentDir.empty() && !std::filesystem::exists(parentDir)) {
        try {
            std::filesystem::create_directories(parentDir);
        } catch (const std::exception& e) {
            std::cerr << "Failed to create directory " << parentDir << ": " << e.what() << std::endl;
            return false;
        }
    }

    if (!LevelSerialization::saveLevel(path, *m_grid, *m_levelData)) {
        std::cerr << "Failed to save level to " << path << std::endl;
        return false;
    }

    m_levelPath = path;
    std::cout << "Saved level to " << path << std::endl;
    return true;
}

void TileGridEditor::drawFileManagementControls() {
    ImGui::SeparatorText("Level");

    if (!m_levelPath.empty()) {
        ImGui::Text("File: %s", m_levelPath.c_str());
    } else {
        ImGui::TextDisabled("File: (unsaved)");
    }

    // New Level button
    if (ImGui::Button("New Level")) {
        m_showNewLevelDialog = true;
        m_newLevelSize = glm::ivec3(16, 16, 4);
        m_newLevelTileSize = 3.0f;
        m_fileDialogError.clear();
    }

    ImGui::SameLine();

    // Load Level button
    if (ImGui::Button("Load Level")) {
        m_showLoadLevelDialog = true;
        scanAvailableLevelFiles();
        m_fileDialogError.clear();
    }

    ImGui::SameLine();

    // Save button (disabled if no path set)
    ImGui::BeginDisabled(m_levelPath.empty());
    if (ImGui::Button("Save")) {
        if (!saveLevel()) {
            m_fileDialogError = "Failed to save level";
        }
    }
    ImGui::EndDisabled();

    ImGui::SameLine();

    // Save As button
    if (ImGui::Button("Save As...")) {
        m_showSaveAsDialog = true;
        if (m_levelPath.empty()) {
            std::snprintf(m_filePathBuffer.data(), m_filePathBuffer.size(), "assets/levels/new_level.tg");
        } else {
            std::snprintf(m_filePathBuffer.data(), m_filePathBuffer.size(), "%s", m_levelPath.c_str());
        }
        m_fileDialogError.clear();
    }

    // Show hotkey hints
    ImGui::TextDisabled("Ctrl+N: New | Ctrl+O: Open | Ctrl+S: Save | Ctrl+Shift+S: Save As");
}

void TileGridEditor::drawNewLevelDialog() {
    if (!m_showNewLevelDialog) {
        return;
    }

    ImGui::OpenPopup("New Level##Dialog");

    if (ImGui::BeginPopupModal("New Level##Dialog", &m_showNewLevelDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Create a new level with the following settings:");
        ImGui::Separator();

        int size[3] = {m_newLevelSize.x, m_newLevelSize.y, m_newLevelSize.z};
        if (ImGui::InputInt3("Grid Size (W x H x D)", size)) {
            m_newLevelSize = glm::ivec3(
                std::max(1, size[0]),
                std::max(1, size[1]),
                std::max(1, size[2])
            );
        }

        ImGui::InputFloat("Tile Size", &m_newLevelTileSize, 0.1f, 1.0f, "%.2f");
        m_newLevelTileSize = std::max(0.1f, m_newLevelTileSize);

        ImGui::Separator();

        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Warning: Creating a new level will discard unsaved changes!");

        if (!m_fileDialogError.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", m_fileDialogError.c_str());
        }

        ImGui::Separator();

        if (ImGui::Button("Create", ImVec2(120, 0))) {
            if (newLevel(m_newLevelSize, m_newLevelTileSize)) {
                m_showNewLevelDialog = false;
            } else {
                m_fileDialogError = "Failed to create new level";
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showNewLevelDialog = false;
        }

        ImGui::EndPopup();
    }
}

void TileGridEditor::drawLoadLevelDialog() {
    if (!m_showLoadLevelDialog) {
        return;
    }

    ImGui::OpenPopup("Load Level##Dialog");

    if (ImGui::BeginPopupModal("Load Level##Dialog", &m_showLoadLevelDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Select a level file to load:");
        ImGui::Separator();

        // Manual path input
        ImGui::InputText("Path", m_filePathBuffer.data(), m_filePathBuffer.size());

        ImGui::Separator();

        // List of available level files
        ImGui::Text("Available levels in assets/levels/:");
        
        ImVec2 listSize = ImVec2(400.0f, ImGui::GetTextLineHeightWithSpacing() * 8.0f);
        if (ImGui::BeginChild("LevelFileList", listSize, true)) {
            if (m_availableLevelFiles.empty()) {
                ImGui::TextDisabled("No level files found");
            } else {
                for (const auto& file : m_availableLevelFiles) {
                    if (ImGui::Selectable(file.c_str(), std::strcmp(m_filePathBuffer.data(), file.c_str()) == 0)) {
                        std::snprintf(m_filePathBuffer.data(), m_filePathBuffer.size(), "%s", file.c_str());
                    }
                }
            }
        }
        ImGui::EndChild();

        if (ImGui::Button("Refresh")) {
            scanAvailableLevelFiles();
        }

        ImGui::Separator();

        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Warning: Loading a level will discard unsaved changes!");

        if (!m_fileDialogError.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", m_fileDialogError.c_str());
        }

        ImGui::Separator();

        if (ImGui::Button("Load", ImVec2(120, 0))) {
            std::string path = m_filePathBuffer.data();
            if (path.empty()) {
                m_fileDialogError = "Please enter a file path";
            } else if (loadLevel(path)) {
                m_showLoadLevelDialog = false;
            } else {
                m_fileDialogError = "Failed to load level from: " + path;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showLoadLevelDialog = false;
        }

        ImGui::EndPopup();
    }
}

void TileGridEditor::drawSaveAsDialog() {
    if (!m_showSaveAsDialog) {
        return;
    }

    ImGui::OpenPopup("Save Level As##Dialog");

    if (ImGui::BeginPopupModal("Save Level As##Dialog", &m_showSaveAsDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter the path to save the level:");
        ImGui::Separator();

        ImGui::InputText("Path", m_filePathBuffer.data(), m_filePathBuffer.size());

        ImGui::Separator();

        // List of available level files for reference
        ImGui::Text("Existing levels in assets/levels/:");
        
        ImVec2 listSize = ImVec2(400.0f, ImGui::GetTextLineHeightWithSpacing() * 6.0f);
        if (ImGui::BeginChild("LevelFileListSave", listSize, true)) {
            if (m_availableLevelFiles.empty()) {
                ImGui::TextDisabled("No level files found");
            } else {
                for (const auto& file : m_availableLevelFiles) {
                    if (ImGui::Selectable(file.c_str(), std::strcmp(m_filePathBuffer.data(), file.c_str()) == 0)) {
                        std::snprintf(m_filePathBuffer.data(), m_filePathBuffer.size(), "%s", file.c_str());
                    }
                }
            }
        }
        ImGui::EndChild();

        // Check if file exists and warn
        std::string path = m_filePathBuffer.data();
        if (!path.empty() && std::filesystem::exists(path)) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Warning: This file already exists and will be overwritten!");
        }

        if (!m_fileDialogError.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", m_fileDialogError.c_str());
        }

        ImGui::Separator();

        if (ImGui::Button("Save", ImVec2(120, 0))) {
            if (path.empty()) {
                m_fileDialogError = "Please enter a file path";
            } else if (saveLevelAs(path)) {
                m_showSaveAsDialog = false;
                scanAvailableLevelFiles(); // Refresh the list
            } else {
                m_fileDialogError = "Failed to save level to: " + path;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showSaveAsDialog = false;
        }

        ImGui::EndPopup();
    }
}

void TileGridEditor::scanAvailableLevelFiles() {
    m_availableLevelFiles.clear();
    
    const std::string levelDir = "assets/levels";
    if (!std::filesystem::exists(levelDir)) {
        return;
    }

    try {
        for (const auto& entry : std::filesystem::directory_iterator(levelDir)) {
            if (entry.is_regular_file()) {
                std::string path = entry.path().string();
                std::string ext = entry.path().extension().string();
                // Accept .tg files or files without extension
                if (ext == ".tg" || ext.empty()) {
                    m_availableLevelFiles.push_back(path);
                }
            }
        }
        
        // Sort alphabetically
        std::sort(m_availableLevelFiles.begin(), m_availableLevelFiles.end());
    } catch (const std::exception& e) {
        std::cerr << "Error scanning level files: " << e.what() << std::endl;
    }
}
