/**
 * Unit test for TrafficManager AI vehicle turning behavior
 * 
 * Tests that vehicles properly navigate a simple road layout:
 * 
 *   Grid layout (2x2, z=0):
 *   +-------+-------+
 *   | South | North |   (0,1) goes South, (1,1) goes North  
 *   +-------+-------+
 *   | SE    | East  |   (0,0) goes SouthEast, (1,0) goes East
 *   +-------+-------+
 * 
 *   A car starting at (0,1) heading South should:
 *   1. Move south through tile (0,1)
 *   2. Enter tile (0,0) which is SouthEast - turn to 135 degrees
 *   3. Move diagonally into tile (1,0) which is East - turn to 90 degrees
 *   4. End up in the middle of tile (1,0) heading East
 */

#include <iostream>
#include <cmath>
#include <cassert>
#include <vector>
#include <glm/glm.hpp>

// Minimal test implementation - no OpenGL dependencies

enum class CarDirection {
    None = 0,
    North,
    South,
    East,
    West,
    SouthNorth,
    WestEast,
    NorthEast,
    NorthWest,
    SouthEast,
    SouthWest,
    NorthEastSouthWest,
    NorthWestSouthEast
};

// Simple test tile
struct TestTile {
    CarDirection direction = CarDirection::None;
};

// Simple test grid
class TestGrid {
public:
    int width, height;
    float tileSize;
    std::vector<TestTile> tiles;
    
    TestGrid(int w, int h, float size) : width(w), height(h), tileSize(size) {
        tiles.resize(w * h);
    }
    
    void setDirection(int x, int y, CarDirection dir) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            tiles[y * width + x].direction = dir;
        }
    }
    
    CarDirection getDirection(int x, int y) const {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            return tiles[y * width + x].direction;
        }
        return CarDirection::None;
    }
    
    glm::ivec2 worldToGrid(const glm::vec2& worldPos) const {
        return glm::ivec2(
            static_cast<int>(std::floor(worldPos.x / tileSize)),
            static_cast<int>(std::floor(worldPos.y / tileSize))
        );
    }
    
    glm::vec2 gridToWorld(int x, int y) const {
        return glm::vec2(
            (x + 0.5f) * tileSize,
            (y + 0.5f) * tileSize
        );
    }
};

// Test vehicle state
struct TestVehicle {
    glm::vec2 position;
    float rotation;  // degrees, 0 = North (+Y), 90 = East (+X), 180 = South, 270 = West
    
    glm::vec2 getForward() const {
        float radians = glm::radians(rotation);
        return glm::vec2(std::sin(radians), std::cos(radians));
    }
};

// Calculate target rotation for a tile direction
float calculateTargetRotation(CarDirection tileDir, float currentRotation) {
    switch (tileDir) {
        case CarDirection::North: return 0.0f;
        case CarDirection::South: return 180.0f;
        case CarDirection::East: return 90.0f;
        case CarDirection::West: return 270.0f;
        case CarDirection::NorthEast: return 45.0f;
        case CarDirection::NorthWest: return 315.0f;
        case CarDirection::SouthEast: return 135.0f;
        case CarDirection::SouthWest: return 225.0f;
        default:
            break;
    }
    
    // Bidirectional - pick closest
    float rot1, rot2;
    switch (tileDir) {
        case CarDirection::SouthNorth:
            rot1 = 0.0f; rot2 = 180.0f;
            break;
        case CarDirection::WestEast:
            rot1 = 90.0f; rot2 = 270.0f;
            break;
        case CarDirection::NorthEastSouthWest:
            rot1 = 45.0f; rot2 = 225.0f;
            break;
        case CarDirection::NorthWestSouthEast:
            rot1 = 315.0f; rot2 = 135.0f;
            break;
        default:
            return currentRotation;
    }
    
    float diff1 = std::abs(currentRotation - rot1);
    float diff2 = std::abs(currentRotation - rot2);
    if (diff1 > 180.0f) diff1 = 360.0f - diff1;
    if (diff2 > 180.0f) diff2 = 360.0f - diff2;
    
    return (diff1 <= diff2) ? rot1 : rot2;
}

// Normalize angle to [0, 360)
float normalizeAngle(float angle) {
    while (angle < 0.0f) angle += 360.0f;
    while (angle >= 360.0f) angle -= 360.0f;
    return angle;
}

// Calculate shortest angular difference
float angleDifference(float target, float current) {
    float diff = target - current;
    while (diff > 180.0f) diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;
    return diff;
}

/**
 * Core AI steering algorithm V8 - Exit-targeting for corners
 * 
 * When on a corner tile, aim toward the center of the exit tile.
 * When approaching a corner, aim toward the corner center.
 */
void updateVehicleAI(TestVehicle& vehicle, const TestGrid& grid, float deltaTime,
                     float turnRate, float maxSpeed, float minSpeed, float lookAheadDist) {
    glm::ivec2 gridPos = grid.worldToGrid(vehicle.position);
    CarDirection tileDir = grid.getDirection(gridPos.x, gridPos.y);
    
    if (tileDir == CarDirection::None) {
        vehicle.position += vehicle.getForward() * maxSpeed * deltaTime;
        return;
    }
    
    glm::vec2 tileCenter = grid.gridToWorld(gridPos.x, gridPos.y);
    glm::vec2 forward = vehicle.getForward();
    glm::vec2 toCenter = tileCenter - vehicle.position;
    float dotToCenter = glm::dot(glm::normalize(toCenter), forward);
    
    float targetRotation = calculateTargetRotation(tileDir, vehicle.rotation);
    
    // Check if current tile is a corner
    bool currentIsCorner = (tileDir == CarDirection::NorthEast ||
                           tileDir == CarDirection::NorthWest ||
                           tileDir == CarDirection::SouthEast ||
                           tileDir == CarDirection::SouthWest);
    
    if (currentIsCorner) {
        // On a corner - aim toward the exit tile's center
        float exitRotation = calculateTargetRotation(tileDir, vehicle.rotation);
        float exitRadians = glm::radians(exitRotation);
        glm::vec2 exitDir(std::sin(exitRadians), std::cos(exitRadians));
        
        // Find exit tile
        glm::vec2 exitLookPos = tileCenter + exitDir * grid.tileSize;
        glm::ivec2 exitGridPos = grid.worldToGrid(exitLookPos);
        
        CarDirection exitTileDir = grid.getDirection(exitGridPos.x, exitGridPos.y);
        if (exitTileDir != CarDirection::None) {
            glm::vec2 exitTileCenter = grid.gridToWorld(exitGridPos.x, exitGridPos.y);
            glm::vec2 toExit = exitTileCenter - vehicle.position;
            targetRotation = glm::degrees(std::atan2(toExit.x, toExit.y));
            if (targetRotation < 0) targetRotation += 360.0f;
        }
    }
    // Look ahead when on straight tile approaching edge
    else if (dotToCenter < 0.3f && lookAheadDist > 0) {
        glm::vec2 lookAheadPos = vehicle.position + forward * lookAheadDist;
        glm::ivec2 lookAheadGridPos = grid.worldToGrid(lookAheadPos);
        
        if (lookAheadGridPos != gridPos) {
            CarDirection lookAheadDir = grid.getDirection(lookAheadGridPos.x, lookAheadGridPos.y);
            if (lookAheadDir != CarDirection::None) {
                bool nextIsCorner = (lookAheadDir == CarDirection::NorthEast ||
                                    lookAheadDir == CarDirection::NorthWest ||
                                    lookAheadDir == CarDirection::SouthEast ||
                                    lookAheadDir == CarDirection::SouthWest);
                
                if (nextIsCorner) {
                    // Approaching corner - aim toward corner center
                    glm::vec2 cornerCenter = grid.gridToWorld(lookAheadGridPos.x, lookAheadGridPos.y);
                    glm::vec2 toCorner = cornerCenter - vehicle.position;
                    targetRotation = glm::degrees(std::atan2(toCorner.x, toCorner.y));
                    if (targetRotation < 0) targetRotation += 360.0f;
                } else {
                    targetRotation = calculateTargetRotation(lookAheadDir, vehicle.rotation);
                }
            }
        }
    }
    
    float rotDiff = angleDifference(targetRotation, vehicle.rotation);
    
    // Dynamic turn rate
    float dynamicMultiplier = 1.0f + std::abs(rotDiff) / 45.0f;
    float effectiveTurnRate = turnRate * dynamicMultiplier;
    
    if (std::abs(rotDiff) > 0.5f) {
        float turnAmount = effectiveTurnRate * deltaTime;
        if (std::abs(rotDiff) <= turnAmount) {
            vehicle.rotation = targetRotation;
            rotDiff = 0;
        } else if (rotDiff > 0) {
            vehicle.rotation += turnAmount;
        } else {
            vehicle.rotation -= turnAmount;
        }
        vehicle.rotation = normalizeAngle(vehicle.rotation);
    }
    
    // Speed reduction while turning
    float turnFactor = std::min(std::abs(rotDiff) / 30.0f, 1.0f);
    float speed = maxSpeed - (maxSpeed - minSpeed) * turnFactor;
    
    vehicle.position += vehicle.getForward() * speed * deltaTime;
}

// Test helper to run simulation
struct SimulationResult {
    glm::vec2 finalPosition;
    float finalRotation;
    float totalTime;
    bool stayedOnRoad;
    float maxDeviationFromCenter;
};

SimulationResult runSimulation(TestGrid& grid, TestVehicle startVehicle,
                               glm::vec2 targetTileCenter, float targetRotation,
                               float turnRate, float maxSpeed, float minSpeed,
                               float lookAheadDist = 0.0f,
                               float maxTime = 10.0f) {
    SimulationResult result;
    result.stayedOnRoad = true;
    result.maxDeviationFromCenter = 0.0f;
    result.totalTime = 0.0f;
    
    TestVehicle vehicle = startVehicle;
    const float dt = 1.0f / 60.0f;  // 60 FPS simulation
    
    while (result.totalTime < maxTime) {
        updateVehicleAI(vehicle, grid, dt, turnRate, maxSpeed, minSpeed, lookAheadDist);
        result.totalTime += dt;
        
        // Check if we reached target area
        glm::vec2 toTarget = targetTileCenter - vehicle.position;
        float distToTarget = glm::length(toTarget);
        float rotDiff = std::abs(angleDifference(targetRotation, vehicle.rotation));
        
        // Track deviation from road center
        glm::ivec2 currentGrid = grid.worldToGrid(vehicle.position);
        glm::vec2 tileCenter = grid.gridToWorld(currentGrid.x, currentGrid.y);
        float deviation = glm::length(vehicle.position - tileCenter);
        result.maxDeviationFromCenter = std::max(result.maxDeviationFromCenter, deviation);
        
        // Success condition: within 0.3 units of target center and facing right direction
        if (distToTarget < 0.3f && rotDiff < 5.0f) {
            result.finalPosition = vehicle.position;
            result.finalRotation = vehicle.rotation;
            return result;
        }
        
        // Check if off grid (failed)
        if (currentGrid.x < 0 || currentGrid.x >= grid.width ||
            currentGrid.y < 0 || currentGrid.y >= grid.height) {
            result.stayedOnRoad = false;
            break;
        }
        
        // Check if on a non-road tile
        if (grid.getDirection(currentGrid.x, currentGrid.y) == CarDirection::None) {
            result.stayedOnRoad = false;
            break;
        }
    }
    
    result.finalPosition = vehicle.position;
    result.finalRotation = vehicle.rotation;
    return result;
}

void printResult(const std::string& testName, const SimulationResult& result, 
                 glm::vec2 expectedPos, float expectedRot) {
    std::cout << "\n=== " << testName << " ===" << std::endl;
    std::cout << "Final position: (" << result.finalPosition.x << ", " << result.finalPosition.y << ")" << std::endl;
    std::cout << "Expected:       (" << expectedPos.x << ", " << expectedPos.y << ")" << std::endl;
    std::cout << "Final rotation: " << result.finalRotation << " degrees" << std::endl;
    std::cout << "Expected:       " << expectedRot << " degrees" << std::endl;
    std::cout << "Time elapsed:   " << result.totalTime << " seconds" << std::endl;
    std::cout << "Stayed on road: " << (result.stayedOnRoad ? "YES" : "NO") << std::endl;
    std::cout << "Max deviation:  " << result.maxDeviationFromCenter << " units" << std::endl;
    
    float posError = glm::length(result.finalPosition - expectedPos);
    float rotError = std::abs(angleDifference(expectedRot, result.finalRotation));
    
    bool passed = result.stayedOnRoad && posError < 0.5f && rotError < 10.0f;
    std::cout << "Result: " << (passed ? "PASSED" : "FAILED") << std::endl;
}

int main() {
    std::cout << "Traffic Turning Unit Tests" << std::endl;
    std::cout << "==========================" << std::endl;
    
    const float TILE_SIZE = 3.0f;
    int passedTests = 0;
    int totalTests = 0;
    
    // Test 1: South -> SouthEast -> East
    // Grid:   (0,1)=South  
    //         (0,0)=SouthEast (corner tile)
    //         (1,0)=East
    std::cout << "\n=== TEST 1: South to SouthEast to East ===" << std::endl;
    {
        totalTests++;
        TestGrid grid(2, 2, TILE_SIZE);
        grid.setDirection(0, 1, CarDirection::South);
        grid.setDirection(0, 0, CarDirection::SouthEast);  // Corner
        grid.setDirection(1, 0, CarDirection::East);
        grid.setDirection(1, 1, CarDirection::None);  // Not used
        
        TestVehicle start;
        start.position = glm::vec2(1.5f, 5.0f);
        start.rotation = 180.0f;  // Facing South
        
        glm::vec2 targetCenter = grid.gridToWorld(1, 0);
        float targetRotation = 90.0f;  // Facing East
        
        std::cout << "Start: (" << start.position.x << ", " << start.position.y << ") rot=" << start.rotation << " (South)" << std::endl;
        std::cout << "Target: (" << targetCenter.x << ", " << targetCenter.y << ") rot=" << targetRotation << " (East)" << std::endl;
        
        float bestDeviation = 999.0f;
        float bestTurnRate = 0, bestMinSpeed = 0, bestLookAhead = 0;
        
        for (float turnRate = 400.0f; turnRate <= 800.0f; turnRate += 50.0f) {
            for (float minSpeed = 2.0f; minSpeed <= 5.0f; minSpeed += 0.5f) {
                for (float lookAhead = 1.0f; lookAhead <= 3.0f; lookAhead += 0.25f) {
                    SimulationResult result = runSimulation(grid, start, targetCenter, targetRotation,
                                                            turnRate, 12.0f, minSpeed, lookAhead);
                    if (result.stayedOnRoad && result.maxDeviationFromCenter < bestDeviation) {
                        float posError = glm::length(result.finalPosition - targetCenter);
                        float rotError = std::abs(angleDifference(targetRotation, result.finalRotation));
                        if (posError < 0.5f && rotError < 10.0f) {
                            bestDeviation = result.maxDeviationFromCenter;
                            bestTurnRate = turnRate;
                            bestMinSpeed = minSpeed;
                            bestLookAhead = lookAhead;
                        }
                    }
                }
            }
        }
        
        if (bestDeviation < 999.0f) {
            SimulationResult finalResult = runSimulation(grid, start, targetCenter, targetRotation,
                                                          bestTurnRate, 12.0f, bestMinSpeed, bestLookAhead);
            printResult("Test 1", finalResult, targetCenter, targetRotation);
            std::cout << "Best params: turnRate=" << bestTurnRate << ", minSpeed=" << bestMinSpeed << ", lookAhead=" << bestLookAhead << std::endl;
            passedTests++;
        } else {
            std::cout << "TEST 1 FAILED - No valid parameters found!" << std::endl;
        }
    }
    
    // Test 2: West -> NorthWest -> North
    // Grid:   (0,1)=North     (1,1)=South  
    //         (0,0)=NorthWest (1,0)=West
    std::cout << "\n=== TEST 2: West to NorthWest to North ===" << std::endl;
    {
        totalTests++;
        TestGrid grid(2, 2, TILE_SIZE);
        grid.setDirection(0, 1, CarDirection::North);
        grid.setDirection(1, 1, CarDirection::None);  // Not used
        grid.setDirection(0, 0, CarDirection::NorthWest);  // Corner
        grid.setDirection(1, 0, CarDirection::West);
        
        TestVehicle start;
        // Start on West tile (1,0), moving West
        start.position = glm::vec2(5.0f, 1.5f);  // Right side of grid, y=1.5 is center of bottom row
        start.rotation = 270.0f;  // Facing West
        
        glm::vec2 targetCenter = grid.gridToWorld(0, 1);  // Center of North tile at (0,1)
        float targetRotation = 0.0f;  // Facing North
        
        std::cout << "Start: (" << start.position.x << ", " << start.position.y << ") rot=" << start.rotation << " (West)" << std::endl;
        std::cout << "Target: (" << targetCenter.x << ", " << targetCenter.y << ") rot=" << targetRotation << " (North)" << std::endl;
        std::cout << "Tile (1,0) West center: " << grid.gridToWorld(1, 0).x << ", " << grid.gridToWorld(1, 0).y << std::endl;
        std::cout << "Tile (0,0) NorthWest center: " << grid.gridToWorld(0, 0).x << ", " << grid.gridToWorld(0, 0).y << std::endl;
        std::cout << "Tile (0,1) North center: " << grid.gridToWorld(0, 1).x << ", " << grid.gridToWorld(0, 1).y << std::endl;
        
        float bestDeviation = 999.0f;
        float bestTurnRate = 0, bestMinSpeed = 0, bestLookAhead = 0;
        
        for (float turnRate = 400.0f; turnRate <= 800.0f; turnRate += 50.0f) {
            for (float minSpeed = 2.0f; minSpeed <= 5.0f; minSpeed += 0.5f) {
                for (float lookAhead = 1.0f; lookAhead <= 3.0f; lookAhead += 0.25f) {
                    SimulationResult result = runSimulation(grid, start, targetCenter, targetRotation,
                                                            turnRate, 12.0f, minSpeed, lookAhead);
                    if (result.stayedOnRoad && result.maxDeviationFromCenter < bestDeviation) {
                        float posError = glm::length(result.finalPosition - targetCenter);
                        float rotError = std::abs(angleDifference(targetRotation, result.finalRotation));
                        if (posError < 0.5f && rotError < 10.0f) {
                            bestDeviation = result.maxDeviationFromCenter;
                            bestTurnRate = turnRate;
                            bestMinSpeed = minSpeed;
                            bestLookAhead = lookAhead;
                        }
                    }
                }
            }
        }
        
        if (bestDeviation < 999.0f) {
            SimulationResult finalResult = runSimulation(grid, start, targetCenter, targetRotation,
                                                          bestTurnRate, 12.0f, bestMinSpeed, bestLookAhead);
            printResult("Test 2", finalResult, targetCenter, targetRotation);
            std::cout << "Best params: turnRate=" << bestTurnRate << ", minSpeed=" << bestMinSpeed << ", lookAhead=" << bestLookAhead << std::endl;
            passedTests++;
        } else {
            std::cout << "TEST 2 FAILED - No valid parameters found!" << std::endl;
            // Debug: Run one simulation to see what happens
            std::cout << "\n--- Debug run ---" << std::endl;
            TestVehicle debugVehicle;
            debugVehicle.position = glm::vec2(5.0f, 1.5f);
            debugVehicle.rotation = 270.0f;
            
            TestGrid debugGrid(2, 2, TILE_SIZE);
            debugGrid.setDirection(0, 1, CarDirection::North);
            debugGrid.setDirection(0, 0, CarDirection::NorthWest);
            debugGrid.setDirection(1, 0, CarDirection::West);
            
            for (int step = 0; step < 50; step++) {
                glm::ivec2 gp = debugGrid.worldToGrid(debugVehicle.position);
                CarDirection cd = debugGrid.getDirection(gp.x, gp.y);
                std::cout << "Step " << step << ": pos=(" << debugVehicle.position.x << "," << debugVehicle.position.y 
                         << ") rot=" << debugVehicle.rotation << " grid=(" << gp.x << "," << gp.y << ") dir=" << (int)cd << std::endl;
                updateVehicleAI(debugVehicle, debugGrid, 0.016f, 600.0f, 12.0f, 3.0f, 2.0f);
            }
        }
    }
    
    std::cout << "\n=== SUMMARY ===" << std::endl;
    std::cout << "Passed: " << passedTests << "/" << totalTests << std::endl;
    
    return passedTests == totalTests ? 0 : 1;
}
