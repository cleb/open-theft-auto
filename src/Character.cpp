#include "Character.hpp"

#include <algorithm>

Character::Character(CharacterPhysics& physics, const glm::vec2& size, float physicsCollisionScale)
    : m_physics(physics)
    , m_characterSize(size)
    , m_physicsCollisionScale(std::max(0.0f, physicsCollisionScale)) {
}

glm::vec2 Character::getPhysicsCollisionSize() const {
    return m_characterSize * m_physicsCollisionScale;
}

CharacterMoveResult Character::tryMove(const glm::vec3& delta,
                                       CharacterMoveMode mode,
                                       const Collider* ignoredObstacle) {
    CharacterMoveResult result =
        m_physics.move(this, m_position, m_rotation.z, getPhysicsCollisionSize(),
                       m_characterSize, delta, mode, ignoredObstacle);
    m_position = result.position;
    return result;
}

bool Character::canMoveTo(const glm::vec3& target, const Collider* ignoredObstacle) const {
    return m_physics.canMove(this, m_position, target, m_rotation.z,
                             getPhysicsCollisionSize(), ignoredObstacle);
}

bool Character::isObstacleAt(const glm::vec3& position, const Collider* ignoredObstacle) const {
    return m_physics.isObstacleAt(this, position, m_rotation.z,
                                  getPhysicsCollisionSize(), ignoredObstacle);
}
