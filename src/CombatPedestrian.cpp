#include "CombatPedestrian.hpp"
#include "SpriteAnimation.hpp"
#include "TileGrid.hpp"
#include "Renderer.hpp"
#include <glm/geometric.hpp>
#include <cmath>

CombatPedestrian::CombatPedestrian() = default;

void CombatPedestrian::spawn(SpriteAnimation* animation, TileGrid* tileGrid,
                              const glm::vec3& position, float headingDeg) {
    m_tileGrid = tileGrid;
    m_pedestrian = std::make_unique<Pedestrian>();
    m_pedestrian->initialize(animation);
    m_pedestrian->setTileGrid(tileGrid);
    m_pedestrian->setSpeed(0.0f);  // We drive movement manually
    m_pedestrian->setActive(true);
    m_pedestrian->setPosition(position);
    m_pedestrian->setRotation(glm::vec3(0.0f, 0.0f, headingDeg));
    m_shootCooldown = 0.2f;
}

void CombatPedestrian::update(float deltaTime, const glm::vec3& targetPos) {
    if (!m_pedestrian || !m_pedestrian->isActive()) return;

    // Pedestrian::update handles animation states (death animation, etc.)
    m_pedestrian->update(deltaTime);

    if (m_pedestrian->isDead()) return;

    const glm::vec3 pos = m_pedestrian->getPosition();
    const glm::vec2 toTarget(targetPos.x - pos.x, targetPos.y - pos.y);
    const float dist = glm::length(toTarget);
    if (dist < 0.001f) return;

    const glm::vec2 dir = toTarget / dist;

    // Face target
    m_pedestrian->setRotation(glm::vec3(0.0f, 0.0f, Heading::headingDegFromForward(dir)));

    // Chase if too far
    if (dist > m_chaseDistance) {
        glm::vec3 nextPos = pos + glm::vec3(dir.x, dir.y, 0.0f) * (m_speed * deltaTime);
        nextPos.z = pos.z;
        if (m_tileGrid && m_tileGrid->canOccupy(pos, nextPos)) {
            m_pedestrian->setPosition(nextPos);
        }
    }

    // Shoot
    m_shootCooldown = std::max(0.0f, m_shootCooldown - deltaTime);
    if (dist <= m_fireDistance && m_shootCooldown <= 0.0f && m_shootCallback) {
        m_shootCallback(glm::vec3(pos.x, pos.y, pos.z + 0.15f), dir);
        m_shootCooldown = m_shootCooldownTime;
    }
}

void CombatPedestrian::render(Renderer* renderer) const {
    if (m_pedestrian && m_pedestrian->isActive()) {
        m_pedestrian->render(renderer);
    }
}

bool CombatPedestrian::checkVehicleCollision(const glm::vec3& vehiclePos, const glm::vec2& vehicleSize, float vehicleRotation) {
    if (!m_pedestrian || !m_pedestrian->isActive() || m_pedestrian->isDead()) return false;

    const glm::vec3 pedPos = m_pedestrian->getPosition();
    const glm::vec2 pedSize = m_pedestrian->getSize();
    const float pedRadius = std::max(pedSize.x, pedSize.y) * 0.3f;

    const glm::vec2 forward = Heading::forwardFromHeadingDeg(vehicleRotation);
    const glm::vec2 right(forward.y, -forward.x);
    const glm::vec2 toP(pedPos.x - vehiclePos.x, pedPos.y - vehiclePos.y);

    const float localX = glm::dot(toP, right);
    const float localY = glm::dot(toP, forward);
    const float halfWidth = vehicleSize.x * 0.5f;
    const float halfLength = vehicleSize.y * 0.5f;

    if (std::abs(localX) < halfWidth + pedRadius && std::abs(localY) < halfLength + pedRadius) {
        m_pedestrian->kill();
        return true;
    }
    return false;
}

bool CombatPedestrian::checkBulletHit(const glm::vec3& bulletPos, float bulletRadius) {
    if (!m_pedestrian || !m_pedestrian->isActive() || m_pedestrian->isDead()) return false;

    const glm::vec3 pos = m_pedestrian->getPosition();
    const glm::vec2 pedSize = m_pedestrian->getSize();
    const float pedRadius = std::max(pedSize.x, pedSize.y) * 0.4f;

    const float dx = bulletPos.x - pos.x;
    const float dy = bulletPos.y - pos.y;
    const float r = pedRadius + bulletRadius;
    if (dx * dx + dy * dy <= r * r) {
        m_pedestrian->kill();
        return true;
    }
    return false;
}

bool CombatPedestrian::isAlive() const {
    return m_pedestrian && m_pedestrian->isActive() && !m_pedestrian->isDead();
}

bool CombatPedestrian::isDead() const {
    return !m_pedestrian || !m_pedestrian->isActive() || m_pedestrian->isDead();
}

const glm::vec3& CombatPedestrian::getPosition() const {
    static const glm::vec3 zero(0.0f);
    return m_pedestrian ? m_pedestrian->getPosition() : zero;
}
