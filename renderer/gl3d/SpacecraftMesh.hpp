#pragma once
#include "ShaderProgram.hpp"
#include <Eigen/Dense>
#include <GL/gl.h>
#include <vector>

namespace openorbit::renderer {

/**
 * @brief Physically-based spacecraft mesh renderer.
 *
 * Procedural geometry (capsule / rocket stage / satellite) with full
 * Disney metallic-roughness PBR shading:
 *   - GGX normal distribution (Trowbridge-Reitz)
 *   - Smith-Schlick-GGX geometry masking
 *   - Fresnel-Schlick approximation
 *   - Engine bell emissive glow proportional to throttle
 *   - Earth albedo ambient term (blue-tinted hemisphere)
 */
class SpacecraftMesh {
public:
    enum class Preset {
        Capsule,      ///< Dragon/Apollo command-module style
        RocketStage,  ///< Cylindrical stage with engine bell
        Satellite,    ///< Box body + deployed solar panels
    };

    explicit SpacecraftMesh(Preset preset = Preset::RocketStage);
    ~SpacecraftMesh();

    SpacecraftMesh(const SpacecraftMesh&)            = delete;
    SpacecraftMesh& operator=(const SpacecraftMesh&) = delete;

    /**
     * @param modelMatrix  Row-major 4×4 model transform
     * @param view         Row-major 4×4 view matrix
     * @param proj         Row-major 4×4 projection matrix
     * @param cameraPos    Camera world position
     * @param sunDir       Unit vector toward the sun
     * @param throttle     Engine throttle [0,1] — drives emissive glow
     */
    void draw(const float* modelMatrix,
              const float* view,
              const float* proj,
              const Eigen::Vector3f& cameraPos,
              const Eigen::Vector3f& sunDir,
              float throttle = 0.0f);

private:
    struct Vertex {
        float px, py, pz;
        float nx, ny, nz;
        float u, v;
        float tx, ty, tz;   // tangent
    };

    struct MeshPart {
        GLuint  vao = 0, vbo = 0, ibo = 0;
        GLsizei indexCount = 0;
        Eigen::Vector3f albedo{0.8f, 0.8f, 0.8f};
        float metalness = 0.0f;
        float roughness = 0.5f;
        float emissiveMul = 0.0f;
        Eigen::Vector3f emissiveColor{1.0f, 0.5f, 0.1f};
    };

    void buildCapsule();
    void buildRocketStage();
    void buildSatellite();

    MeshPart makeCylinder(float radius, float height, int segs,
                           Eigen::Vector3f albedo, float metal, float rough,
                           float emissive = 0.0f,
                           Eigen::Vector3f emColor = {1, 0.5f, 0.1f});
    MeshPart makeCone(float baseR, float topR, float height, int segs,
                       Eigen::Vector3f albedo, float metal, float rough);
    MeshPart makeBox(float w, float h, float d,
                      Eigen::Vector3f albedo, float metal, float rough);
    MeshPart makeDisk(float radius, int segs,
                       Eigen::Vector3f albedo, float metal, float rough,
                       float yOffset = 0.0f, bool flipNormal = false);

    static void uploadMesh(MeshPart& p,
                            const std::vector<Vertex>& verts,
                            const std::vector<unsigned>& idx);
    static void destroyPart(MeshPart& p);
    void buildShader();

    std::vector<MeshPart> m_parts;
    ShaderProgram         m_shader;
};

} // namespace openorbit::renderer
