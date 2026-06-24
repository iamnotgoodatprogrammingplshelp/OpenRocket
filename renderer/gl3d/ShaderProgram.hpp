#pragma once
#include <GL/gl.h>
#include <string>
#include <unordered_map>
#include <Eigen/Dense>

namespace openorbit::renderer {

/**
 * @brief RAII wrapper around a linked OpenGL shader program.
 *
 * Loads and compiles vertex + fragment (and optionally geometry/compute)
 * shader source files. Caches uniform locations on first use.
 */
class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram();

    // Non-copyable
    ShaderProgram(const ShaderProgram&)            = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    // Movable
    ShaderProgram(ShaderProgram&& rhs) noexcept;
    ShaderProgram& operator=(ShaderProgram&& rhs) noexcept;

    /**
     * @brief Load and compile from shader source files.
     * @param vert_path  Path to GLSL vertex shader
     * @param frag_path  Path to GLSL fragment shader
     */
    void load(const std::string& vert_path, const std::string& frag_path);

    /**
     * @brief Load from in-memory GLSL source strings.
     */
    void loadFromString(const std::string& vert_src, const std::string& frag_src);

    void use() const noexcept;
    void unuse() const noexcept;

    [[nodiscard]] GLuint id() const noexcept { return m_id; }
    [[nodiscard]] bool   valid() const noexcept { return m_id != 0; }

    // ── Uniform setters ──────────────────────────────────────────────────────
    void setFloat(const std::string& name, float v);
    void setInt  (const std::string& name, int   v);
    void setVec2 (const std::string& name, float x, float y);
    void setVec3 (const std::string& name, float x, float y, float z);
    void setVec3 (const std::string& name, const Eigen::Vector3f& v);
    void setVec4 (const std::string& name, float x, float y, float z, float w);
    void setMat4 (const std::string& name, const float* data, bool transpose = false);

private:
    GLint location(const std::string& name);
    static GLuint compileShader(GLenum type, const std::string& src);
    static std::string readFile(const std::string& path);

    GLuint m_id = 0;
    std::unordered_map<std::string, GLint> m_uniformCache;
};

} // namespace openorbit::renderer
