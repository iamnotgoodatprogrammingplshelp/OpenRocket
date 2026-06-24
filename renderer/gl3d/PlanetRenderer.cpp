#include "renderer/gl3d/PlanetRenderer.hpp"
#include <GL/gl.h>
#include <cmath>
#include <numbers>
#include <vector>

namespace openorbit::renderer {

// ─────────────────────────────────────────────────────────────────────────────
// Embedded planet shaders (GLSL 460)
// Phase 2: solid-colour PBR sphere. Phase 3 adds heightmap displacement.
// ─────────────────────────────────────────────────────────────────────────────

static const char* kPlanetVert = R"GLSL(
#version 460 core

layout(location = 0) in vec3 aPosition;   // unit-sphere vertex
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

uniform mat4  uModel;
uniform mat4  uView;
uniform mat4  uProjection;
uniform float uPlanetRadius;

out vec3 vNormalWorld;
out vec3 vPosWorld;
out vec2 vUV;

void main() {
    vec3 scaledPos = aPosition * uPlanetRadius;
    vec4 worldPos  = uModel * vec4(scaledPos, 1.0);
    vPosWorld      = worldPos.xyz;
    vNormalWorld   = normalize(mat3(transpose(inverse(uModel))) * aNormal);
    vUV            = aTexCoord;
    gl_Position    = uProjection * uView * worldPos;
}
)GLSL";

static const char* kPlanetFrag = R"GLSL(
#version 460 core

in vec3 vNormalWorld;
in vec3 vPosWorld;
in vec2 vUV;

uniform vec3  uSunDirection;    // unit vector toward sun, world space
uniform vec3  uCameraPos;
uniform vec3  uBaseColor;       // planet surface base colour (used when no texture)
uniform float uRoughness;       // [0=mirror, 1=matte]
uniform float uAmbient;         // ambient light factor

out vec4 fragColor;

// Cook-Torrance microfacet BRDF (simplified)
float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdH = max(dot(N, H), 0.0);
    float denom = NdH * NdH * (a2 - 1.0) + 1.0;
    return a2 / (3.14159265 * denom * denom);
}

void main() {
    vec3 N = normalize(vNormalWorld);
    vec3 L = normalize(uSunDirection);
    vec3 V = normalize(uCameraPos - vPosWorld);
    vec3 H = normalize(L + V);

    float NdL = max(dot(N, L), 0.0);
    float D   = distributionGGX(N, H, uRoughness);
    float G   = clamp(dot(N, V), 0.0, 1.0) * clamp(dot(N, L), 0.0, 1.0);

    vec3 specular = vec3(D * G) * 0.05;
    vec3 diffuse  = uBaseColor * NdL;
    vec3 ambient  = uBaseColor * uAmbient;

    fragColor = vec4(ambient + diffuse + specular, 1.0);
}
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────

struct Vertex {
    float x, y, z;   // position (unit sphere)
    float nx, ny, nz; // normal
    float u, v;        // UV
};

PlanetRenderer::PlanetRenderer(int slices, int stacks) {
    buildSphere(slices, stacks);
    buildShader();
}

PlanetRenderer::~PlanetRenderer() {
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_ibo) glDeleteBuffers(1, &m_ibo);
}

void PlanetRenderer::buildSphere(int slices, int stacks) {
    std::vector<Vertex>        verts;
    std::vector<unsigned int>  indices;

    for (int j = 0; j <= stacks; ++j) {
        float phi = std::numbers::pi_v<float> * j / stacks; // 0 → π (N to S pole)
        float cp  = std::cos(phi), sp = std::sin(phi);

        for (int i = 0; i <= slices; ++i) {
            float theta = 2.0f * std::numbers::pi_v<float> * i / slices;
            float ct = std::cos(theta), st = std::sin(theta);

            Vertex v;
            v.x  = sp * ct;  v.y  = cp;  v.z  = sp * st;
            v.nx = v.x; v.ny = v.y; v.nz = v.z; // unit sphere: normal == position
            v.u  = static_cast<float>(i) / slices;
            v.v  = static_cast<float>(j) / stacks;
            verts.push_back(v);
        }
    }

    for (int j = 0; j < stacks; ++j) {
        for (int i = 0; i < slices; ++i) {
            unsigned a = j * (slices+1) + i;
            unsigned b = a + 1;
            unsigned c = a + slices + 1;
            unsigned d = c + 1;
            // Two triangles per quad
            indices.insert(indices.end(), {a, c, b});
            indices.insert(indices.end(), {b, c, d});
        }
    }

    m_indexCount = static_cast<GLsizei>(indices.size());

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ibo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 verts.size() * sizeof(Vertex),
                 verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 indices.size() * sizeof(unsigned int),
                 indices.data(), GL_STATIC_DRAW);

    // aPosition (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, x)));
    // aNormal (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, nx)));
    // aTexCoord (location 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, u)));

    glBindVertexArray(0);
}

void PlanetRenderer::buildShader() {
    m_shader.loadFromString(kPlanetVert, kPlanetFrag);
}

void PlanetRenderer::draw(const world::CelestialBodyDef& body,
                           const float* view,
                           const float* proj,
                           const Eigen::Vector3f& cameraPos,
                           const Eigen::Vector3f& sunDir)
{
    m_shader.use();

    // Model matrix: identity (planet is at origin; camera is offset)
    float identity[16] = {
        1,0,0,0,  0,1,0,0,  0,0,1,0,  0,0,0,1
    };
    m_shader.setMat4("uModel",      identity);
    m_shader.setMat4("uView",       view);
    m_shader.setMat4("uProjection", proj);
    m_shader.setFloat("uPlanetRadius", static_cast<float>(body.radius_m));
    m_shader.setVec3("uSunDirection", sunDir);
    m_shader.setVec3("uCameraPos",   cameraPos);
    m_shader.setFloat("uRoughness",  0.7f);
    m_shader.setFloat("uAmbient",    0.05f);

    // Body-specific colour heuristic
    Eigen::Vector3f color{0.3f, 0.5f, 0.8f}; // default: blue (Earth)
    if (body.name == "Moon")  color = {0.6f, 0.6f, 0.6f};
    if (body.name == "Mars")  color = {0.7f, 0.3f, 0.2f};
    if (body.name == "Sun")   color = {1.0f, 0.9f, 0.5f};
    m_shader.setVec3("uBaseColor", color);

    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    m_shader.unuse();
}

} // namespace openorbit::renderer
