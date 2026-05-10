#include "Engine.hpp"
#include "TextureManager.hpp"
#include "VehicleConfig.hpp"
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <GL/glew.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

namespace {
constexpr float kAgentDebugFixedStepSeconds = 1.0f / 60.0f;

std::string trim(const std::string& text) {
    auto begin = text.begin();
    while (begin != text.end() && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    auto end = text.end();
    while (end != begin && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    return std::string(begin, end);
}

std::string upper(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return text;
}

std::vector<std::string> splitWhitespace(const std::string& text) {
    std::istringstream in(text);
    std::vector<std::string> tokens;
    std::string token;
    while (in >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

std::vector<std::string> splitKeyChord(const std::string& chord) {
    std::vector<std::string> result;
    std::string current;
    for (char c : chord) {
        if (c == '+' || c == ',') {
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        result.push_back(current);
    }
    return result;
}

bool parseDurationSeconds(const std::string& token, float& seconds) {
    if (token.empty()) {
        return false;
    }

    std::string number = token;
    float multiplier = 1.0f;
    if (token.size() > 2 && token.substr(token.size() - 2) == "ms") {
        number = token.substr(0, token.size() - 2);
        multiplier = 0.001f;
    } else if (token.back() == 's') {
        number = token.substr(0, token.size() - 1);
    } else if (token.back() == 'm') {
        number = token.substr(0, token.size() - 1);
        multiplier = 60.0f;
    }

    try {
        std::size_t parsed = 0;
        const float value = std::stof(number, &parsed);
        if (parsed != number.size() || value < 0.0f || !std::isfinite(value)) {
            return false;
        }
        seconds = value * multiplier;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool parseNonNegativeInt(const std::string& token, int& value) {
    try {
        std::size_t parsed = 0;
        const int parsedValue = std::stoi(token, &parsed);
        if (parsed != token.size() || parsedValue < 0) {
            return false;
        }
        value = parsedValue;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool parseFloatValue(const std::string& token, float& value) {
    try {
        std::size_t parsed = 0;
        const float parsedValue = std::stof(token, &parsed);
        if (parsed != token.size() || !std::isfinite(parsedValue)) {
            return false;
        }
        value = parsedValue;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool parseKeyToken(const std::string& rawToken, int& key) {
    static const std::unordered_map<std::string, int> namedKeys = {
        {"LEFT", GLFW_KEY_LEFT},
        {"RIGHT", GLFW_KEY_RIGHT},
        {"UP", GLFW_KEY_UP},
        {"DOWN", GLFW_KEY_DOWN},
        {"KEY_W", GLFW_KEY_W},
        {"KEY_A", GLFW_KEY_A},
        {"KEY_S", GLFW_KEY_S},
        {"KEY_D", GLFW_KEY_D},
        {"ENTER", GLFW_KEY_ENTER},
        {"RETURN", GLFW_KEY_ENTER},
        {"SPACE", GLFW_KEY_SPACE},
        {"CTRL", GLFW_KEY_LEFT_CONTROL},
        {"CONTROL", GLFW_KEY_LEFT_CONTROL},
        {"SHIFT", GLFW_KEY_LEFT_SHIFT},
        {"PLUS", GLFW_KEY_EQUAL},
        {"MINUS", GLFW_KEY_MINUS},
        {"F1", GLFW_KEY_F1},
        {"F2", GLFW_KEY_F2}
    };

    if (rawToken == "L") { key = GLFW_KEY_LEFT; return true; }
    if (rawToken == "R") { key = GLFW_KEY_RIGHT; return true; }
    if (rawToken == "U") { key = GLFW_KEY_UP; return true; }
    if (rawToken == "D") { key = GLFW_KEY_DOWN; return true; }
    if (rawToken == "w" || rawToken == "W") { key = GLFW_KEY_W; return true; }
    if (rawToken == "a" || rawToken == "A") { key = GLFW_KEY_A; return true; }
    if (rawToken == "s" || rawToken == "S") { key = GLFW_KEY_S; return true; }
    if (rawToken == "d") { key = GLFW_KEY_D; return true; }

    const auto it = namedKeys.find(upper(rawToken));
    if (it == namedKeys.end()) {
        return false;
    }
    key = it->second;
    return true;
}

bool parseKeyChord(const std::string& chord, std::vector<int>& keys, std::ostream& out) {
    for (const auto& token : splitKeyChord(chord)) {
        int key = GLFW_KEY_UNKNOWN;
        if (!parseKeyToken(token, key)) {
            out << "Unknown key token: " << token << std::endl;
            return false;
        }
        keys.push_back(key);
    }
    return !keys.empty();
}

void printAgentDebugHelp(std::ostream& out) {
    out << "Agent debug commands:\n"
        << "  <keys> <duration>       Hold keys while advancing scripted time, e.g. L 6s or U+L 500ms\n"
        << "  hold <keys> <duration>  Same as shorthand\n"
        << "  tap <keys>              Press keys for one 1/60s frame\n"
        << "  wait <duration>         Advance time with no input\n"
        << "  dump                    Print machine-readable game state\n"
        << "  screenshot [path]       Render and save a PNG; default is debug-screenshots/agent-debug-N.png\n"
        << "  invincible [on|off]     Toggle or set player/current-vehicle invincibility\n"
        << "  wanted <level>          Set wanted level; 0 clears the chase\n"
        << "  weapon <type> [ammo]    Grant and equip a weapon, e.g. weapon pistol 50\n"
        << "  vehicle <type>          Spawn a configured vehicle near the player, e.g. vehicle pickup\n"
        << "  vehicle_at <type> <x> <y> <heading>  Spawn a vehicle at an exact world position\n"
        << "  officer_at <x> <y> <heading>         Spawn an on-foot officer at an exact position\n"
        << "  teleport <x> <y> [heading]           Move the player/current vehicle to an exact world position\n"
        << "  enter [radius]         Enter the nearest vehicle, default radius 6\n"
        << "  vehicles                List configured vehicle type ids\n"
        << "  cheat <command...>      Optional prefix for cheat commands\n"
        << "  help                    Show this help\n"
        << "  quit                    Exit\n"
        << "Keys: L/R/U/D are arrow keys; w/a/s/d are WASD (use lowercase d for the D key),\n"
        << "      KEY_D, ENTER, SPACE, CTRL, SHIFT, PLUS, MINUS, F1, F2 are also supported.\n";
}
}

Engine::Engine()
    : m_running(false)
    , m_deltaTime(0.0f)
    , m_imguiInitialized(false)
    , m_agentDebugMode(false)
    , m_agentDebugTimeSeconds(0.0)
    , m_agentDebugScreenshotIndex(1) {
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
    if (m_agentDebugMode) {
        m_window->setVSyncEnabled(false);
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
    
    // Load vehicle configuration
    if (!VehicleConfig::getInstance().loadFromFile("assets/vehicles.json")) {
        std::cerr << "Warning: Failed to load vehicles.json, using defaults" << std::endl;
    }

    TextureManager::instance().preloadTexturesFromDirectory("assets");
    
    // Set up window resize callback to update renderer projection
    m_window->setResizeCallback([this](int w, int h) {
        if (m_renderer) {
            m_renderer->onWindowResize(w, h);
        }
    });
    
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
    
    // Initialize game logic
    m_gameLogic = std::make_unique<GameLogic>();
    
    // Initialize scene
    m_scene = std::make_unique<Scene>();
    m_scene->initialize(m_gameLogic.get(), m_window.get(), m_renderer.get());
    
    m_running = true;
    m_lastTime = std::chrono::high_resolution_clock::now();
    
    std::cout << "Engine initialized successfully" << std::endl;
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GPU: " << glGetString(GL_RENDERER) << std::endl;
    
    return true;
}

void Engine::run() {
    if (m_agentDebugMode) {
        runAgentDebugPrompt();
        return;
    }

    while (m_running && !m_window->shouldClose()) {
        calculateDeltaTime();
        runFrame(m_deltaTime, true);
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

void Engine::runFrame(float deltaTime, bool processInputAndUpdate, const std::string& screenshotPath) {
    if (!m_window || !m_renderer) {
        return;
    }

    m_window->pollEvents();
    m_deltaTime = deltaTime;

    if (processInputAndUpdate) {
        processInput();
        update(deltaTime);
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    render();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (!screenshotPath.empty()) {
        if (m_renderer->saveScreenshot(screenshotPath, m_window->getWidth(), m_window->getHeight())) {
            std::cout << "screenshot.path=" << screenshotPath << std::endl;
        }
    }

    m_window->swapBuffers();
}

void Engine::advanceAgentDebugTime(float durationSeconds, const std::vector<int>& heldKeys) {
    if (!m_inputManager) {
        return;
    }

    for (int key : heldKeys) {
        m_inputManager->setSyntheticKey(key, true);
    }

    float remaining = durationSeconds;
    if (remaining == 0.0f) {
        runFrame(0.0f, true);
    }
    while (remaining > 0.0f && m_running && m_window && !m_window->shouldClose()) {
        const float step = std::min(kAgentDebugFixedStepSeconds, remaining);
        runFrame(step, true);
        m_agentDebugTimeSeconds += step;
        remaining -= step;
    }

    for (int key : heldKeys) {
        m_inputManager->setSyntheticKey(key, false);
    }
}

std::string Engine::nextAgentDebugScreenshotPath() {
    std::ostringstream path;
    path << "debug-screenshots/agent-debug-" << m_agentDebugScreenshotIndex++ << ".png";
    return path.str();
}

void Engine::dumpAgentDebugState(std::ostream& out) const {
    if (!m_scene) {
        out << "BEGIN_GAME_STATE\nscene=null\nEND_GAME_STATE" << std::endl;
        return;
    }
    m_scene->dumpDebugState(out, m_agentDebugTimeSeconds);
}

bool Engine::runAgentDebugCommand(const std::string& rawLine) {
    std::string line = trim(rawLine);
    if (line.empty()) {
        return true;
    }

    const auto comment = line.find('#');
    if (comment != std::string::npos) {
        line = trim(line.substr(0, comment));
        if (line.empty()) {
            return true;
        }
    }

    std::vector<std::string> tokens = splitWhitespace(line);
    if (tokens.empty()) {
        return true;
    }

    if (upper(tokens[0]) == "CHEAT") {
        tokens.erase(tokens.begin());
        if (tokens.empty()) {
            std::cout << "Usage: cheat <invincible|wanted|weapon|vehicle> ..." << std::endl;
            return true;
        }
    }

    const std::string command = upper(tokens[0]);
    if (command == "QUIT" || command == "EXIT") {
        m_running = false;
        return false;
    }
    if (command == "HELP") {
        printAgentDebugHelp(std::cout);
        return true;
    }
    if (command == "DUMP") {
        dumpAgentDebugState(std::cout);
        return true;
    }
    if (command == "SCREENSHOT") {
        const std::string path = tokens.size() >= 2 ? tokens[1] : nextAgentDebugScreenshotPath();
        runFrame(0.0f, false, path);
        return true;
    }
    if (command == "INVINCIBLE" || command == "GOD" || command == "GODMODE") {
        if (!m_scene) {
            std::cout << "No scene is available." << std::endl;
            return true;
        }
        bool enabled = !m_scene->isDebugInvincible();
        if (tokens.size() >= 2) {
            const std::string value = upper(tokens[1]);
            if (value == "ON" || value == "TRUE" || value == "1") {
                enabled = true;
            } else if (value == "OFF" || value == "FALSE" || value == "0") {
                enabled = false;
            } else if (value != "TOGGLE") {
                std::cout << "Usage: invincible [on|off|toggle]" << std::endl;
                return true;
            }
        }
        m_scene->setDebugInvincible(enabled);
        return true;
    }
    if (command == "WANTED") {
        int wantedLevel = 0;
        if (tokens.size() != 2 || !parseNonNegativeInt(tokens[1], wantedLevel) || !m_scene
            || !m_scene->setDebugWantedLevel(wantedLevel)) {
            std::cout << "Usage: wanted <non-negative-level>" << std::endl;
            return true;
        }
        return true;
    }
    if (command == "WEAPON") {
        int ammo = 999;
        if (tokens.size() >= 3 && !parseNonNegativeInt(tokens[2], ammo)) {
            std::cout << "Usage: weapon <pistol|machine_gun> [ammo]" << std::endl;
            return true;
        }
        if (tokens.size() < 2 || tokens.size() > 3 || !m_scene || !m_scene->grantDebugWeapon(tokens[1], ammo)) {
            std::cout << "Usage: weapon <pistol|machine_gun> [ammo]" << std::endl;
            return true;
        }
        return true;
    }
    if (command == "VEHICLE" || command == "SPAWN_VEHICLE") {
        if (tokens.size() != 2 || !m_scene || !m_scene->spawnDebugVehicle(tokens[1])) {
            std::cout << "Usage: vehicle <type>. Known types:";
            for (const auto& def : VehicleConfig::getInstance().getAllDefinitions()) {
                std::cout << " " << def.id;
            }
            std::cout << std::endl;
            return true;
        }
        return true;
    }
    if (command == "VEHICLE_AT") {
        float x = 0.0f;
        float y = 0.0f;
        float heading = 0.0f;
        if (tokens.size() != 5 || !parseFloatValue(tokens[2], x) || !parseFloatValue(tokens[3], y)
            || !parseFloatValue(tokens[4], heading) || !m_scene
            || !m_scene->spawnDebugVehicleAt(tokens[1], glm::vec3(x, y, 0.1f), heading)) {
            std::cout << "Usage: vehicle_at <type> <x> <y> <heading>" << std::endl;
            return true;
        }
        return true;
    }
    if (command == "OFFICER_AT") {
        float x = 0.0f;
        float y = 0.0f;
        float heading = 0.0f;
        if (tokens.size() != 4 || !parseFloatValue(tokens[1], x) || !parseFloatValue(tokens[2], y)
            || !parseFloatValue(tokens[3], heading) || !m_scene
            || !m_scene->spawnDebugOfficerAt(glm::vec3(x, y, 0.1f), heading)) {
            std::cout << "Usage: officer_at <x> <y> <heading>" << std::endl;
            return true;
        }
        return true;
    }
    if (command == "TELEPORT" || command == "TP") {
        float x = 0.0f;
        float y = 0.0f;
        float heading = 90.0f;
        if (tokens.size() != 3 && tokens.size() != 4) {
            std::cout << "Usage: teleport <x> <y> [heading]" << std::endl;
            return true;
        }
        if (!parseFloatValue(tokens[1], x) || !parseFloatValue(tokens[2], y) ||
            (tokens.size() == 4 && !parseFloatValue(tokens[3], heading)) || !m_scene ||
            !m_scene->setDebugPlayerPose(glm::vec3(x, y, 0.1f), heading)) {
            std::cout << "Usage: teleport <x> <y> [heading]" << std::endl;
            return true;
        }
        return true;
    }
    if (command == "ENTER") {
        float radius = 6.0f;
        if (tokens.size() > 2 || (tokens.size() == 2 && !parseFloatValue(tokens[1], radius)) || !m_scene
            || !m_scene->debugEnterNearestVehicle(radius)) {
            std::cout << "Usage: enter [radius]" << std::endl;
            return true;
        }
        return true;
    }
    if (command == "VEHICLES") {
        std::cout << "vehicle.types=";
        const auto& definitions = VehicleConfig::getInstance().getAllDefinitions();
        for (std::size_t i = 0; i < definitions.size(); ++i) {
            if (i > 0) {
                std::cout << ",";
            }
            std::cout << definitions[i].id;
        }
        std::cout << std::endl;
        return true;
    }

    std::vector<int> keys;
    float durationSeconds = 0.0f;

    if (command == "WAIT" || command == "STEP") {
        if (tokens.size() != 2 || !parseDurationSeconds(tokens[1], durationSeconds)) {
            std::cout << "Usage: " << tokens[0] << " <duration>" << std::endl;
            return true;
        }
        advanceAgentDebugTime(durationSeconds, keys);
        return true;
    }

    if (command == "TAP") {
        if (tokens.size() != 2 || !parseKeyChord(tokens[1], keys, std::cout)) {
            std::cout << "Usage: tap <keys>" << std::endl;
            return true;
        }
        advanceAgentDebugTime(kAgentDebugFixedStepSeconds, keys);
        return true;
    }

    if (command == "HOLD") {
        if (tokens.size() != 3 || !parseKeyChord(tokens[1], keys, std::cout)
            || !parseDurationSeconds(tokens[2], durationSeconds)) {
            std::cout << "Usage: hold <keys> <duration>" << std::endl;
            return true;
        }
        advanceAgentDebugTime(durationSeconds, keys);
        return true;
    }

    if (tokens.size() == 2 && parseKeyChord(tokens[0], keys, std::cout)
        && parseDurationSeconds(tokens[1], durationSeconds)) {
        advanceAgentDebugTime(durationSeconds, keys);
        return true;
    }

    std::cout << "Unrecognized agent debug command. Type 'help' for commands." << std::endl;
    return true;
}

void Engine::runAgentDebugPrompt() {
    std::cout << "Agent debug mode enabled. Type 'help' for commands." << std::endl;
    runFrame(0.0f, false);
    dumpAgentDebugState(std::cout);

    std::string line;
    while (m_running && m_window && !m_window->shouldClose()) {
        std::cout << "agent-debug> " << std::flush;
        if (!std::getline(std::cin, line)) {
            break;
        }
        runAgentDebugCommand(line);
    }
}
