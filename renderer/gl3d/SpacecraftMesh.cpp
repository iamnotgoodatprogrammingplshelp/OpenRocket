#include "renderer/gl3d/SpacecraftMesh.hpp"
#include <GL/gl.h>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace openorbit::renderer {

// ─────────────────────────────────────────────────────────────────────────────
// PBR Shaders — Disney metallic-roughness workflow, GLSL 4.6
// ─────────────────────────────────────────────────────────────────────────────

static const char* kScVert = R"GLSL(
#version 460 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aTangent;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3  vWorldPos;
out vec3  vNormal;
out vec2  vUV;
out mat3  vTBN;

void main() {
    vec4 worldPos = uModel * vec4(aPosition, 1.0);
    vWorldPos = worldPos.xyz;

    mat3 NM = transpose(inverse(mat3(uModel)));
    vec3 N  = normalize(NM * aNormal);
    vec3 T  = normalize(NM * aTangent);
    T = normalize(T - dot(T, N) * N);
    vec3 B  = cross(N, T);
    vTBN    = mat3(T, B, N);

    vNormal = N;
    vUV     = aTexCoord;
    gl_Position = uProjection * uView * worldPos;
}
)GLSL";

static const char* kScFrag = R"GLSL(
#version 460 core

in vec3  vWorldPos;
in vec3  vNormal;
in vec2  vUV;
in mat3  vTBN;

uniform vec3  uCameraPos;
uniform vec3  uSunDir;
uniform vec3  uAlbedo;
uniform float uMetalness;
uniform float uRoughness;
uniform float uEmissive;
uniform vec3  uEmissiveColor;

out vec4 fragColor;

const float PI = 3.14159265359;

// GGX normal distribution
float D_GGX(vec3 N, vec3 H, float r) {
    float a = r * r;
    float a2 = a * a;
    float NdH = max(dot(N, H), 0.0);
    float d = NdH * NdH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

// Smith-Schlick-GGX geometry
float G_Schlick(float NdV, float r) {
    float k = (r + 1.0);
    k = (k * k) / 8.0;
    return NdV / (NdV * (1.0 - k) + k);
}

float G_Smith(vec3 N, vec3 V, vec3 L, float r) {
    float NdV = max(dot(N, V), 0.0);
    float NdL = max(dot(N, L), 0.0);
    return G_Schlick(NdV, r) * G_Schlick(NdL, r);
}

// Fresnel-Schlick
vec3 F_Schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 L = normalize(uSunDir);
    vec3 H = normalize(V + L);

    // Dielectric F0 = 0.04, metal uses albedo
    vec3 F0 = mix(vec3(0.04), uAlbedo, uMetalness);

    float NdL = max(dot(N, L), 0.0);

    // Cook-Torrance specular
    float Dv  = D_GGX(N, H, uRoughness);
    float Gv  = G_Smith(N, V, L, uRoughness);
    vec3  Fv  = F_Schlick(max(dot(H, V), 0.0), F0);

    vec3 specular = (Dv * Gv * Fv) /
                    (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 1e-4);

    // Lambertian diffuse (metals have no diffuse)
    vec3 kD = (vec3(1.0) - Fv) * (1.0 - uMetalness);
    vec3 diffuse = kD * uAlbedo / PI;

    // Sunlight (5800K warm white, ~3x to be over-exposed before tonemap)
    vec3 sunRadiance = vec3(1.0, 0.97, 0.90) * 3.0;
    vec3 Lo = (diffuse + specular) * sunRadiance * NdL;

    // Ambient: dim sky + blue earth-shine from below
    float earthshine = max(0.0, -N.y) * 0.04;
    vec3 ambient = vec3(0.025) * uAlbedo
                 + vec3(0.06, 0.12, 0.30) * earthshine;

    vec3 color = ambient + Lo + uEmissiveColor * uEmissive;

    fragColor = vec4(color, 1.0);
}
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────

SpacecraftMesh::SpacecraftMesh(Preset preset) {
    buildShader();
    switch (preset) {
        case Preset::Capsule:     buildCapsule();     break;
        case Preset::RocketStage: buildRocketStage(); break;
        case Preset::Satellite:   buildSatellite();   break;
    }
}

SpacecraftMesh::~SpacecraftMesh() {
    for (auto& p : m_parts) destroyPart(p);
}

// ── Geometry helpers ─────────────────────────────────────────────────────────

void SpacecraftMesh::uploadMesh(MeshPart& p,
                                 const std::vector<Vertex>& verts,
                                 const std::vector<unsigned>& idx)
{
    p.indexCount = static_cast<GLsizei>(idx.size());

    glGenVertexArrays(1, &p.vao);
    glGenBuffers(1, &p.vbo);
    glGenBuffers(1, &p.ibo);

    glBindVertexArray(p.vao);

    glBindBuffer(GL_ARRAY_BUFFER, p.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, p.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 idx.size() * sizeof(unsigned), idx.data(), GL_STATIC_DRAW);

    auto stride = static_cast<GLsizei>(sizeof(Vertex));
    // position  (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(Vertex, px)));
    // normal (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(Vertex, nx)));
    // UV (location 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(Vertex, u)));
    // tangent (location 3)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(Vertex, tx)));

    glBindVertexArray(0);
}

void SpacecraftMesh::destroyPart(MeshPart& p) {
    if (p.vao) { glDeleteVertexArrays(1, &p.vao); p.vao = 0; }
    if (p.vbo) { glDeleteBuffers(1, &p.vbo); p.vbo = 0; }
    if (p.ibo) { glDeleteBuffers(1, &p.ibo); p.ibo = 0; }
}

SpacecraftMesh::MeshPart SpacecraftMesh::makeCylinder(
        float radius, float height, int segs,
        Eigen::Vector3f albedo, float metal, float rough,
        float emissive, Eigen::Vector3f emColor)
{
    std::vector<Vertex>   verts;
    std::vector<unsigned> idx;

    float half = height * 0.5f;
    float dTheta = 2.0f * std::numbers::pi_v<float> / segs;

    // Cylinder side
    for (int i = 0; i <= segs; ++i) {
        float theta = i * dTheta;
        float ct = std::cos(theta), st = std::sin(theta);
        float u = static_cast<float>(i) / segs;

        // bottom ring
        verts.push_back({radius*ct, -half, radius*st,
                          ct, 0.0f, st,
                          u, 1.0f,
                          -st, 0.0f, ct});
        // top ring
        verts.push_back({radius*ct, +half, radius*st,
                          ct, 0.0f, st,
                          u, 0.0f,
                          -st, 0.0f, ct});
    }
    for (int i = 0; i < segs; ++i) {
        unsigned b = i * 2;
        idx.insert(idx.end(), {b, b+2, b+1,  b+1, b+2, b+3});
    }

    // Top cap
    unsigned capBase = static_cast<unsigned>(verts.size());
    verts.push_back({0, +half, 0,  0,1,0,  0.5f,0.5f,  1,0,0}); // centre
    for (int i = 0; i <= segs; ++i) {
        float theta = i * dTheta;
        float ct = std::cos(theta), st = std::sin(theta);
        verts.push_back({radius*ct, +half, radius*st,  0,1,0,
                          0.5f+0.5f*ct, 0.5f+0.5f*st,  1,0,0});
    }
    for (int i = 0; i < segs; ++i)
        idx.insert(idx.end(), {capBase, capBase+i+1, capBase+i+2});

    // Bottom cap
    unsigned botBase = static_cast<unsigned>(verts.size());
    verts.push_back({0, -half, 0,  0,-1,0,  0.5f,0.5f,  1,0,0});
    for (int i = 0; i <= segs; ++i) {
        float theta = i * dTheta;
        float ct = std::cos(theta), st = std::sin(theta);
        verts.push_back({radius*ct, -half, radius*st,  0,-1,0,
                          0.5f+0.5f*ct, 0.5f+0.5f*st,  1,0,0});
    }
    for (int i = 0; i < segs; ++i)
        idx.insert(idx.end(), {botBase, botBase+i+2, botBase+i+1});

    MeshPart p;
    p.albedo = albedo;
    p.metalness = metal;
    p.roughness = rough;
    p.emissiveMul = emissive;
    p.emissiveColor = emColor;
    uploadMesh(p, verts, idx);
    return p;
}

SpacecraftMesh::MeshPart SpacecraftMesh::makeCone(
        float baseR, float topR, float height, int segs,
        Eigen::Vector3f albedo, float metal, float rough)
{
    std::vector<Vertex>   verts;
    std::vector<unsigned> idx;

    float half = height * 0.5f;
    float dTheta = 2.0f * std::numbers::pi_v<float> / segs;
    float slant = std::sqrt((baseR-topR)*(baseR-topR) + height*height);
    float nSlant = (baseR - topR) / slant;   // normal Y-component
    float nY     = height / slant;

    for (int i = 0; i <= segs; ++i) {
        float theta = i * dTheta;
        float ct = std::cos(theta), st = std::sin(theta);
        float u = static_cast<float>(i) / segs;

        Eigen::Vector3f n{ct * nSlant, nY, st * nSlant};
        n.normalize();

        verts.push_back({baseR*ct, -half, baseR*st,
                          n.x(), n.y(), n.z(),
                          u, 1.0f,
                          -st, 0, ct});
        verts.push_back({topR*ct, +half, topR*st,
                          n.x(), n.y(), n.z(),
                          u, 0.0f,
                          -st, 0, ct});
    }
    for (int i = 0; i < segs; ++i) {
        unsigned b = i * 2;
        idx.insert(idx.end(), {b, b+2, b+1,  b+1, b+2, b+3});
    }

    MeshPart p;
    p.albedo = albedo; p.metalness = metal; p.roughness = rough;
    uploadMesh(p, verts, idx);
    return p;
}

SpacecraftMesh::MeshPart SpacecraftMesh::makeBox(
        float w, float h, float d,
        Eigen::Vector3f albedo, float metal, float rough)
{
    float hw = w * 0.5f, hh = h * 0.5f, hd = d * 0.5f;

    // 6 faces × 4 verts — simplified (no tangent per-face)
    static const float normals[6][3] = {
        { 0, 0, 1}, { 0, 0,-1},
        { 1, 0, 0}, {-1, 0, 0},
        { 0, 1, 0}, { 0,-1, 0},
    };
    static const float corners[6][4][3] = {
        {{ hw, hh, hd},{-hw, hh, hd},{-hw,-hh, hd},{ hw,-hh, hd}},
        {{-hw, hh,-hd},{ hw, hh,-hd},{ hw,-hh,-hd},{-hw,-hh,-hd}},
        {{ hw, hh,-hd},{ hw, hh, hd},{ hw,-hh, hd},{ hw,-hh,-hd}},
        {{-hw, hh, hd},{-hw, hh,-hd},{-hw,-hh,-hd},{-hw,-hh, hd}},
        {{ hw, hh,-hd},{-hw, hh,-hd},{-hw, hh, hd},{ hw, hh, hd}},
        {{ hw,-hh, hd},{-hw,-hh, hd},{-hw,-hh,-hd},{ hw,-hh,-hd}},
    };
    static const float uvs[4][2] = {{0,1},{1,1},{1,0},{0,0}};

    std::vector<Vertex>   verts;
    std::vector<unsigned> idx;

    for (int f = 0; f < 6; ++f) {
        unsigned base = static_cast<unsigned>(verts.size());
        float nx = normals[f][0], ny = normals[f][1], nz = normals[f][2];
        for (int v = 0; v < 4; ++v) {
            verts.push_back({corners[f][v][0], corners[f][v][1], corners[f][v][2],
                              nx, ny, nz,
                              uvs[v][0], uvs[v][1],
                              1, 0, 0});
        }
        idx.insert(idx.end(), {base, base+1, base+2, base, base+2, base+3});
    }

    MeshPart p;
    p.albedo = albedo; p.metalness = metal; p.roughness = rough;
    uploadMesh(p, verts, idx);
    return p;
}

// ── Preset builders ───────────────────────────────────────────────────────────

void SpacecraftMesh::buildCapsule() {
    // Dragon-style capsule: truncated cone nose + cylinder + heat shield
    // Nose cone
    m_parts.push_back(makeCone(0.9f, 0.5f, 0.8f, 24,
        {0.72f, 0.73f, 0.75f}, 0.6f, 0.3f));  // aluminum
    // Main cylinder
    auto cyl = makeCylinder(0.9f, 1.2f, 24,
        {0.55f, 0.56f, 0.58f}, 0.5f, 0.35f);
    m_parts.push_back(std::move(cyl));
    // Heat shield (base)
    m_parts.push_back(makeCone(0.9f, 0.85f, 0.2f, 24,
        {0.25f, 0.20f, 0.15f}, 0.0f, 0.9f));  // ablative ceramic
    // Engine nozzle (emissive)
    auto nozzle = makeCylinder(0.15f, 0.2f, 16,
        {0.3f, 0.3f, 0.3f}, 0.8f, 0.2f,
        1.0f, {1.0f, 0.6f, 0.1f});
    nozzle.emissiveMul = 0.0f; // starts at 0, driven by throttle
    m_parts.push_back(std::move(nozzle));
}

void SpacecraftMesh::buildRocketStage() {
    // Falcon 9 style: long cylindrical tank body + interstage + 9 Merlin engines
    // Main tank body
    m_parts.push_back(makeCylinder(1.85f, 12.0f, 32,
        {0.92f, 0.92f, 0.92f}, 0.3f, 0.25f));  // white paint / composite
    // LOX tank section (cryogenic blue)
    auto lox = makeCylinder(1.85f, 5.0f, 32,
        {0.85f, 0.90f, 0.95f}, 0.2f, 0.30f);
    m_parts.push_back(std::move(lox));
    // Interstage
    m_parts.push_back(makeCylinder(1.90f, 1.2f, 32,
        {0.20f, 0.20f, 0.20f}, 0.5f, 0.5f));
    // Engine section: 9 Merlin 1D engines in 3×3 pattern (simplified as 1)
    for (int ring = 0; ring < 2; ++ring) {
        int count = ring == 0 ? 1 : 8;  // centre + outer ring
        float r   = ring == 0 ? 0.0f : 1.2f;
        float dA  = 2.0f * std::numbers::pi_v<float> / count;
        for (int i = 0; i < count; ++i) {
            auto nz = makeCylinder(0.25f, 0.6f, 12,
                {0.35f, 0.35f, 0.35f}, 0.85f, 0.15f,
                1.5f, {1.0f, 0.55f, 0.08f});
            m_parts.push_back(std::move(nz));
        }
    }
}

void SpacecraftMesh::buildSatellite() {
    // Box bus + solar panel wings
    // Main bus
    m_parts.push_back(makeBox(1.0f, 1.5f, 1.0f,
        {0.80f, 0.80f, 0.82f}, 0.4f, 0.4f));
    // Solar panels (2× wing)
    for (float sign : {-1.0f, 1.0f}) {
        auto panel = makeBox(0.02f, 1.4f, 3.0f,
            {0.05f, 0.12f, 0.45f}, 0.1f, 0.8f); // dark blue PV cells
        m_parts.push_back(std::move(panel));
    }
    // Antenna dish (cone as proxy)
    m_parts.push_back(makeCone(0.5f, 0.02f, 0.3f, 24,
        {0.7f, 0.7f, 0.7f}, 0.7f, 0.2f));
}

// ── Draw ─────────────────────────────────────────────────────────────────────

void SpacecraftMesh::draw(const float* model,
                           const float* view,
                           const float* proj,
                           const Eigen::Vector3f& cameraPos,
                           const Eigen::Vector3f& sunDir,
                           float throttle)
{
    m_shader.use();
    m_shader.setMat4("uModel",      model);
    m_shader.setMat4("uView",       view);
    m_shader.setMat4("uProjection", proj);
    m_shader.setVec3("uCameraPos",  cameraPos);
    m_shader.setVec3("uSunDir",     sunDir);

    for (const auto& part : m_parts) {
        m_shader.setVec3("uAlbedo",
            part.albedo.x(), part.albedo.y(), part.albedo.z());
        m_shader.setFloat("uMetalness",     part.metalness);
        m_shader.setFloat("uRoughness",     part.roughness);
        float em = part.emissiveMul > 0.0f ? throttle * part.emissiveMul : 0.0f;
        m_shader.setFloat("uEmissive",      em);
        m_shader.setVec3("uEmissiveColor",
            part.emissiveColor.x(), part.emissiveColor.y(), part.emissiveColor.z());

        glBindVertexArray(part.vao);
        glDrawElements(GL_TRIANGLES, part.indexCount, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }
    m_shader.unuse();
}

void SpacecraftMesh::buildShader() {
    m_shader.loadFromString(kScVert, kScFrag);
}

} // namespace openorbit::renderer
