#pragma once

#include <memory>
#include <chrono>
#include <iosfwd>
#include <string>
#include <vector>
#include "Window.hpp"
#include "Renderer.hpp"
#include "InputManager.hpp"
#include "Scene.hpp"
#include "GameLogic.hpp"

class Engine {
private:
    std::unique_ptr<Window> m_window;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<InputManager> m_inputManager;
    std::unique_ptr<GameLogic> m_gameLogic;
    std::unique_ptr<Scene> m_scene;
    
    bool m_running;
    std::chrono::high_resolution_clock::time_point m_lastTime;
    float m_deltaTime;
    bool m_imguiInitialized;
    bool m_agentDebugMode;
    double m_agentDebugTimeSeconds;
    int m_agentDebugScreenshotIndex;
    
    void calculateDeltaTime();
    void processInput();
    void update(float deltaTime);
    void render();
    void runFrame(float deltaTime, bool processInputAndUpdate, const std::string& screenshotPath = std::string());
    void advanceAgentDebugTime(float durationSeconds, const std::vector<int>& heldKeys);
    bool runAgentDebugCommand(const std::string& line);
    std::string nextAgentDebugScreenshotPath();

public:
    Engine();
    ~Engine();
    
    bool initialize(int width, int height, const std::string& title);
    void run();
    void shutdown();
    void setAgentDebugMode(bool enabled) { m_agentDebugMode = enabled; }
    bool isAgentDebugMode() const { return m_agentDebugMode; }
    void runAgentDebugPrompt();
    void dumpAgentDebugState(std::ostream& out) const;
    
    Window* getWindow() const { return m_window.get(); }
    Renderer* getRenderer() const { return m_renderer.get(); }
    InputManager* getInputManager() const { return m_inputManager.get(); }
};
