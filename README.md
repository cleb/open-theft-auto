# GTA-Style 2.5D Game

A top-down 2.5D game engine inspired by GTA 1 & 2, featuring 2D sprites for cars and characters with 3D tile-based world. Built with C++ and OpenGL for cross-platform compatibility.

## Features

- **2.5D Rendering**: Combines 2D sprites for characters/vehicles with 3D tile-based world
- **Top-down Camera**: Follows the player with smooth movement
- **Cross-platform**: Runs on Linux and Windows
- **Modern OpenGL**: Uses OpenGL 3.3 core profile with shaders
- **Modular Architecture**: Clean separation between engine components

## Architecture

### Core Components
- **Engine**: Main game loop and system management
- **Window**: GLFW window creation and event handling
- **Renderer**: OpenGL rendering system with shader management
- **Camera**: Top-down camera with player following
- **InputManager**: Keyboard and mouse input handling
- **Scene**: Game object management and scene organization

### Game Objects
- **GameObject**: Base class for all game entities
- **Player**: Player character with movement controls
- **Vehicle**: Drivable vehicles (extensible)
- **Tile**: Tile-based world system
- **TileGrid**: Grid management for tile-based world

## Prerequisites

### Linux (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install build-essential cmake
sudo apt install libglfw3-dev libglew-dev libglm-dev libimgui-dev nlohmann-json3-dev
```

### Linux (Fedora/CentOS)
```bash
sudo dnf install gcc-c++ cmake
sudo dnf install glfw-devel glew-devel glm-devel nlohmann-json-devel
```

### Windows
1. Install [Visual Studio 2019 or later](https://visualstudio.microsoft.com/) with C++ support
2. Install [CMake](https://cmake.org/download/)
3. Install [vcpkg](https://github.com/Microsoft/vcpkg) and install dependencies:
   ```cmd
   vcpkg install glfw3 glew glm nlohmann-json
   ```

## Building

### Linux
```bash
# Clone or navigate to the project directory
cd gta-cpp

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build the project
make -j$(nproc)

# Run the game
./bin/GTA_CPP
```

### Windows (Visual Studio)
```cmd
# Open Command Prompt or PowerShell in the project directory
mkdir build
cd build

# Configure with CMake (adjust paths as needed)
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake

# Build with CMake
cmake --build . --config Release

# Run the game
bin\Release\GTA_CPP.exe
```

### Windows (MinGW)
```bash
# In MSYS2 or MinGW terminal
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
mingw32-make -j4
./bin/GTA_CPP.exe
```

## Controls

- **WASD** or **Arrow Keys**: Move player
- **ESC**: Exit game
- **F1**: Toggle map edit mode

### Edit Mode Shortcuts

- **WASD / Arrow Keys**: Move tile cursor
- **Q / E**: Change layer
- **Ctrl + Mouse Wheel**: Zoom camera in/out
- **1 / 2 / 3**: Select grass, road, or empty brush
- **R**: Cycle road direction when road brush active
- **I / J / K / L**: Toggle south / east / north / west walls
- **Space / Left Click**: Apply selected brush
- **Ctrl + S**: Save current level file

### Editor UI

- Press **F1** to open the ImGui-based *Map Editor* panel.
- Use the *Brush* controls to switch between grass, road, or empty brushes.
- The *Tile Faces* tabs expose top and wall settings for the selected tile, including per-face wall textures.
- Texture inputs accept either absolute paths or texture aliases; pick from the alias dropdown for quick selections.
- Wall controls allow toggling walkability and assigning textures per face.

## Project Structure

```
gta-cpp/
├── CMakeLists.txt          # Build configuration
├── README.md               # This file
├── assets/                 # Game assets
│   ├── shaders/           # GLSL shader files
│   ├── textures/          # Texture files
│   └── models/            # 3D model files
├── include/               # Header files
│   ├── Engine.hpp
│   ├── Window.hpp
│   ├── Renderer.hpp
│   ├── Camera.hpp
│   ├── InputManager.hpp
│   ├── Scene.hpp
│   ├── GameObject.hpp
│   ├── Player.hpp
│   ├── Vehicle.hpp
│   ├── Shader.hpp
│   ├── Texture.hpp
│   └── Mesh.hpp
└── src/                   # Source files
    ├── main.cpp
    ├── Engine.cpp
    ├── Window.cpp
    ├── Renderer.cpp
    ├── Camera.cpp
    ├── InputManager.cpp
    ├── Scene.cpp
    ├── GameObject.cpp
    ├── Player.cpp
    ├── Shader.cpp
    ├── Texture.cpp
    └── Mesh.cpp
```

## Development Notes

### Current Implementation Status
- ✅ Core engine architecture
- ✅ Window management and OpenGL context
- ✅ Basic 3D rendering pipeline
- ✅ Top-down camera system
- ✅ Input handling
- ✅ Player movement
- 🚧 Texture loading (placeholder implementation)
- 🚧 Vehicle system (structure ready)
- 🚧 Sprite rendering for 2D objects

### Extending the Engine

#### Adding New Game Objects
1. Inherit from `GameObject`
2. Implement `update()` and `render()` methods
3. Add to scene in `Scene::createTestScene()`

#### Adding Textures
The current texture system uses placeholder textures. To add real texture loading:
1. Add stb_image library to CMakeLists.txt
2. Implement proper texture loading in `Texture::loadFromFile()`

#### Adding Sound
Consider integrating OpenAL or FMOD for audio support.

## Troubleshooting

### Common Build Issues

**Linux: Missing GLFW/GLEW/GLM**
```bash
# Make sure development packages are installed
sudo apt install libglfw3-dev libglew-dev libglm-dev
```

**Windows: Cannot find libraries**
- Ensure vcpkg is properly configured
- Verify CMAKE_TOOLCHAIN_FILE path in cmake command

**Runtime: Shader loading failed**
- Make sure the `assets/` directory is copied to the build output
- Check that shader files exist and have correct syntax

### Performance Tips
- The engine uses VSync by default (limits to 60 FPS)
- Adjust the camera view size in `Renderer::initialize()` for different zoom levels

## License

This project is provided as a learning example. Feel free to use and modify for educational purposes.

## Contributing

This is a scaffold/template project. To contribute:
1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test on both Linux and Windows
5. Submit a pull request

## Future Enhancements

- Vehicle physics and handling
- Collision detection system
- AI pedestrians and traffic
- Mission system
- Save/load game state
- Particle effects
- Audio system
- Network multiplayer support
