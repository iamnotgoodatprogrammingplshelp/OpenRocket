#pragma once
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <functional>
#include <string>

namespace openorbit::renderer {

/**
 * @brief Input event callback map — all keybindings route through here.
 */
struct InputCallbacks {
    std::function<void()> onThrottleUp;
    std::function<void()> onThrottleDown;
    std::function<void()> onStage;
    std::function<void()> onToggleMap;
    std::function<void()> onToggleHUD;
    std::function<void(float, float)> onPitchYaw;  ///< pitch, yaw in [-1,1]
    std::function<void()> onQuit;
};

/**
 * @brief Manages the SFML RenderWindow with an OpenGL 4.6 core-profile context.
 *
 * This is the root of the graphics stack. It owns the window, drives the
 * event loop, and forwards events to ImGui-SFML (if compiled in) and to the
 * InputCallbacks registered by the application.
 *
 * ## SFML–OpenGL sharing contract
 * The window creates an OpenGL 4.6 core-profile context with:
 *   - 24-bit depth buffer
 *   - 8-bit stencil buffer
 *   - 4× MSAA
 *
 * Raw OpenGL calls must be bracketed with:
 *   window.setActive(true)  ← before any glXxx()
 *   window.pushGLStates()   ← before any SFML drawing after GL
 *   window.popGLStates()    ← after SFML drawing, back to GL
 */
class AppWindow {
public:
    /**
     * @param width      Window width in pixels
     * @param height     Window height in pixels
     * @param title      Window title
     * @param fullscreen Open in fullscreen mode
     */
    explicit AppWindow(unsigned width  = 1920,
                       unsigned height = 1080,
                       const std::string& title = "OpenOrbit",
                       bool fullscreen = false);

    ~AppWindow();

    // Non-copyable, non-movable (owns GPU resources)
    AppWindow(const AppWindow&)            = delete;
    AppWindow& operator=(const AppWindow&) = delete;

    // ── Window state ─────────────────────────────────────────────────────────

    [[nodiscard]] bool isOpen()  const noexcept { return m_window.isOpen(); }
    [[nodiscard]] unsigned width()  const noexcept { return m_window.getSize().x; }
    [[nodiscard]] unsigned height() const noexcept { return m_window.getSize().y; }

    /// Access the underlying SFML window (pass to SFML draw calls)
    [[nodiscard]] sf::RenderWindow& sfml() noexcept { return m_window; }

    // ── Event loop ───────────────────────────────────────────────────────────

    /**
     * @brief Process all pending window events.
     *
     * Must be called once per frame, before any rendering. Forwards events to
     * InputCallbacks and (if enabled) ImGui-SFML.
     */
    void pollEvents();

    /**
     * @brief Present the back buffer to the screen.
     *
     * Calls sf::RenderWindow::display(). Call at the very end of the frame,
     * after all drawing is complete.
     */
    void display();

    // ── Input routing ─────────────────────────────────────────────────────────

    void setInputCallbacks(InputCallbacks cbs) { m_inputCbs = std::move(cbs); }

    // ── GL context helpers ────────────────────────────────────────────────────

    /// Make this window's GL context current (call before raw glXxx() calls).
    void activateGL();

    /// Call after all raw GL work, before SFML 2D drawing.
    void deactivateGL();

    // ── Screenshot ────────────────────────────────────────────────────────────

    /// Capture the current frame to a PNG file.
    void screenshot(const std::string& path);

    // ── Timing ───────────────────────────────────────────────────────────────

    [[nodiscard]] float deltaTime() const noexcept { return m_dt; }
    [[nodiscard]] float fps()       const noexcept {
        return m_dt > 0.0f ? 1.0f / m_dt : 0.0f;
    }

private:
    void onCreate(unsigned w, unsigned h, const std::string& title, bool fullscreen);
    void onKeyPressed(const sf::Event::KeyEvent& key);
    void onResized(unsigned w, unsigned h);

    sf::RenderWindow   m_window;
    InputCallbacks     m_inputCbs;
    sf::Clock          m_clock;
    float              m_dt = 0.0f;
    bool               m_glActive = false;
};

} // namespace openorbit::renderer
