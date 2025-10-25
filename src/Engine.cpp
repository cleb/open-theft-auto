#include "Engine.hpp"
#include <iostream>
#include <GL/glew.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

Engine::Engine() : m_running(false), m_deltaTime(0.0f), m_imguiInitialized(false) {
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
    
    // Initialize input manager
    m_inputManager = std::make_unique<InputManager>();
    m_inputManager->initialize();

    // Create window
    m_window = std::make_unique<Window>();
    if (!m_window->create(width, height, title, m_inputManager.get())) {
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
    
    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();
    if (!ImGui_ImplGlfw_InitForOpenGL(m_window->getGLFWWindow(), false)) {
        std::cerr << "Failed to initialize ImGui GLFW backend" << std::endl;
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 330")) {
        std::cerr << "Failed to initialize ImGui OpenGL backend" << std::endl;
        return false;
    }
    m_imguiInitialized = true;
    
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

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        render();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

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
    
    if (m_imguiInitialized) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_imguiInitialized = false;
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

    m_scene->processInput(m_inputManager.get(), m_deltaTime);

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
        m_scene->drawGui();
    }
    
    m_renderer->endFrame();
}