#include "renderer/gl3d/AtmosphericScattering.hpp"
#include <GL/gl.h>
#include <cmath>
#include <numbers>
#include <vector>

namespace openorbit::renderer {

// ─────────────────────────────────────────────────────────────────────────────
// Sky-dome shaders — Rayleigh + Mie single-scattering, GLSL 4.6
//
// Reference: Nishita et al. 1993, Preetham et al. 1999,
//            Scratchapixel "Simulating the Colors of the Sky"
// ─────────────────────────────────────────────────────────────────────────────

static const char* kAtmVert = R"GLSL(
#version 460 core

layout(location = 0) in vec3 aPosition;

uniform mat4 uView;
uniform mat4 uProjection;
uniform vec3 uCameraPos;

out vec3 vDir;      // world-space ray direction from camera

void main() {
    // Sky dome: large sphere centred on camera so it's always in view
    vec3 worldPos = aPosition * 1e9 + uCameraPos;
    vDir = normalize(aPosition);
    gl_Position = uProjection * uView * vec4(worldPos, 1.0);
    // Push to far plane to render behind everything else
    gl_Position.z = gl_Position.w * 0.9999;
}
)GLSL";

static const char* kAtmFrag = R"GLSL(
#version 460 core

in vec3 vDir;

uniform vec3  uCameraPos;       // [m] ECI
uniform vec3  uSunDir;          // unit vector toward sun
uniform float uPlanetRadius;    // [m]
uniform float uAtmTop;          // [m]
uniform float uHr;              // Rayleigh scale height [m]
uniform float uHm;              // Mie scale height [m]
uniform float uG;               // Mie anisotropy
uniform int   uNumSamples;
uniform int   uNumSamplesLight;
uniform float uExposure;

out vec4 fragColor;

const float PI = 3.14159265359;

// Rayleigh scattering coefficients at sea level (RGB, λ = 680/550/440 nm)
const vec3 betaR = vec3(5.8e-6, 13.5e-6, 33.1e-6);

// Mie scattering coefficient at sea level
const float betaM = 21e-6;

// ── Sphere intersection ──────────────────────────────────────────────────────
bool raySphereIntersect(vec3 orig, vec3 dir, float rad,
                         out float t0, out float t1) {
    vec3  L = orig;
    float a = dot(dir, dir);
    float b = 2.0 * dot(dir, L);
    float c = dot(L, L) - rad * rad;
    float disc = b*b - 4.0*a*c;
    if (disc < 0.0) return false;
    float sq = sqrt(disc);
    t0 = (-b - sq) / (2.0 * a);
    t1 = (-b + sq) / (2.0 * a);
    return true;
}

// ── Phase functions ──────────────────────────────────────────────────────────
float phaseRayleigh(float cosA) {
    return 3.0 / (16.0 * PI) * (1.0 + cosA * cosA);
}

float phaseMie(float cosA, float g) {
    float g2 = g * g;
    return 3.0 / (8.0 * PI) * ((1.0 - g2) * (1.0 + cosA*cosA))
           / ((2.0 + g2) * pow(abs(1.0 + g2 - 2.0*g*cosA), 1.5));
}

// ── Main scattering integral ─────────────────────────────────────────────────
void main() {
    vec3 orig = uCameraPos;
    vec3 dir  = normalize(vDir);

    float t0, t1;
    if (!raySphereIntersect(orig, dir, uAtmTop, t0, t1)) {
        fragColor = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }
    t0 = max(t0, 0.0);

    // Does the ray hit the planet surface?
    float tPlanet0, tPlanet1;
    if (raySphereIntersect(orig, dir, uPlanetRadius, tPlanet0, tPlanet1)
        && tPlanet0 > 0.0) {
        t1 = min(t1, tPlanet0);
    }

    float segLen = (t1 - t0) / float(uNumSamples);
    float mu     = dot(dir, uSunDir);

    float phaseR = phaseRayleigh(mu);
    float phaseM = phaseMie(mu, uG);

    vec3  sumR = vec3(0.0);
    vec3  sumM = vec3(0.0);
    float optDepthR = 0.0;
    float optDepthM = 0.0;

    for (int i = 0; i < uNumSamples; ++i) {
        vec3  samplePos = orig + dir * (t0 + (float(i) + 0.5) * segLen);
        float height = length(samplePos) - uPlanetRadius;
        height = max(height, 0.0);

        float hr = exp(-height / uHr) * segLen;
        float hm = exp(-height / uHm) * segLen;
        optDepthR += hr;
        optDepthM += hm;

        // Light ray: sample toward sun for transmittance
        float tl0, tl1;
        if (!raySphereIntersect(samplePos, uSunDir, uAtmTop, tl0, tl1)) continue;
        float lightSegLen = tl1 / float(uNumSamplesLight);

        float lightOptR = 0.0, lightOptM = 0.0;
        bool blocked = false;
        for (int j = 0; j < uNumSamplesLight; ++j) {
            vec3 lp = samplePos + uSunDir * ((float(j) + 0.5) * lightSegLen);
            float lh = length(lp) - uPlanetRadius;
            if (lh < 0.0) { blocked = true; break; }
            lightOptR += exp(-lh / uHr) * lightSegLen;
            lightOptM += exp(-lh / uHm) * lightSegLen;
        }
        if (blocked) continue;

        vec3 tau = betaR * (optDepthR + lightOptR)
                 + vec3(betaM * 1.1) * (optDepthM + lightOptM);
        vec3 attenuation = exp(-tau);

        sumR += attenuation * hr;
        sumM += attenuation * hm;
    }

    // Sun intensity: roughly solar constant at 1 AU
    float sunIntensity = 20.0;
    vec3 color = sunIntensity * (sumR * betaR * phaseR
                               + sumM * betaM * phaseM);

    // Exposure
    color *= uExposure;

    // Output pre-tonemapped HDR color — bridge will ACES-tonemap the whole buffer
    fragColor = vec4(color, 1.0);
}
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────

AtmosphericScattering::AtmosphericScattering(Params p) : m_params(p) {
    buildMesh();
    buildShader();
}

AtmosphericScattering::~AtmosphericScattering() {
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_ibo) glDeleteBuffers(1, &m_ibo);
}

void AtmosphericScattering::buildMesh() {
    // Low-poly UV sphere — we just need direction coverage
    const int slices = 32, stacks = 16;
    std::vector<float>    verts;
    std::vector<unsigned> idx;

    for (int j = 0; j <= stacks; ++j) {
        float phi = std::numbers::pi_v<float> * j / stacks;
        float cp  = std::cos(phi), sp = std::sin(phi);
        for (int i = 0; i <= slices; ++i) {
            float theta = 2.0f * std::numbers::pi_v<float> * i / slices;
            verts.push_back(sp * std::cos(theta));
            verts.push_back(cp);
            verts.push_back(sp * std::sin(theta));
        }
    }
    for (int j = 0; j < stacks; ++j) {
        for (int i = 0; i < slices; ++i) {
            unsigned a = j * (slices+1) + i;
            unsigned b = a + 1;
            unsigned c = a + slices + 1;
            unsigned d = c + 1;
            idx.insert(idx.end(), {a, c, b,  b, c, d});
        }
    }

    m_indexCount = static_cast<GLsizei>(idx.size());

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ibo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 idx.size() * sizeof(unsigned), idx.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

    glBindVertexArray(0);
}

void AtmosphericScattering::buildShader() {
    m_shader.loadFromString(kAtmVert, kAtmFrag);
}

void AtmosphericScattering::draw(const float* view,
                                  const float* proj,
                                  const Eigen::Vector3f& cameraPos,
                                  const Eigen::Vector3f& sunDir,
                                  float exposure)
{
    // Render sky dome behind everything — disable depth writes, keep depth test
    glDepthMask(GL_FALSE);

    m_shader.use();
    m_shader.setMat4("uView",        view);
    m_shader.setMat4("uProjection",  proj);
    m_shader.setVec3("uCameraPos",   cameraPos);
    m_shader.setVec3("uSunDir",      sunDir);
    m_shader.setFloat("uPlanetRadius", m_params.planetRadius);
    m_shader.setFloat("uAtmTop",       m_params.atmosphereTop);
    m_shader.setFloat("uHr",           m_params.Hr);
    m_shader.setFloat("uHm",           m_params.Hm);
    m_shader.setFloat("uG",            m_params.g);
    m_shader.setInt  ("uNumSamples",       m_params.numSamples);
    m_shader.setInt  ("uNumSamplesLight",  m_params.numSamplesLight);
    m_shader.setFloat("uExposure",     exposure);

    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
    m_shader.unuse();

    glDepthMask(GL_TRUE);
}

} // namespace openorbit::renderer
