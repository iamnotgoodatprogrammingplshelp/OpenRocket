#include "renderer/sfml_opengl_bridge/SFMLOpenGLBridge.hpp"
#include <GL/gl.h>
#include <stdexcept>
#include <format>
#include <print>
#include <vector>
#include <string>

namespace openorbit::renderer {

// ─────────────────────────────────────────────────────────────────────────────
// Fullscreen-triangle tone-mapping shaders (embedded GLSL 460)
// ─────────────────────────────────────────────────────────────────────────────

static const char* kTonemapVert = R"GLSL(
#version 460 core
out vec2 vUV;
void main() {
    // Generates a triangle covering the full clip space without a VBO.
    float x = float((gl_VertexID & 1) << 2) - 1.0;
    float y = float((gl_VertexID & 2) << 1) - 1.0;
    vUV = vec2((x + 1.0) * 0.5, (y + 1.0) * 0.5);
    gl_Position = vec4(x, y, 0.0, 1.0);
}
)GLSL";

static const char* kTonemapFrag = R"GLSL(
#version 460 core
in  vec2 vUV;
out vec4 fragColor;

uniform sampler2D uHDR;
uniform float     uExposure;  // default 1.0

// ACES filmic tone-map approximation (Narkowicz 2015)
vec3 aces(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

void main() {
    vec3 hdr = texture(uHDR, vUV).rgb * uExposure;
    vec3 ldr = aces(hdr);
    // Gamma correction (sRGB)
    fragColor = vec4(pow(ldr, vec3(1.0/2.2)), 1.0);
}
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static GLuint compileShader(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(sh, 512, nullptr, log);
        throw std::runtime_error(std::format("Shader compile error: {}", log));
    }
    return sh;
}

static GLuint linkProgram(GLuint vert, GLuint frag) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, 512, nullptr, log);
        throw std::runtime_error(std::format("Program link error: {}", log));
    }
    glDeleteShader(vert);
    glDeleteShader(frag);
    return prog;
}

// ─────────────────────────────────────────────────────────────────────────────
// Construction / destruction
// ─────────────────────────────────────────────────────────────────────────────

SFMLOpenGLBridge::SFMLOpenGLBridge(sf::RenderWindow& window,
                                    unsigned width, unsigned height)
    : m_window(window), m_width(width), m_height(height)
{
    m_window.setActive(true);

    createFBO(width, height);

    // Compile tone-mapping program
    GLuint vert = compileShader(GL_VERTEX_SHADER,   kTonemapVert);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, kTonemapFrag);
    m_tonemapProg = linkProgram(vert, frag);

    // No-VBO fullscreen triangle — just need a dummy VAO
    glGenVertexArrays(1, &m_quadVAO);

    // SFML texture (LDR output side)
    if (!m_sfmlTexture.create(width, height)) {
        throw std::runtime_error("Failed to create SFML composite texture");
    }
    m_sfmlSprite.setTexture(m_sfmlTexture);

    m_pixelBuffer.resize(static_cast<size_t>(width) * height * 4);

    std::print("SFMLOpenGLBridge: {}×{} HDR FBO + ACES tone-map ready\n", width, height);
}

SFMLOpenGLBridge::~SFMLOpenGLBridge() {
    m_window.setActive(true);
    destroyFBO();
    if (m_tonemapProg) glDeleteProgram(m_tonemapProg);
    if (m_quadVAO)     glDeleteVertexArrays(1, &m_quadVAO);
}

void SFMLOpenGLBridge::createFBO(unsigned w, unsigned h) {
    // ── HDR scene FBO ─────────────────────────────────────────────────────────
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    // RGBA16F HDR colour texture
    glGenTextures(1, &m_hdrTex);
    glBindTexture(GL_TEXTURE_2D, m_hdrTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_hdrTex, 0);

    // Depth+stencil renderbuffer
    glGenRenderbuffers(1, &m_depthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                               GL_RENDERBUFFER, m_depthRBO);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        throw std::runtime_error(std::format("Scene FBO incomplete: 0x{:x}", status));
    }

    // ── Tone-map output FBO (RGBA8) ────────────────────────────────────────────
    glGenFramebuffers(1, &m_tonemapFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_tonemapFBO);

    glGenTextures(1, &m_tonemapTex);
    glBindTexture(GL_TEXTURE_2D, m_tonemapTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_tonemapTex, 0);

    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error(std::format("Tonemap FBO incomplete: 0x{:x}", status));

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SFMLOpenGLBridge::destroyFBO() {
    if (m_fbo)       { glDeleteFramebuffers(1,  &m_fbo);       m_fbo = 0; }
    if (m_hdrTex)    { glDeleteTextures(1,       &m_hdrTex);   m_hdrTex = 0; }
    if (m_depthRBO)  { glDeleteRenderbuffers(1,  &m_depthRBO); m_depthRBO = 0; }
    if (m_tonemapFBO){ glDeleteFramebuffers(1,  &m_tonemapFBO);m_tonemapFBO = 0; }
    if (m_tonemapTex){ glDeleteTextures(1,       &m_tonemapTex);m_tonemapTex = 0; }
}

// ─────────────────────────────────────────────────────────────────────────────
// Frame rendering
// ─────────────────────────────────────────────────────────────────────────────

void SFMLOpenGLBridge::beginGL() {
    m_window.setActive(true);

    // Bind the HDR scene FBO
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height));
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
}

void SFMLOpenGLBridge::endGL() {
    runTonemapping();

    // Read the tone-mapped pixels into the CPU staging buffer
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_tonemapFBO);
    glReadPixels(0, 0,
                 static_cast<GLsizei>(m_width),
                 static_cast<GLsizei>(m_height),
                 GL_RGBA, GL_UNSIGNED_BYTE,
                 m_pixelBuffer.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Flip rows (OpenGL origin is bottom-left; SFML is top-left)
    const unsigned rowBytes = m_width * 4;
    std::vector<sf::Uint8> flipped(m_pixelBuffer.size());
    for (unsigned y = 0; y < m_height; ++y) {
        const sf::Uint8* src = m_pixelBuffer.data() + (m_height - 1 - y) * rowBytes;
        sf::Uint8*       dst = flipped.data()        + y * rowBytes;
        std::copy(src, src + rowBytes, dst);
    }

    // Upload to SFML texture and draw as a full-screen sprite
    m_sfmlTexture.update(flipped.data());

    // Push SFML state so SFML drawing won't corrupt our GL state
    m_window.pushGLStates();
    m_window.draw(m_sfmlSprite);
    // (HUD drawing happens here in the application loop, before present())
}

void SFMLOpenGLBridge::runTonemapping() {
    glBindFramebuffer(GL_FRAMEBUFFER, m_tonemapFBO);
    glViewport(0, 0, static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height));
    glDisable(GL_DEPTH_TEST);

    glUseProgram(m_tonemapProg);
    glUniform1i(glGetUniformLocation(m_tonemapProg, "uHDR"), 0);
    glUniform1f(glGetUniformLocation(m_tonemapProg, "uExposure"), 1.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_hdrTex);

    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3); // fullscreen triangle — no VBO needed
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
}

void SFMLOpenGLBridge::present() {
    // Restore GL state that was saved before SFML drawing
    m_window.popGLStates();
}

void SFMLOpenGLBridge::resize(unsigned w, unsigned h) {
    m_window.setActive(true);
    destroyFBO();
    m_width  = w;
    m_height = h;
    createFBO(w, h);

    if (!m_sfmlTexture.create(w, h))
        throw std::runtime_error("Failed to resize SFML composite texture");
    m_sfmlSprite.setTexture(m_sfmlTexture, true);
    m_pixelBuffer.resize(static_cast<size_t>(w) * h * 4);
}

} // namespace openorbit::renderer
