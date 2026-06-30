#include "renderer/window/AppWindow.hpp"
#include <GL/gl.h>
#include <stdexcept>
#include <format>
#include <print>

#ifdef OPENORBIT_HAS_IMGUI
#include <imgui-SFML.h>
#include <imgui.h>
#endif

namespace openorbit::renderer {

AppWindow::AppWindow(unsigned width, unsigned height,
                     const std::string& title, bool fullscreen)
{
    onCreate(width, height, title, fullscreen);
}

AppWindow::~AppWindow() {
#ifdef OPENORBIT_HAS_IMGUI
    ImGui::SFML::Shutdown();
#endif
}

void AppWindow::onCreate(unsigned w, unsigned h,
                          const std::string& title, bool fullscreen)
{
    // Request OpenGL 4.6 core-profile context through SFML
    sf::ContextSettings glSettings;
    glSettings.depthBits         = 24;
    glSettings.stencilBits       = 8;
    glSettings.antialiasingLevel = 4;
    glSettings.majorVersion      = 4;
    glSettings.minorVersion      = 6;
    glSettings.attributeFlags    = sf::ContextSettings::Core;

    sf::VideoMode mode;
    sf::Uint32    style;

    if (fullscreen) {
        mode  = sf::VideoMode::getDesktopMode();
        style = sf::Style::Fullscreen;
    } else {
        mode  = sf::VideoMode(w, h);
        style = sf::Style::Default;
    }

    m_window.create(mode, title, style, glSettings);
    m_window.setVerticalSyncEnabled(true);
    m_window.setFramerateLimit(0); // uncapped; we manage timing manually

    // Verify we got the context we asked for
    auto& ctx = m_window.getSettings();
    if (ctx.majorVersion < 4) {
        throw std::runtime_error(
            std::format("OpenOrbit requires OpenGL 4.x but got {}.{}",
                        ctx.majorVersion, ctx.minorVersion));
    }

    std::print("OpenOrbit: OpenGL {}.{} context (depth={}, stencil={}, MSAA={}x)\n",
               ctx.majorVersion, ctx.minorVersion,
               ctx.depthBits, ctx.stencilBits, ctx.antialiasingLevel);

#ifdef OPENORBIT_HAS_IMGUI
    if (!ImGui::SFML::Init(m_window)) {
        throw std::runtime_error("Failed to initialise ImGui-SFML");
    }
#endif

    m_clock.restart();
}

// ─────────────────────────────────────────────────────────────────────────────
// Event loop
// ─────────────────────────────────────────────────────────────────────────────

void AppWindow::pollEvents() {
    m_dt = m_clock.restart().asSeconds();

#ifdef OPENORBIT_HAS_IMGUI
    ImGui::SFML::Update(m_window, sf::seconds(m_dt));
#endif

    sf::Event event;
    while (m_window.pollEvent(event)) {
#ifdef OPENORBIT_HAS_IMGUI
        ImGui::SFML::ProcessEvent(m_window, event);
#endif
        switch (event.type) {
            case sf::Event::Closed:
                if (m_inputCbs.onQuit) m_inputCbs.onQuit();
                m_window.close();
                break;

            case sf::Event::Resized:
                onResized(event.size.width, event.size.height);
                break;

            case sf::Event::KeyPressed:
                onKeyPressed(event.key);
                break;

            case sf::Event::JoystickMoved:
                // Route joystick axis to flight input mapper
                if (event.joystickMove.axis == sf::Joystick::X
                 || event.joystickMove.axis == sf::Joystick::Y) {
                    float x = sf::Joystick::getAxisPosition(0, sf::Joystick::X) / 100.0f;
                    float y = sf::Joystick::getAxisPosition(0, sf::Joystick::Y) / 100.0f;
                    // Apply dead-zone (5%)
                    auto deadzone = [](float v) {
                        return std::abs(v) < 0.05f ? 0.0f : v;
                    };
                    if (m_inputCbs.onPitchYaw)
                        m_inputCbs.onPitchYaw(deadzone(y), deadzone(x));
                }
                break;

            default:
                break;
        }
    }
}

void AppWindow::display() {
#ifdef OPENORBIT_HAS_IMGUI
    ImGui::SFML::Render(m_window);
#endif
    m_window.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// Key routing (default keybindings — all rebindable via InputCallbacks)
// ─────────────────────────────────────────────────────────────────────────────

void AppWindow::onKeyPressed(const sf::Event::KeyEvent& key) {
    using K = sf::Keyboard::Key;

    switch (key.code) {
        case K::Z:     if (m_inputCbs.onThrottleUp)   m_inputCbs.onThrottleUp();   break;
        case K::X:     if (m_inputCbs.onThrottleDown) m_inputCbs.onThrottleDown(); break;
        case K::Space: if (m_inputCbs.onStage)         m_inputCbs.onStage();        break;
        case K::M:     if (m_inputCbs.onToggleMap)     m_inputCbs.onToggleMap();    break;
        case K::H:     if (m_inputCbs.onToggleHUD)     m_inputCbs.onToggleHUD();    break;
        case K::V:     if (m_inputCbs.onToggleVAB)     m_inputCbs.onToggleVAB();    break;
        case K::Escape:
            if (m_inputCbs.onQuit) m_inputCbs.onQuit();
            m_window.close();
            break;
        default:
            break;
    }
}

void AppWindow::onResized(unsigned w, unsigned h) {
    // Update SFML view to match new window size
    m_window.setView(sf::View(sf::FloatRect(0.f, 0.f,
                                             static_cast<float>(w),
                                             static_cast<float>(h))));
    // Update OpenGL viewport
    m_window.setActive(true);
    glViewport(0, 0, static_cast<GLsizei>(w), static_cast<GLsizei>(h));
}

// ─────────────────────────────────────────────────────────────────────────────
// GL context helpers
// ─────────────────────────────────────────────────────────────────────────────

void AppWindow::activateGL() {
    m_window.setActive(true);
    m_glActive = true;
}

void AppWindow::deactivateGL() {
    if (m_glActive) {
        m_window.pushGLStates();
        m_glActive = false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Screenshot
// ─────────────────────────────────────────────────────────────────────────────

void AppWindow::screenshot(const std::string& path) {
    sf::Vector2u size = m_window.getSize();
    sf::Texture tex;
    tex.create(size.x, size.y);
    tex.update(m_window);
    sf::Image img = tex.copyToImage();
    if (!img.saveToFile(path)) {
        std::print(stderr, "OpenOrbit: failed to save screenshot to {}\n", path);
    }
}

} // namespace openorbit::renderer
