#pragma once

#include <GL/glew.h>
#include <string>

class Texture {
private:
    GLuint m_textureID;
    int m_width;
    int m_height;
    int m_channels;

public:
    Texture();
    ~Texture();
    
    bool loadFromFile(const std::string& filePath);
    void bind(GLuint slot = 0) const;
    void unbind() const;
    
    GLuint getID() const { return m_textureID; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
};