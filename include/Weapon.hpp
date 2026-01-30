#pragma once

class Weapon {
public:
    virtual ~Weapon() = default;

    virtual void update(float deltaTime) = 0;
    virtual bool canFire() const = 0;
    virtual void recordFire() = 0;
};
