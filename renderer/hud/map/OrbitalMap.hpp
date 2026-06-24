#pragma once
#include <SFML/Graphics.hpp>
#include "core/physics/OrbitalMechanics.hpp"
#include <vector>
#include <string>

namespace openorbit::hud {

/**
 * @brief 2D orbital map HUD panel.
 *
 * Renders Keplerian orbits as SFML VertexArray ellipses, SOI boundaries as
 * circles, spacecraft icons with velocity vectors, and maneuver nodes.
 *
 * The map uses a flat equirectangular projection centred on the primary body.
 * Scale is set by the "view radius" — the physical distance [m] that maps to
 * the panel half-width.
 */
class OrbitalMap {
public:
    OrbitalMap(const sf::Font& font, float width = 300.0f, float height = 300.0f);

    // ── Data setters (call before draw) ──────────────────────────────────────

    struct OrbitTrack {
        physics::OrbitalElements elements;
        double                   mu;           ///< central body GM [m³/s²]
        sf::Color                color;
        std::string              label;
    };

    struct SpacecraftIcon {
        Eigen::Vector3d position;  ///< ECI position [m]
        Eigen::Vector3d velocity;  ///< ECI velocity [m/s]
        sf::Color       color{sf::Color::White};
        std::string     label;
    };

    struct ManeuverNode {
        Eigen::Vector3d position;  ///< ECI burn position [m]
        Eigen::Vector3d delta_v;   ///< delta-V vector [m/s]
        double          time_to_burn; ///< seconds from now
    };

    void clearTracks() noexcept { m_tracks.clear(); }
    void addOrbit(OrbitTrack track) { m_tracks.push_back(std::move(track)); }
    void setSpacecraft(SpacecraftIcon sc) { m_spacecraft = std::move(sc); }
    void addManeuverNode(ManeuverNode node) { m_nodes.push_back(std::move(node)); }
    void clearNodes() noexcept { m_nodes.clear(); }

    /// SOI radius of primary body [m] — drawn as outer boundary circle
    void setPrimarySOI(double soi_m, const std::string& body_name) {
        m_soiRadius = soi_m;
        m_bodyName  = body_name;
    }

    /// Zoom level: physical radius [m] that fills half the map width
    void setViewRadius(double r) noexcept { m_viewRadius = r; }

    void draw(sf::RenderTarget& target, sf::Vector2f position);

private:
    sf::Vector2f worldToMap(const Eigen::Vector3d& worldPos) const noexcept;
    void drawOrbitEllipse(sf::RenderTarget& target,
                           const OrbitTrack& track,
                           sf::Vector2f origin);

    float       m_width;
    float       m_height;
    double      m_viewRadius = 1e7;  ///< view scale [m → px]
    double      m_soiRadius  = 0.0;
    std::string m_bodyName;

    std::vector<OrbitTrack>   m_tracks;
    SpacecraftIcon             m_spacecraft;
    std::vector<ManeuverNode>  m_nodes;

    const sf::Font& m_font;
    sf::Text        m_labelText;

    sf::Vector2f m_origin; ///< panel centre in target coords (set during draw)

    static constexpr sf::Color kBg      {0x04, 0x08, 0x10, 0xCC};
    static constexpr sf::Color kGrid    {0x10, 0x20, 0x30, 0xFF};
    static constexpr sf::Color kSOI     {0x22, 0x44, 0x66, 0xFF};
    static constexpr sf::Color kNode    {0xFF, 0xFF, 0x00, 0xFF};
    static constexpr sf::Color kVelocity{0x00, 0xFF, 0xFF, 0xCC};
};

} // namespace openorbit::hud
