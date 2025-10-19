#include "InputManager.hpp"
#include <iostream>

InputManager::InputManager() 
    : m_window(nullptr)
    , m_mouseX(0.0)
    , m_mouseY(0.0)
    , m_deltaMouseX(0.0)
    , m_deltaMouseY(0.0)
    , m_lastMouseX(0.0)
    , m_lastMouseY(0.0)
    , m_firstMouse(true) {
}

void InputManager::initialize(GLFWwindow* window) {
    m_window = window;
    
    // Set this instance as user pointer so callbacks can access it
    glfwSetWindowUserPointer(window, this);
    
    // Set callbacks
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    
    // Get initial mouse position
    glfwGetCursorPos(window, &m_mouseX, &m_mouseY);
    m_lastMouseX = m_mouseX;
    m_lastMouseY = m_mouseY;
}

void InputManager::update() {
    // Clear pressed states from previous frame
    m_keysPressed.clear();
    m_mouseButtonsPressed.clear();
    
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

void InputManager::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    InputManager* inputManager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
    if (!inputManager) return;
    
    (void)scancode; (void)mods; // Suppress unused parameter warnings
    
    if (action == GLFW_PRESS) {
        inputManager->m_keys[key] = true;
        inputManager->m_keysPressed[key] = true;
    } else if (action == GLFW_RELEASE) {
        inputManager->m_keys[key] = false;
    }
}

void InputManager::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    InputManager* inputManager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
    if (!inputManager) return;
    
    (void)mods; // Suppress unused parameter warning
    
    if (action == GLFW_PRESS) {
        inputManager->m_mouseButtons[button] = true;
        inputManager->m_mouseButtonsPressed[button] = true;
    } else if (action == GLFW_RELEASE) {
        inputManager->m_mouseButtons[button] = false;
    }
}

void InputManager::cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    InputManager* inputManager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
    if (!inputManager) return;
    
    inputManager->m_mouseX = xpos;
    inputManager->m_mouseY = ypos;
}