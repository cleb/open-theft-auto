#pragma once

#include "GameObject.hpp"
#include "SpriteAnimation.hpp"
#include "Tile.hpp"
#include <memory>
#include <functional>
#include <glm/glm.hpp>

class TileGrid;
class Renderer;

// Callback that returns true if the given position is blocked by a vehicle
using VehicleBlockCheck = std::function<bool(const glm::vec3& position, float pedRadius)>;

// Pedestrian behavior state
enum class PedestrianState {
    Walking,           // Normal walking behavior
    CarjackExit,       // Playing carjack exit animation (frames 9, 11)
    CarjackStunned,    // Standing still at frame 11 for a moment
    CarjackRecovery,   // Playing frame 9 again before resuming
    SeekingSidewalk,   // Walking towards nearest sidewalk
    CenteringSidewalk, // Walking to center of sidewalk tile before normal walking
    Panic,             // Running away from the player
    Dead               // Death animation
};

class Pedestrian : public GameObject {
public:
    Pedestrian();
    ~Pedestrian() = default;

    // Initialize with a shared animation (texture shared across all pedestrians)
    void initialize(SpriteAnimation* sharedAnimation);
    void update(float deltaTime) override;
    void render(Renderer* renderer) override;

    void setTileGrid(TileGrid* tileGrid) { m_tileGrid = tileGrid; }
    void setSpeed(float speed);
    float getSpeed() const { return m_speed; }
    
    // Vehicle blocking callback - pedestrians can't walk through vehicles
    void setVehicleBlockCheck(VehicleBlockCheck callback) { m_vehicleBlockCheck = std::move(callback); }
    bool isBlockedByVehicle(const glm::vec3& position) const;
    
    // Set the walking direction based on sidewalk direction
    // If avoidReverse is true, prefer directions that don't turn back (used when arriving at sidewalk)
    void setWalkingDirection(SidewalkDirection dir, bool avoidReverse = false);
    SidewalkDirection getWalkingDirection() const { return m_walkingDirection; }
    
    glm::vec2 getSize() const { return m_size; }
    
    // Death state
    void kill();
    bool isDead() const { return m_state == PedestrianState::Dead; }
    
    // Carjack state - pedestrian was ejected from vehicle
    void startCarjackExit();
    bool isCarjacking() const;

    void startPanic(const glm::vec3& threatPosition, float durationSeconds);
    
    // State access
    PedestrianState getState() const { return m_state; }

private:
    SpriteAnimation* m_sharedAnimation;  // Shared animation (not owned)
    float m_animationTime;               // Per-instance animation timer
    
    TileGrid* m_tileGrid;
    float m_speed;
    float m_baseSpeed;
    float m_panicSpeed;
    glm::vec2 m_size;
    SidewalkDirection m_walkingDirection;
    
    // State machine
    PedestrianState m_state;
    float m_stateTimer;         // Timer for current state
    int m_carjackFrame;         // Current carjack animation frame (0=frame9, 1=frame11)
    
    // Legacy death state (kept for compatibility, now uses m_state)
    int m_deathFrame;           // Current death animation frame (0 or 1)
    float m_deathAnimTimer;     // Timer for death animation
    
    // Sidewalk seeking
    glm::vec3 m_targetSidewalkPos;  // Position to walk towards
    bool m_hasTargetSidewalk;
    glm::vec3 m_sidewalkCenterPos;  // Center of current sidewalk tile
    SidewalkDirection m_pendingSidewalkDir; // Direction to use after centering
    glm::vec3 m_panicSource;
    float m_panicDuration;
    
    // Vehicle blocking callback
    VehicleBlockCheck m_vehicleBlockCheck;
    
    void updateMovement(float deltaTime);
    void updateDeathAnimation(float deltaTime);
    void updateCarjackAnimation(float deltaTime);
    void updateSeekingSidewalk(float deltaTime);
    void updateCenteringSidewalk(float deltaTime);
    void updatePanic(float deltaTime);

    // Adjust the z of `pos` to follow the tile grid's top surface, honoring slopes.
    void snapToSurface(glm::vec3& pos) const;
    void findNearestSidewalk();
    glm::vec4 getCurrentFrameUV() const;
};
