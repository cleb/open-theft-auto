#pragma once

#include <GLFW/glfw3.h>
#include <unordered_map>

class InputManager {
private:
    GLFWwindow* m_window;
    std::unordered_map<int, bool> m_keys;
    std::unordered_map<int, bool> m_keysPressed;
    std::unordered_map<int, bool> m_mouseButtons;
    std::unordered_map<int, bool> m_mouseButtonsPressed;

    static std::unordered_map<GLFWwindow*, InputManager*> s_instances;
    
    double m_mouseX, m_mouseY;
    double m_deltaMouseX, m_deltaMouseY;
    double m_lastMouseX, m_lastMouseY;
    bool m_firstMouse;

public:
    InputManager();
    ~InputManager();
    
    void initialize(GLFWwindow* window);
    void update();
    void clearPressed();
    
    // Key input
    bool isKeyDown(int key) const;
    bool isKeyPressed(int key) const; // True only on the frame the key was pressed
    
    // Mouse input
    bool isMouseButtonDown(int button) const;
    bool isMouseButtonPressed(int button) const;
    
    double getMouseX() const { return m_mouseX; }
    double getMouseY() const { return m_mouseY; }
    double getDeltaMouseX() const { return m_deltaMouseX; }
    double getDeltaMouseY() const { return m_deltaMouseY; }
    
    // Callbacks (to be set as GLFW callbacks)
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void charCallback(GLFWwindow* window, unsigned int codepoint);
};