#include "InputManager.hpp"
#include <iostream>

InputManager::InputManager()
    : m_mouseX(0.0)
    , m_mouseY(0.0)
    , m_deltaMouseX(0.0)
    , m_deltaMouseY(0.0)
    , m_lastMouseX(0.0)
    , m_lastMouseY(0.0)
    , m_firstMouse(true) {
}

InputManager::~InputManager() {
}

void InputManager::initialize() {
    m_keys.clear();
    m_keysPressed.clear();
    m_mouseButtons.clear();
    m_mouseButtonsPressed.clear();

    m_mouseX = 0.0;
    m_mouseY = 0.0;
    m_deltaMouseX = 0.0;
    m_deltaMouseY = 0.0;
    m_lastMouseX = 0.0;
    m_lastMouseY = 0.0;
    m_firstMouse = true;
}

void InputManager::setInitialMousePosition(double xpos, double ypos) {
    m_mouseX = xpos;
    m_mouseY = ypos;
    m_lastMouseX = xpos;
    m_lastMouseY = ypos;
}

void InputManager::update() {
    // Update mouse delta
    m_deltaMouseX = m_mouseX - m_lastMouseX;
    m_deltaMouseY = m_mouseY - m_lastMouseY;
    m_lastMouseX = m_mouseX;
    m_lastMouseY = m_mouseY;
    
    if (m_firstMouse) {
        m_deltaMouseX = 0.0;
        m_deltaMouseY = 0.0;
        m_firstMouse = false;
    }
}

void InputManager::clearPressed() {
    m_keysPressed.clear();
    m_mouseButtonsPressed.clear();
}

bool InputManager::isKeyDown(int key) const {
    auto it = m_keys.find(key);
    return it != m_keys.end() && it->second;
}

bool InputManager::isKeyPressed(int key) const {
    auto it = m_keysPressed.find(key);
    return it != m_keysPressed.end() && it->second;
}

bool InputManager::isMouseButtonDown(int button) const {
    auto it = m_mouseButtons.find(button);
    return it != m_mouseButtons.end() && it->second;
}

bool InputManager::isMouseButtonPressed(int button) const {
    auto it = m_mouseButtonsPressed.find(button);
    return it != m_mouseButtonsPressed.end() && it->second;
}

void InputManager::onKey(int key, int action) {
    if (action == GLFW_PRESS) {
        m_keys[key] = true;
        m_keysPressed[key] = true;
    } else if (action == GLFW_RELEASE) {
        m_keys[key] = false;
    }
}

void InputManager::onMouseButton(int button, int action) {
    if (action == GLFW_PRESS) {
        m_mouseButtons[button] = true;
        m_mouseButtonsPressed[button] = true;
    } else if (action == GLFW_RELEASE) {
        m_mouseButtons[button] = false;
    }
}

void InputManager::onCursorPos(double xpos, double ypos) {
    m_mouseX = xpos;
    m_mouseY = ypos;
}

void InputManager::onScroll(double xoffset, double yoffset) {
    (void)xoffset;
    (void)yoffset;
}

void InputManager::onChar(unsigned int codepoint) {
    (void)codepoint;
}
