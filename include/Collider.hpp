#pragma once

#include <glm/glm.hpp>
#include <array>
#include <vector>
#include <functional>

// Forward declarations
class Collider;

// Function type for getting all collidable objects in the scene
using ColliderCallback = std::function<std::vector<const Collider*>()>;

/**
 * Interface for objects that can participate in collision detection.
 * Any sprite-based object (vehicles, player, etc.) can implement this.
 */
class Collider {
public:
    virtual ~Collider() = default;
    
    // Get the world position of the collider
    virtual glm::vec3 getColliderPosition() const = 0;
    
    // Get the rotation in degrees (z-axis rotation for 2D)
    virtual float getColliderRotation() const = 0;
    
    // Get the size of the collider (width, height)
    virtual glm::vec2 getColliderSize() const = 0;
    
    // Check if this collider is active
    virtual bool isColliderActive() const = 0;
    
    // Get the 4 corners of the oriented bounding box in world space
    std::array<glm::vec2, 4> getCorners() const;
    
    // Get corners at a hypothetical position and rotation
    std::array<glm::vec2, 4> getCornersAt(const glm::vec3& position, float rotation) const;

    // Get corners for an arbitrary oriented box.
    static std::array<glm::vec2, 4> getCornersAt(const glm::vec3& position,
                                                 float rotation,
                                                 const glm::vec2& size);
    
    // Check collision with another collider
    bool checkCollisionWith(const Collider* other) const;
    
    // Check if this collider would collide with another at a given position/rotation
    bool checkCollisionAtPosition(const Collider* other, const glm::vec3& position, float rotation) const;
    
    // Static utility: Check collision between two sets of corners
    static bool checkOBBCollision(const std::array<glm::vec2, 4>& corners1,
                                   const std::array<glm::vec2, 4>& corners2);
};

/**
 * Helper class to manage collision queries against multiple objects.
 * Stores a callback to get all collidable objects and provides query methods.
 */
class CollisionManager {
public:
    CollisionManager() = default;
    
    void setColliderCallback(ColliderCallback callback) { m_callback = callback; }
    bool hasCallback() const { return m_callback != nullptr; }
    
    // Check if a collider would collide with any other colliders at a given position
    bool wouldCollide(const Collider* self, const glm::vec3& position, float rotation) const;
    
    // Get all colliders that would collide with the given collider at position
    std::vector<const Collider*> getCollisions(const Collider* self, const glm::vec3& position, float rotation) const;
    
private:
    ColliderCallback m_callback;
};
