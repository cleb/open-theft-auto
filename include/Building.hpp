#pragma once

#include "GameObject.hpp"
#include <memory>

class Building : public GameObject {
private:
    std::unique_ptr<Mesh> m_mesh;
    std::shared_ptr<Texture> m_texture;
    glm::vec3 m_size;

public:
    Building();
    ~Building() = default;
    
    bool initialize(const glm::vec3& size, const std::string& texturePath = "");
    void update(float deltaTime) override;
    void render(class Renderer* renderer) override;
    
    const glm::vec3& getSize() const { return m_size; }
    
private:
    void createBuildingMesh(const glm::vec3& size);
};