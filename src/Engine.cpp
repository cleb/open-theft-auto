#include "Engine.hpp"
#include <iostream>
#include <GL/glew.h>

Engine::Engine() : m_running(false), m_deltaTime(0.0f) {
}

Engine::~Engine() {
    shutdown();
}

bool Engine::initialize(int width, int height, const std::string& title) {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }
    
    // Create window
    m_window = std::make_unique<Window>();
    if (!m_window->create(width, height, title)) {
        std::cerr << "Failed to create window" << std::endl;
        return false;
    }
    
    // Initialize GLEW
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return false;
    }
    
    // Initialize renderer
    m_renderer = std::make_unique<Renderer>();
    if (!m_renderer->initialize(width, height)) {
        std::cerr << "Failed to initialize renderer" << std::endl;
        return false;
    }
    
    // Initialize input manager
    m_inputManager = std::make_unique<InputManager>();
    m_inputManager->initialize(m_window->getGLFWWindow());
    
    // Initialize scene
    m_scene = std::make_unique<Scene>();
    m_scene->initialize();
    
    m_running = true;
    m_lastTime = std::chrono::high_resolution_clock::now();
    
    std::cout << "Engine initialized successfully" << std::endl;
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GPU: " << glGetString(GL_RENDERER) << std::endl;
    
    return true;
}

void Engine::run() {
    while (m_running && !m_window->shouldClose()) {
        m_window->pollEvents();
        calculateDeltaTime();
        processInput();
        update(m_deltaTime);
        render();
        
        m_window->swapBuffers();
    }
}

void Engine::shutdown() {
    if (m_scene) {
        m_scene.reset();
    }
    
    if (m_inputManager) {
        m_inputManager.reset();
    }
    
    if (m_renderer) {
        m_renderer->shutdown();
        m_renderer.reset();
    }
    
    if (m_window) {
        m_window->destroy();
        m_window.reset();
    }
    
    glfwTerminate();
    m_running = false;
}

void Engine::calculateDeltaTime() {
    auto currentTime = std::chrono::high_resolution_clock::now();
    m_deltaTime = std::chrono::duration<float>(currentTime - m_lastTime).count();
    m_lastTime = currentTime;
}

void Engine::processInput() {
    m_inputManager->update();
    
    // Handle basic engine inputs
    if (m_inputManager->isKeyPressed(GLFW_KEY_ESCAPE)) {
        m_running = false;
    }
    
    if (!m_scene) {
        m_inputManager->clearPressed();
        return;
    }

    if (m_inputManager->isKeyPressed(GLFW_KEY_ENTER) ||
        m_inputManager->isKeyPressed(GLFW_KEY_KP_ENTER)) {
        m_scene->tryEnterNearestVehicle();
    }

    if (m_scene->isPlayerInVehicle()) {
        if (Vehicle* vehicle = m_scene->getActiveVehicle()) {
            if (m_inputManager->isKeyDown(GLFW_KEY_W) || m_inputManager->isKeyDown(GLFW_KEY_UP)) {
                vehicle->accelerate(m_deltaTime);
            }
            if (m_inputManager->isKeyDown(GLFW_KEY_S) || m_inputManager->isKeyDown(GLFW_KEY_DOWN)) {
                vehicle->brake(m_deltaTime);
            }
            if (m_inputManager->isKeyDown(GLFW_KEY_A) || m_inputManager->isKeyDown(GLFW_KEY_LEFT)) {
                vehicle->turnRight(m_deltaTime);
            }
            if (m_inputManager->isKeyDown(GLFW_KEY_D) || m_inputManager->isKeyDown(GLFW_KEY_RIGHT)) {
                vehicle->turnLeft(m_deltaTime);
            }
        }
        m_inputManager->clearPressed();
        return;
    }

    // Handle player input
    if (Player* player = m_scene->getPlayer()) {
        if (m_inputManager->isKeyDown(GLFW_KEY_W) || m_inputManager->isKeyDown(GLFW_KEY_UP)) {
            player->moveForward(m_deltaTime);
        }
        if (m_inputManager->isKeyDown(GLFW_KEY_S) || m_inputManager->isKeyDown(GLFW_KEY_DOWN)) {
            player->moveBackward(m_deltaTime);
        }
        if (m_inputManager->isKeyDown(GLFW_KEY_A) || m_inputManager->isKeyDown(GLFW_KEY_LEFT)) {
            player->turnLeft(m_deltaTime);
        }
        if (m_inputManager->isKeyDown(GLFW_KEY_D) || m_inputManager->isKeyDown(GLFW_KEY_RIGHT)) {
            player->turnRight(m_deltaTime);
        }
    }

    m_inputManager->clearPressed();
}

void Engine::update(float deltaTime) {
    if (m_scene) {
        m_scene->update(deltaTime);
    }
    
    // Update camera
    if (m_renderer && m_renderer->getCamera()) {
        m_renderer->getCamera()->update(deltaTime);
    }
}

void Engine::render() {
    m_renderer->beginFrame();
    
    if (m_scene) {
        m_scene->render(m_renderer.get());
    }
    
    m_renderer->endFrame();
}