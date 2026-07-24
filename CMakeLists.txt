cmake_minimum_required(VERSION 3.16)
project(GTA_CPP)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find required packages
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)

# Find GLM (header-only library)
find_package(glm REQUIRED)

# Find nlohmann_json for vehicle config
find_package(nlohmann_json REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)

# Source files
file(GLOB_RECURSE SOURCES 
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*.c"
)

# Header files
file(GLOB_RECURSE HEADERS 
    "${CMAKE_SOURCE_DIR}/include/*.h"
    "${CMAKE_SOURCE_DIR}/include/*.hpp"
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link libraries
find_path(IMGUI_INCLUDE_DIR imgui.h
    PATHS
        /usr/include/imgui
)

if (NOT IMGUI_INCLUDE_DIR)
    message(FATAL_ERROR "Could not locate ImGui headers. Please ensure libimgui-dev is installed.")
endif()

find_library(IMGUI_LIBRARY imgui
    PATHS
        /usr/lib
        /usr/lib/x86_64-linux-gnu
)

if (NOT IMGUI_LIBRARY)
    message(FATAL_ERROR "Could not locate ImGui library. Please ensure libimgui-dev is installed.")
endif()

find_library(STB_LIBRARY stb
    PATHS
        /usr/lib
        /usr/lib/x86_64-linux-gnu
)

if (NOT STB_LIBRARY)
    message(FATAL_ERROR "Could not locate stb library. Please ensure libstb-dev is installed.")
endif()

target_link_libraries(${PROJECT_NAME} 
    OpenGL::GL
    glfw
    GLEW::GLEW
    glm::glm
    nlohmann_json::nlohmann_json
    ${IMGUI_LIBRARY}
    ${STB_LIBRARY}
)

find_path(IMGUI_BACKEND_INCLUDE_DIR imgui_impl_glfw.h
    PATHS
        /usr/include/imgui/backends
)

if (NOT IMGUI_BACKEND_INCLUDE_DIR)
    message(FATAL_ERROR "Could not locate ImGui backend headers. Please ensure libimgui-dev is installed.")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${IMGUI_INCLUDE_DIR}
    ${IMGUI_BACKEND_INCLUDE_DIR}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLEW)

# Platform-specific settings
if(WIN32)
    # Windows-specific settings
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
    
    # Copy DLLs to output directory (if needed)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set_target_properties(${PROJECT_NAME} PROPERTIES
            VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        )
    endif()
    
elseif(UNIX AND NOT APPLE)
    # Linux-specific settings
    target_link_libraries(${PROJECT_NAME} 
        ${CMAKE_DL_LIBS}
        pthread
    )
endif()

# Copy assets to build directory
file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE 
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4
        $<$<CONFIG:Debug>:/Od /Zi>
        $<$<CONFIG:Release>:/O2 /DNDEBUG>
    )
endif()

# Enable debug symbols in Debug mode
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)

# Unit tests
add_executable(test_traffic_turning tests/test_traffic_turning.cpp)
target_link_libraries(test_traffic_turning glm::glm)
target_include_directories(test_traffic_turning PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(test_heading_conventions tests/test_heading_conventions.cpp)
target_link_libraries(test_heading_conventions glm::glm)
target_include_directories(test_heading_conventions PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(test_curve_math tests/test_curve_math.cpp)
target_link_libraries(test_curve_math glm::glm)
target_include_directories(test_curve_math PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(test_phone_booth_job_sequence tests/test_phone_booth_job_sequence.cpp)
target_include_directories(test_phone_booth_job_sequence PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(test_character_physics
    tests/test_character_physics.cpp
    src/CharacterPhysics.cpp
    src/Collider.cpp
)
target_link_libraries(test_character_physics glm::glm)
target_include_directories(test_character_physics PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(test_tile_fall
    tests/test_tile_fall.cpp
    src/TileGrid.cpp
    src/Tile.cpp
)
target_link_libraries(test_tile_fall glm::glm GLEW::GLEW OpenGL::GL)
target_include_directories(test_tile_fall PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Register tests with CTest
enable_testing()
add_test(NAME heading_conventions COMMAND test_heading_conventions)
add_test(NAME curve_math COMMAND test_curve_math)
add_test(NAME phone_booth_job_sequence COMMAND test_phone_booth_job_sequence)
add_test(NAME character_physics COMMAND test_character_physics)
add_test(NAME tile_fall COMMAND test_tile_fall)
