#pragma once
#include "ShaderProgram.hpp"
#include <Eigen/Dense>
#include <GL/gl.h>

namespace openorbit::renderer {

/**
 * @brief Analytical single-scattering atmosphere rendered as a sky dome.
 *
 * Implements a real-time Rayleigh + Mie scattering model:
 *
 *   Rayleigh scattering — molecules (N₂, O₂), wavelength-dependent (λ⁻⁴),
 *     produces blue sky and red sunsets.
 *
 *   Mie scattering — aerosols / dust, wavelength-independent,
 *     produces the bright halo around the sun.
 *
 * The sky dome is an inverted unit-sphere centred on the camera (infinite
 * distance). The atmosphere is rendered additively over the 3D scene before
 * tone-mapping, so it participates in the HDR pipeline correctly.
 *
 * Parameters tuned for Earth at sea level (1 atm).  Changing betaR, betaM,
 * Hr, Hm and planetRadius lets you render Mars or Titan atmospheres.
 */
class AtmosphericScattering {
public:
    struct Params {
        float planetRadius   = 6.371e6f;   ///< ground radius [m]
        float atmosphereTop  = 6.471e6f;   ///< outer shell radius [m]
        float Hr             = 8000.0f;    ///< Rayleigh scale height [m]
        float Hm             = 1200.0f;    ///< Mie scale height [m]
        float g              = 0.76f;      ///< Mie phase anisotropy [-1,1]
        int   numSamples     = 16;         ///< in-scatter samples
        int   numSamplesLight = 8;         ///< transmittance samples
    };

    explicit AtmosphericScattering(Params p = {});
    ~AtmosphericScattering();

    AtmosphericScattering(const AtmosphericScattering&)            = delete;
    AtmosphericScattering& operator=(const AtmosphericScattering&) = delete;

    /**
     * @brief Render sky dome — call inside the GL render pass (after planet).
     *
     * @param view        Row-major 4×4 view matrix
     * @param proj        Row-major 4×4 projection matrix
     * @param cameraPos   Camera world position [m] (ECI)
     * @param sunDir      Unit vector toward sun in world space
     * @param exposure    HDR exposure multiplier
     */
    void draw(const float* view,
              const float* proj,
              const Eigen::Vector3f& cameraPos,
              const Eigen::Vector3f& sunDir,
              float exposure = 1.0f);

    Params& params() noexcept { return m_params; }

private:
    void buildMesh();
    void buildShader();

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ibo = 0;
    GLsizei m_indexCount = 0;

    ShaderProgram m_shader;
    Params        m_params;
};

} // namespace openorbit::renderer
