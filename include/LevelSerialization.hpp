#pragma once

#include <string>

struct LevelData;
class TileGrid;

namespace LevelSerialization {
bool loadLevel(const std::string& filePath, TileGrid& grid, LevelData& data);
bool saveLevel(const std::string& filePath, const TileGrid& grid, const LevelData& data);
}
