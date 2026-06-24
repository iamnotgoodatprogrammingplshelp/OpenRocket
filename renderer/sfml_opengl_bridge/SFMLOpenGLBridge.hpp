#pragma once
#include <SFML/Graphics.hpp>
#include <GL/gl.h>
#include <cstdint>

namespace openorbit::renderer {

/**
 * @brief Manages safe interleaving of SFML 2D and raw OpenGL 4.6 rendering.
 *
 * ## Why this class exists
 * SFML and OpenGL share one GPU context but maintain separate state machines.
 * SFML caches VAO / VBO / shader bindings internally; raw OpenGL calls can
 * silently corrupt those caches. The bridge prevents corruption by:
 *   1. Saving SFML GL state before any raw glXxx() block (pushGLStates)
 *   2. Restoring it after the OpenGL block (popGLStates)
 *   3. Blitting the rendered scene from an FBO into a SFML-compatible texture
 *      so SFML can composit the HUD over the 3D scene without knowing about
 *      the FBO.
 *
 * ## Render order each frame
 * ```
 * bridge.beginGL()          ← activates GL context, binds scene FBO
 *   // ... all OpenGL 3D drawing ...
 * bridge.endGL()            ← resolves FBO → SFML texture, pushes GL states
 *   // ... SFML 2D HUD drawing on window ...
 * bridge.present()          ← pops GL states, nothing else needed
 * ```
 */
class SFMLOpenGLBridge {
public:
    /**
     * @param window   The RenderWindow that owns the shared GL context.
     * @param width    Framebuffer width  [px]
     * @param height   Framebuffer height [px]
     */
    SFMLOpenGLBridge(sf::RenderWindow& window, unsigned width, unsigned height);
    ~SFMLOpenGLBridge();

    // Non-copyable
    SFMLOpenGLBridge(const SFMLOpenGLBridge&)            = delete;
    SFMLOpenGLBridge& operator=(const SFMLOpenGLBridge&) = delete;

    /**
     * @brief Begin OpenGL rendering pass.
     *
     * - Makes the window's context current
     * - Binds the HDR scene FBO as the render target
     * - Clears colour and depth
     *
     * @pre Must be called at the start of the OpenGL 3D drawing section.
     */
    void beginGL();

    /**
     * @brief End OpenGL rendering pass and composite into the SFML window.
     *
     * - Runs the tone-mapping pass: HDR FBO → 8-bit LDR back-buffer
     * - Copies the result into m_sfmlTexture (visible to SFML as a Sprite)
     * - Calls window.pushGLStates() — SFML drawing can now happen safely
     * - Draws the 3D scene sprite to the window at (0,0)
     *
     * @post SFML 2D drawing (HUD gauges, text, overlays) may happen now.
     */
    void endGL();

    /**
     * @brief Restore GL state after SFML has finished its 2D pass.
     *
     * Calls window.popGLStates(). Must be called after all SFML drawing.
     */
    void present();

    /**
     * @brief Resize the internal FBO and texture (e.g., on window resize).
     */
    void resize(unsigned width, unsigned height);

    // ── Accessors for the scene FBO (3D render targets) ──────────────────────

    [[nodiscard]] GLuint colorFBO()  const noexcept { return m_fbo; }
    [[nodiscard]] GLuint depthRBO()  const noexcept { return m_depthRBO; }
    [[nodiscard]] GLuint hdrTexture() const noexcept { return m_hdrTex; }

private:
    void createFBO(unsigned w, unsigned h);
    void destroyFBO();
    void runTonemapping();

    sf::RenderWindow& m_window;
    unsigned          m_width;
    unsigned          m_height;

    // Scene HDR FBO
    GLuint m_fbo     = 0;
    GLuint m_hdrTex  = 0;   ///< RGBA16F colour attachment
    GLuint m_depthRBO = 0;  ///< depth+stencil renderbuffer

    // Tone-mapping pass
    GLuint m_tonemapFBO  = 0;
    GLuint m_tonemapTex  = 0;  ///< RGBA8 output, copied into SFML texture
    GLuint m_quadVAO     = 0;
    GLuint m_quadVBO     = 0;
    GLuint m_tonemapProg = 0;  ///< fullscreen triangle shader

    // SFML interface
    sf::Texture m_sfmlTexture;
    sf::Sprite  m_sfmlSprite;

    std::vector<sf::Uint8> m_pixelBuffer; ///< CPU-side staging for texture upload
};

} // namespace openorbit::renderer
