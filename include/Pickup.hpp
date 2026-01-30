#pragma once

#include "GameObject.hpp"
#include "PickupTypes.hpp"
#include "Texture.hpp"
#include <algorithm>
#include <memory>

class Renderer;

class Pickup : public GameObject {
public:
    explicit Pickup(PickupType type = PickupType::Pistol);
    ~Pickup() override = default;

    bool initialize();
    void update(float deltaTime) override;
    void render(Renderer* renderer) override;

    PickupType getType() const { return m_type; }
    const glm::vec2& getSize() const { return m_size; }
    float getRadius() const { return std::max(m_size.x, m_size.y) * 0.5f; }

private:
    PickupType m_type;
    std::shared_ptr<Texture> m_texture;
    glm::vec2 m_size;
    float m_rotationSpeed;
};
