#pragma once

#include <memory>
#include <chrono>
#include "Window.hpp"
#include "Renderer.hpp"
#include "InputManager.hpp"
#include "Scene.hpp"

class Engine {
private:
    std::unique_ptr<Window> m_window;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<InputManager> m_inputManager;
    std::unique_ptr<Scene> m_scene;
    
    bool m_running;
    std::chrono::high_resolution_clock::time_point m_lastTime;
    float m_deltaTime;
    bool m_imguiInitialized;
    
    void calculateDeltaTime();
    void processInput();
    void update(float deltaTime);
    void render();

public:
    Engine();
    ~Engine();
    
    bool initialize(int width, int height, const std::string& title);
    void run();
    void shutdown();
    
    Window* getWindow() const { return m_window.get(); }
    Renderer* getRenderer() const { return m_renderer.get(); }
    InputManager* getInputManager() const { return m_inputManager.get(); }
};