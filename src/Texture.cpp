#include "Texture.hpp"
#include <iostream>

Texture::Texture() : m_textureID(0), m_width(0), m_height(0), m_channels(0) {
}

Texture::~Texture() {
    if (m_textureID) {
        glDeleteTextures(1, &m_textureID);
    }
}

bool Texture::loadFromFile(const std::string& filePath) {
    // For now, create a simple colored texture as placeholder
    // In a real implementation, you'd use stb_image or similar library
    
    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    
    // Create a simple 2x2 white texture as placeholder
    unsigned char data[] = {
        255, 255, 255, 255,  // White pixel
        255, 255, 255, 255,  // White pixel
        255, 255, 255, 255,  // White pixel
        255, 255, 255, 255   // White pixel
    };
    
    m_width = 2;
    m_height = 2;
    m_channels = 4;
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    std::cout << "Created placeholder texture (2x2 white) for: " << filePath << std::endl;
    return true;
}

void Texture::bind(GLuint slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
}

void Texture::unbind() const {
    glBindTexture(GL_TEXTURE_2D, 0);
}