#include "TileGridEditor.hpp"

#include "TileGrid.hpp"
#include "Tile.hpp"
#include "Renderer.hpp"
#include "InputManager.hpp"
#include "Mesh.hpp"
#include "Texture.hpp"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace {
constexpr const char* kWallLabels[4] = {"North", "South", "East", "West"};

const char* carDirectionToString(CarDirection dir) {
    switch (dir) {
        case CarDirection::North: return "north";
        case CarDirection::South: return "south";
        case CarDirection::East: return "east";
        case CarDirection::West: return "west";
        case CarDirection::NorthSouth: return "north_south";
        case CarDirection::EastWest: return "east_west";
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
        case CarDirection::NorthSouth: return 5;
        case CarDirection::EastWest: return 6;
    }
    return 0;
}

CarDirection indexToCarDirection(int index) {
    switch (index) {
        case 1: return CarDirection::North;
        case 2: return CarDirection::South;
        case 3: return CarDirection::East;
        case 4: return CarDirection::West;
        case 5: return CarDirection::NorthSouth;
        case 6: return CarDirection::EastWest;
        default: return CarDirection::None;
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
    , m_enabled(false)
    , m_cursor(0)
    , m_lastAnnouncedCursor(
          std::numeric_limits<int>::min(),
          std::numeric_limits<int>::min(),
          std::numeric_limits<int>::min())
    , m_brush(BrushType::Grass)
    , m_lastAnnouncedBrush(BrushType::Empty)
    , m_roadDirection(CarDirection::NorthSouth)
    , m_cursorColor(0.3f, 0.9f, 0.3f)
    , m_helpPrinted(false)
    , m_selectedPrefabIndex(-1)
    , m_prefabAutoNameCounter(1) {
}

TileGridEditor::~TileGridEditor() = default;

void TileGridEditor::initialize(TileGrid* grid) {
    m_grid = grid;
    m_cursorMesh.reset();
    clampCursor();
    ensureCursorMesh();
    refreshCursorColor();
    rebuildAliasList();
    refreshUiStateFromTile();
    m_selectedPrefabIndex = -1;
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

void TileGridEditor::processInput(InputManager* input) {
    if (!m_enabled || !m_grid || !input) {
        return;
    }

    handleSaveHotkey(input);

    ImGuiIO& io = ImGui::GetIO();
    const bool captureKeyboard = io.WantCaptureKeyboard;
    const bool captureMouse = io.WantCaptureMouse;

    if (!captureKeyboard) {
        handleBrushHotkeys(input);
        handleWallHotkeys(input);
        handlePrefabHotkeys(input);

        if (input->isKeyPressed(GLFW_KEY_UP) || input->isKeyPressed(GLFW_KEY_W)) {
            moveCursor(0, -1);
        }
        if (input->isKeyPressed(GLFW_KEY_DOWN) || input->isKeyPressed(GLFW_KEY_S)) {
            moveCursor(0, 1);
        }
        if (input->isKeyPressed(GLFW_KEY_LEFT) || input->isKeyPressed(GLFW_KEY_A)) {
            moveCursor(1, 0);
        }
        if (input->isKeyPressed(GLFW_KEY_RIGHT) || input->isKeyPressed(GLFW_KEY_D)) {
            moveCursor(-1, 0);
        }

        if (input->isKeyPressed(GLFW_KEY_Q)) {
            changeLayer(-1);
        }
        if (input->isKeyPressed(GLFW_KEY_E)) {
            changeLayer(1);
        }

        if (m_brush == BrushType::Road && input->isKeyPressed(GLFW_KEY_R)) {
            switch (m_roadDirection) {
                case CarDirection::NorthSouth:
                    m_roadDirection = CarDirection::EastWest;
                    break;
                case CarDirection::EastWest:
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
                default:
                    m_roadDirection = CarDirection::NorthSouth;
                    break;
            }
            announceBrush();
        }
    }

    const bool applySpace = !captureKeyboard && input->isKeyPressed(GLFW_KEY_SPACE);
    const bool applyClick = !captureMouse && input->isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
    if (applySpace || applyClick) {
        applyBrush();
    }
}

void TileGridEditor::render(Renderer* renderer) {
    if (!m_enabled || !m_grid || !renderer) {
        return;
    }
    ensureCursorMesh();
    if (!m_cursorMesh) {
        return;
    }

    const glm::vec3 base = m_grid->gridToWorld(m_cursor);
    const float offset = m_grid->getTileSize() * 0.02f;
    glm::mat4 model = glm::translate(glm::mat4(1.0f), base + glm::vec3(0.0f, 0.0f, offset));
    renderer->renderMesh(*m_cursorMesh, model, "model", m_cursorColor);
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

    drawBrushControls();
    drawPrefabControls();

    static bool saveErrorPopup = false;
    if (ImGui::Button("Save Level")) {
        if (m_levelPath.empty() || !m_grid->saveToFile(m_levelPath)) {
            saveErrorPopup = true;
        }
    }
    if (saveErrorPopup) {
        ImGui::OpenPopup("Save Level Error");
        saveErrorPopup = false;
    }
    if (ImGui::BeginPopupModal("Save Level Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Unable to save level. Ensure a valid path is configured.");
        if (ImGui::Button("OK")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (!m_levelPath.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", m_levelPath.c_str());
    }

    drawTileFaceTabs();

    ImGui::End();
}

Tile* TileGridEditor::currentTile() {
    return m_grid ? m_grid->getTile(m_cursor) : nullptr;
}

const Tile* TileGridEditor::currentTile() const {
    return m_grid ? m_grid->getTile(m_cursor) : nullptr;
}

void TileGridEditor::ensureCursorMesh() {
    if (!m_grid || m_cursorMesh) {
        return;
    }

    const float tileSize = m_grid->getTileSize();
    const float halfSize = tileSize * 0.5f;
    const float height = tileSize;

    std::vector<Vertex> vertices = {
        {{-halfSize, -halfSize, height}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ halfSize, -halfSize, height}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{ halfSize,  halfSize, height}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-halfSize,  halfSize, height}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    };

    std::vector<GLuint> indices = {
        0, 1, 2,
        2, 3, 0
    };

    m_cursorMesh = std::make_unique<Mesh>(vertices, indices);
    if (!m_cursorTexture) {
        m_cursorTexture = std::make_shared<Texture>();
        m_cursorTexture->createSolidColor(255, 255, 255, 96);
    }
    m_cursorMesh->setTexture(m_cursorTexture);
}

void TileGridEditor::refreshCursorColor() {
    switch (m_brush) {
        case BrushType::Grass:
            m_cursorColor = glm::vec3(0.3f, 0.9f, 0.3f);
            break;
        case BrushType::Road:
            m_cursorColor = glm::vec3(0.9f, 0.9f, 0.2f);
            break;
        case BrushType::Empty:
            m_cursorColor = glm::vec3(0.9f, 0.3f, 0.3f);
            break;
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
    std::cout << std::endl;
}

void TileGridEditor::announceBrush() {
    if (m_brush == m_lastAnnouncedBrush && m_brush != BrushType::Road) {
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
        case BrushType::Empty:
            std::cout << "empty";
            break;
    }
    std::cout << std::endl;
    refreshCursorColor();
}

void TileGridEditor::printHelp() const {
    std::cout << "Edit mode controls:\n"
              << "  Arrow keys / WASD: move cursor\n"
              << "  Q / E: change layer\n"
              << "  1: grass brush\n"
              << "  2: road brush\n"
              << "  3: empty brush\n"
              << "  R: cycle road direction\n"
              << "  I/J/K/L: toggle wall (north/west/south/east)\n"
              << "  Space or Left Click: apply brush\n"
              << "  Ctrl+1-9: apply prefab\n"
              << "  Ctrl+S: save level\n"
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
    if (ImGui::RadioButton("Empty", m_brush == BrushType::Empty)) {
        m_brush = BrushType::Empty;
        changed = true;
    }

    if (changed) {
        announceBrush();
    }

    if (m_brush == BrushType::Road) {
        int directionIndex = carDirectionToIndex(m_roadDirection);
        const char* directionLabels[] = {"None", "North", "South", "East", "West", "North-South", "East-West"};
        if (ImGui::Combo("Road Direction", &directionIndex, directionLabels, IM_ARRAYSIZE(directionLabels))) {
            m_roadDirection = indexToCarDirection(directionIndex);
            if (m_roadDirection == CarDirection::None) {
                m_roadDirection = CarDirection::NorthSouth;
            }
            announceBrush();
        }
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
        }
        applyTopSurfaceFromUi();
    }

    if (!m_uiTileState.topSolid) {
        ImGui::TextDisabled("Top surface disabled");
        return;
    }

    int directionIndex = carDirectionToIndex(m_uiTileState.topCarDirection);
    const char* directionLabels[] = {"None", "North", "South", "East", "West", "North-South", "East-West"};
    if (ImGui::Combo("Traffic Direction", &directionIndex, directionLabels, IM_ARRAYSIZE(directionLabels))) {
        m_uiTileState.topCarDirection = indexToCarDirection(directionIndex);
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
    } else {
        const std::string texturePath = m_uiTileState.topTexture.data();
        tile->setTopSurface(true, texturePath, m_uiTileState.topCarDirection);
        tile->setCarDirection(m_uiTileState.topCarDirection);
    }

    announceCursor();
    refreshUiStateFromTile();
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

void TileGridEditor::refreshUiStateFromTile() {
    m_uiTileState.position = m_cursor;

    Tile* tile = currentTile();
    if (!tile) {
        m_uiTileState.hasTile = false;
        m_uiTileState.topSolid = false;
        m_uiTileState.topCarDirection = CarDirection::None;
        m_uiTileState.topTexture.fill('\0');
        for (auto& flags : m_uiTileState.wallWalkable) {
            flags = true;
        }
        for (auto& texture : m_uiTileState.wallTextures) {
            texture.fill('\0');
        }
        return;
    }

    m_uiTileState.hasTile = true;

    const TopSurfaceData& top = tile->getTopSurface();
    m_uiTileState.topSolid = top.solid;
    m_uiTileState.topCarDirection = top.carDirection;
    std::snprintf(m_uiTileState.topTexture.data(), m_uiTileState.topTexture.size(), "%s", top.texturePath.c_str());

    for (int i = 0; i < 4; ++i) {
        const WallDirection direction = static_cast<WallDirection>(i);
        const WallData& wall = tile->getWall(direction);
        m_uiTileState.wallWalkable[i] = wall.walkable;
        std::snprintf(m_uiTileState.wallTextures[i].data(),
                      m_uiTileState.wallTextures[i].size(),
                      "%s",
                      wall.texturePath.c_str());
    }
}

void TileGridEditor::rebuildAliasList() {
    m_aliasEntries.clear();
    if (!m_grid) {
        return;
    }

    const auto& aliases = m_grid->getTextureAliases();
    m_aliasEntries.reserve(aliases.size());
    for (const auto& entry : aliases) {
        m_aliasEntries.push_back({entry.first, entry.second});
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
            break;
        case BrushType::Road:
            tile->setTopSurface(true, "assets/textures/road.png", m_roadDirection);
            tile->setCarDirection(m_roadDirection);
            break;
        case BrushType::Empty:
            tile->setTopSurface(false, "", CarDirection::None);
            tile->setCarDirection(CarDirection::None);
            break;
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
        m_brush = BrushType::Empty;
        announceBrush();
    }
}

void TileGridEditor::handleWallHotkeys(InputManager* input) {
    if (input->isKeyPressed(GLFW_KEY_I)) {
        toggleWall(WallDirection::North);
    }
    if (input->isKeyPressed(GLFW_KEY_K)) {
        toggleWall(WallDirection::South);
    }
    if (input->isKeyPressed(GLFW_KEY_L)) {
        toggleWall(WallDirection::East);
    }
    if (input->isKeyPressed(GLFW_KEY_J)) {
        toggleWall(WallDirection::West);
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
    if (m_levelPath.empty()) {
        return;
    }

    const bool ctrl = input->isKeyDown(GLFW_KEY_LEFT_CONTROL) || input->isKeyDown(GLFW_KEY_RIGHT_CONTROL);
    if (ctrl && input->isKeyPressed(GLFW_KEY_S)) {
        if (!m_grid->saveToFile(m_levelPath)) {
            std::cerr << "Failed to save tile grid to " << m_levelPath << std::endl;
        }
    }
}
