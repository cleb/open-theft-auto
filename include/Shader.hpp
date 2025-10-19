#pragma once

#include <GL/glew.h>
#include <string>
#include <glm/glm.hpp>

class Shader {
private:
    GLuint m_programID;
    
    GLuint loadShader(const std::string& filePath, GLenum shaderType);
    std::string readFile(const std::string& filePath);

public:
    Shader();
    ~Shader();
    
    bool loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);
    void use();
    void unuse();
    
    GLuint getProgramID() const { return m_programID; }
    
    // Uniform setters
    void setInt(const std::string& name, int value);
    void setFloat(const std::string& name, float value);
    void setVec2(const std::string& name, const glm::vec2& value);
    void setVec3(const std::string& name, const glm::vec3& value);
    void setVec4(const std::string& name, const glm::vec4& value);
    void setMat4(const std::string& name, const glm::mat4& value);
    
private:
    GLint getUniformLocation(const std::string& name);
};