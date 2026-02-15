#pragma once

class Weapon {
public:
    virtual ~Weapon() = default;

    virtual void update(float deltaTime) = 0;
    virtual bool canFire() const = 0;
    virtual void recordFire() = 0;
    virtual const char* getDisplayName() const = 0;
    virtual int getAmmo() const = 0;
    virtual void addAmmo(int amount) = 0;
};
