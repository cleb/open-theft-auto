#pragma once

#include <GLFW/glfw3.h>
#include <unordered_map>

class InputManager {
private:
    std::unordered_map<int, bool> m_keys;
    std::unordered_map<int, bool> m_keysPressed;
    std::unordered_map<int, bool> m_mouseButtons;
    std::unordered_map<int, bool> m_mouseButtonsPressed;

    double m_mouseX, m_mouseY;
    double m_deltaMouseX, m_deltaMouseY;
    double m_lastMouseX, m_lastMouseY;
    bool m_firstMouse;

public:
    InputManager();
    ~InputManager();

    void initialize();
    void setInitialMousePosition(double xpos, double ypos);
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

    // Event handlers (invoked by Window callbacks)
    void onKey(int key, int scancode, int action, int mods);
    void onMouseButton(int button, int action, int mods);
    void onCursorPos(double xpos, double ypos);
    void onScroll(double xoffset, double yoffset);
    void onChar(unsigned int codepoint);
};
