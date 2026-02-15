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

    // Returns true if the weapon fires continuously while the fire key is held.
    // When false the player must release and re-press the key for each shot.
    virtual bool isAutoFire() const { return false; }
};
