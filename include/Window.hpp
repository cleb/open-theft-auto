#pragma once

#include <string>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

class Window {
private:
    GLFWwindow* m_window;
    int m_width;
    int m_height;
    std::string m_title;
    
public:
    Window();
    ~Window();
    
    bool create(int width, int height, const std::string& title);
    void destroy();
    
    bool shouldClose() const;
    void swapBuffers();
    void pollEvents();
    
    GLFWwindow* getGLFWWindow() const { return m_window; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    float getAspectRatio() const { return static_cast<float>(m_width) / static_cast<float>(m_height); }
    
    // Callbacks
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
};