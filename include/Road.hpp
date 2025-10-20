#pragma once

#include "GameObject.hpp"
#include <memory>
#include <vector>

class Road : public GameObject {
private:
    std::unique_ptr<Mesh> m_mesh;
    glm::vec2 m_size;

public:
    Road();
    ~Road() = default;
    
    bool initialize(const glm::vec2& size);
    void update(float deltaTime) override;
    void render(class Renderer* renderer) override;
    
private:
    void createRoadMesh(const glm::vec2& size);
};