#pragma once
#include "ShaderProgram.hpp"
#include "core/world/CelestialBody.hpp"
#include <Eigen/Dense>
#include <vector>

namespace openorbit::renderer {

/**
 * @brief Renders a spherical planet with a UV-sphere mesh and basic PBR shading.
 *
 * Phase 2 implementation: solid-colour UV sphere proving the OpenGL pipeline works.
 * Phase 3 will replace the solid colour with the spherical-LOD terrain shader.
 *
 * The mesh is generated once at construction and stored in a VAO/VBO.
 * The planet transform is set each frame before calling draw().
 */
class PlanetRenderer {
public:
    /**
     * @param slices  Longitude segments (default 64)
     * @param stacks  Latitude  segments (default 64)
     */
    explicit PlanetRenderer(int slices = 64, int stacks = 64);
    ~PlanetRenderer();

    // Non-copyable
    PlanetRenderer(const PlanetRenderer&)            = delete;
    PlanetRenderer& operator=(const PlanetRenderer&) = delete;

    /**
     * @brief Draw one planet.
     *
     * @param body        Celestial body definition (used for radius, colour hint)
     * @param view        Camera view matrix  (row-major float[16])
     * @param proj        Projection matrix   (row-major float[16])
     * @param cameraPos   Camera world position (single precision — double → float
     *                    offset applied by the caller before this point)
     * @param sunDir      Unit vector toward the sun in world space
     */
    void draw(const openorbit::world::CelestialBodyDef& body,
              const float* view,
              const float* proj,
              const Eigen::Vector3f& cameraPos,
              const Eigen::Vector3f& sunDir);

private:
    void buildSphere(int slices, int stacks);
    void buildShader();

    GLuint m_vao    = 0;
    GLuint m_vbo    = 0;
    GLuint m_ibo    = 0;
    GLsizei m_indexCount = 0;

    ShaderProgram m_shader;
};

} // namespace openorbit::renderer
