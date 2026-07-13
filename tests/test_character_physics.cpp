#include "CharacterPhysics.hpp"

#include <cassert>
#include <cmath>
#include <vector>

namespace {

class TestCollider final : public Collider {
public:
    glm::vec3 position{0.0f};
    glm::vec2 size{1.0f};
    float rotation = 0.0f;
    bool active = true;

    glm::vec3 getColliderPosition() const override { return position; }
    float getColliderRotation() const override { return rotation; }
    glm::vec2 getColliderSize() const override { return size; }
    bool isColliderActive() const override { return active; }
};

bool approximately(float lhs, float rhs, float epsilon = 0.0001f) {
    return std::abs(lhs - rhs) <= epsilon;
}

}  // namespace

int main() {
    TestCollider character;
    character.size = glm::vec2(0.6f);

    CharacterPhysics unconfigured;
    CharacterMoveResult result =
        unconfigured.move(&character, glm::vec3(0.0f), 0.0f, character.size, character.size,
                          glm::vec3(1.0f, 0.0f, 0.0f), CharacterMoveMode::AllOrNothing);
    assert(!result.moved);
    assert(result.blockedX);

    TestCollider vehicle;
    vehicle.position = glm::vec3(2.0f, 0.0f, 0.0f);
    vehicle.size = glm::vec2(1.0f);

    bool allowPositiveX = true;
    CharacterPhysics physics;
    physics.configure(
        [&allowPositiveX](const glm::vec3&, const glm::vec3& to) {
            return allowPositiveX || to.x <= 0.0f;
        },
        [](const glm::vec3&, const glm::vec2&, const glm::vec2&, float) {
            return 3.0f;
        },
        [&vehicle]() {
            return std::vector<const Collider*>{&vehicle};
        });

    result = physics.move(&character, glm::vec3(0.0f), 0.0f, character.size, character.size,
                          glm::vec3(4.0f, 0.0f, 0.0f), CharacterMoveMode::AllOrNothing);
    assert(result.moved);
    assert(result.blockedX);
    assert(result.position.x < vehicle.position.x);

    result = physics.move(&character, glm::vec3(0.0f), 0.0f, character.size, character.size,
                          glm::vec3(4.0f, 0.0f, 0.0f), CharacterMoveMode::AllOrNothing, &vehicle);
    assert(result.moved);
    assert(!result.blocked());
    assert(approximately(result.position.x, 4.0f));
    assert(approximately(result.position.z, 3.0f));

    allowPositiveX = false;
    result = physics.move(&character, glm::vec3(0.0f), 0.0f, character.size, character.size,
                          glm::vec3(1.0f, 1.0f, 0.0f), CharacterMoveMode::Slide, &vehicle);
    assert(result.moved);
    assert(result.blockedX);
    assert(!result.blockedY);
    assert(approximately(result.position.x, 0.0f));
    assert(approximately(result.position.y, 1.0f));

    character.size = glm::vec2(0.48f);
    vehicle.position = glm::vec3(0.0f);
    vehicle.size = glm::vec2(1.5f, 3.0f);
    assert(physics.isObstacleAt(&character, glm::vec3(1.65f, 0.95f, 0.0f),
                                0.0f, character.size));

    return 0;
}
