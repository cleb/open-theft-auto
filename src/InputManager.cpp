#include "InputManager.hpp"
#include <iostream>
#include <imgui_impl_glfw.h>

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
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetCharCallback(window, charCallback);
    
    // Get initial mouse position
    glfwGetCursorPos(window, &m_mouseX, &m_mouseY);
    m_lastMouseX = m_mouseX;
    m_lastMouseY = m_mouseY;
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

void InputManager::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    InputManager* inputManager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
    if (!inputManager) return;
    
    (void)scancode; (void)mods; // Suppress unused parameter warnings
    
    if (action == GLFW_PRESS) {
        ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
        inputManager->m_keys[key] = true;
        inputManager->m_keysPressed[key] = true;
    } else if (action == GLFW_RELEASE) {
        ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
        inputManager->m_keys[key] = false;
    } else {
        ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
    }
}

void InputManager::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    InputManager* inputManager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
    if (!inputManager) return;
    
    (void)mods; // Suppress unused parameter warning
    
    if (action == GLFW_PRESS) {
        ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
        inputManager->m_mouseButtons[button] = true;
        inputManager->m_mouseButtonsPressed[button] = true;
    } else if (action == GLFW_RELEASE) {
        ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
        inputManager->m_mouseButtons[button] = false;
    } else {
        ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    }
}

void InputManager::cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    InputManager* inputManager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
    if (!inputManager) return;
    
    ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);

    inputManager->m_mouseX = xpos;
    inputManager->m_mouseY = ypos;
}

void InputManager::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    InputManager* inputManager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
    if (!inputManager) return;

    (void)xoffset;
    (void)yoffset;

    ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
}

void InputManager::charCallback(GLFWwindow* window, unsigned int codepoint) {
    InputManager* inputManager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
    if (!inputManager) return;

    (void)codepoint;

    ImGui_ImplGlfw_CharCallback(window, codepoint);
}