#include "renderer/gl3d/ShaderProgram.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <format>
#include <print>

namespace openorbit::renderer {

ShaderProgram::~ShaderProgram() {
    if (m_id) { glDeleteProgram(m_id); m_id = 0; }
}

ShaderProgram::ShaderProgram(ShaderProgram&& rhs) noexcept
    : m_id(rhs.m_id), m_uniformCache(std::move(rhs.m_uniformCache))
{
    rhs.m_id = 0;
}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& rhs) noexcept {
    if (this != &rhs) {
        if (m_id) glDeleteProgram(m_id);
        m_id             = rhs.m_id;
        m_uniformCache   = std::move(rhs.m_uniformCache);
        rhs.m_id = 0;
    }
    return *this;
}

std::string ShaderProgram::readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error(std::format("ShaderProgram: cannot open '{}'", path));
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

GLuint ShaderProgram::compileShader(GLenum type, const std::string& src) {
    GLuint sh = glCreateShader(type);
    const char* ptr = src.c_str();
    glShaderSource(sh, 1, &ptr, nullptr);
    glCompileShader(sh);

    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint logLen = 0;
        glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &logLen);
        std::string log(logLen, '\0');
        glGetShaderInfoLog(sh, logLen, nullptr, log.data());
        glDeleteShader(sh);
        throw std::runtime_error(std::format("Shader compile error:\n{}", log));
    }
    return sh;
}

void ShaderProgram::load(const std::string& vert_path, const std::string& frag_path) {
    loadFromString(readFile(vert_path), readFile(frag_path));
}

void ShaderProgram::loadFromString(const std::string& vert_src, const std::string& frag_src) {
    if (m_id) { glDeleteProgram(m_id); m_id = 0; }
    m_uniformCache.clear();

    GLuint vert = compileShader(GL_VERTEX_SHADER,   vert_src);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, frag_src);

    m_id = glCreateProgram();
    glAttachShader(m_id, vert);
    glAttachShader(m_id, frag);
    glLinkProgram(m_id);

    GLint ok = 0;
    glGetProgramiv(m_id, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint logLen = 0;
        glGetProgramiv(m_id, GL_INFO_LOG_LENGTH, &logLen);
        std::string log(logLen, '\0');
        glGetProgramInfoLog(m_id, logLen, nullptr, log.data());
        glDeleteProgram(m_id); m_id = 0;
        glDeleteShader(vert);  glDeleteShader(frag);
        throw std::runtime_error(std::format("Shader link error:\n{}", log));
    }

    glDeleteShader(vert);
    glDeleteShader(frag);
}

void ShaderProgram::use()   const noexcept { if (m_id) glUseProgram(m_id); }
void ShaderProgram::unuse() const noexcept { glUseProgram(0); }

GLint ShaderProgram::location(const std::string& name) {
    auto it = m_uniformCache.find(name);
    if (it != m_uniformCache.end()) return it->second;
    GLint loc = glGetUniformLocation(m_id, name.c_str());
    if (loc == -1)
        std::print(stderr, "ShaderProgram: uniform '{}' not found\n", name);
    m_uniformCache[name] = loc;
    return loc;
}

void ShaderProgram::setFloat(const std::string& n, float v)              { glUniform1f(location(n), v); }
void ShaderProgram::setInt  (const std::string& n, int   v)              { glUniform1i(location(n), v); }
void ShaderProgram::setVec2 (const std::string& n, float x, float y)     { glUniform2f(location(n), x, y); }
void ShaderProgram::setVec3 (const std::string& n, float x, float y, float z) { glUniform3f(location(n), x, y, z); }
void ShaderProgram::setVec3 (const std::string& n, const Eigen::Vector3f& v)   { glUniform3f(location(n), v.x(), v.y(), v.z()); }
void ShaderProgram::setVec4 (const std::string& n, float x, float y, float z, float w) { glUniform4f(location(n), x, y, z, w); }
void ShaderProgram::setMat4 (const std::string& n, const float* d, bool t) {
    glUniformMatrix4fv(location(n), 1, t ? GL_TRUE : GL_FALSE, d);
}

} // namespace openorbit::renderer
