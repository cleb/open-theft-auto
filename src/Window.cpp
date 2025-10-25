#include "Window.hpp"
#include "InputManager.hpp"
#include <iostream>
#include <imgui_impl_glfw.h>

Window::Window() : m_window(nullptr), m_width(0), m_height(0), m_inputManager(nullptr) {
}

Window::~Window() {
    destroy();
}

bool Window::create(int width, int height, const std::string& title, InputManager* inputManager) {
    m_width = width;
    m_height = height;
    m_title = title;
    m_inputManager = inputManager;

    if (!m_inputManager) {
        std::cerr << "InputManager pointer is null" << std::endl;
        return false;
    }
    
    // Set GLFW window hints
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    
    // Create window
    m_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!m_window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        return false;
    }
    
    // Make context current
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // Enable vsync
    
    // Set callbacks
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebufferSizeCallback);
    glfwSetKeyCallback(m_window, keyCallback);
    glfwSetMouseButtonCallback(m_window, mouseButtonCallback);
    glfwSetCursorPosCallback(m_window, cursorPosCallback);
    glfwSetScrollCallback(m_window, scrollCallback);
    glfwSetCharCallback(m_window, charCallback);

    double xpos = 0.0;
    double ypos = 0.0;
    glfwGetCursorPos(m_window, &xpos, &ypos);
    m_inputManager->setInitialMousePosition(xpos, ypos);

    return true;
}

void Window::destroy() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    m_inputManager = nullptr;
}

bool Window::shouldClose() const {
    return m_window ? glfwWindowShouldClose(m_window) : true;
}

void Window::swapBuffers() {
    if (m_window) {
        glfwSwapBuffers(m_window);
    }
}

void Window::pollEvents() {
    glfwPollEvents();
}

void Window::framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    Window* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (windowInstance) {
        windowInstance->m_width = width;
        windowInstance->m_height = height;
        glViewport(0, 0, width, height);
    }
}

void Window::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    Window* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!windowInstance || !windowInstance->m_inputManager) {
        return;
    }

    ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
    windowInstance->m_inputManager->onKey(key, scancode, action, mods);
}

void Window::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    Window* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!windowInstance || !windowInstance->m_inputManager) {
        return;
    }

    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    windowInstance->m_inputManager->onMouseButton(button, action, mods);
}

void Window::cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    Window* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!windowInstance || !windowInstance->m_inputManager) {
        return;
    }

    ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
    windowInstance->m_inputManager->onCursorPos(xpos, ypos);
}

void Window::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    Window* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!windowInstance || !windowInstance->m_inputManager) {
        return;
    }

    ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
    windowInstance->m_inputManager->onScroll(xoffset, yoffset);
}

void Window::charCallback(GLFWwindow* window, unsigned int codepoint) {
    Window* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!windowInstance || !windowInstance->m_inputManager) {
        return;
    }

    ImGui_ImplGlfw_CharCallback(window, codepoint);
    windowInstance->m_inputManager->onChar(codepoint);
}
