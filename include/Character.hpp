#pragma once

#include "CharacterPhysics.hpp"
#include "Collider.hpp"
#include "GameObject.hpp"

#include <glm/glm.hpp>

// Base for all on-foot characters. Movement goes through CharacterPhysics so
// new character types inherit terrain, slope, and vehicle collision handling.
class Character : public GameObject, public Collider {
public:
    Character(CharacterPhysics& physics, const glm::vec2& size, float physicsCollisionScale = 1.0f);
    ~Character() override = default;

    CharacterMoveResult tryMove(const glm::vec3& delta,
                                CharacterMoveMode mode = CharacterMoveMode::AllOrNothing,
                                const Collider* ignoredObstacle = nullptr);
    bool canMoveTo(const glm::vec3& target, const Collider* ignoredObstacle = nullptr) const;
    bool isObstacleAt(const glm::vec3& position, const Collider* ignoredObstacle = nullptr) const;

    const glm::vec2& getCharacterSize() const { return m_characterSize; }

    glm::vec3 getColliderPosition() const override { return m_position; }
    float getColliderRotation() const override { return m_rotation.z; }
    glm::vec2 getColliderSize() const override { return m_characterSize; }
    bool isColliderActive() const override { return m_active; }

protected:
    void setCharacterSize(const glm::vec2& size) { m_characterSize = size; }

private:
    glm::vec2 getPhysicsCollisionSize() const;

    CharacterPhysics& m_physics;
    glm::vec2 m_characterSize;
    float m_physicsCollisionScale;
};
