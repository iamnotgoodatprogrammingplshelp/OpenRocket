#include "renderer/hud/map/OrbitalMap.hpp"
#include <cmath>
#include <numbers>
#include <format>

namespace openorbit::hud {

OrbitalMap::OrbitalMap(const sf::Font& font, float width, float height)
    : m_width(width), m_height(height), m_font(font)
{
    m_labelText.setFont(font);
    m_labelText.setCharacterSize(10);
    m_labelText.setFillColor(sf::Color(0xAA, 0xCC, 0xFF, 0xFF));
}

sf::Vector2f OrbitalMap::worldToMap(const Eigen::Vector3d& worldPos) const noexcept {
    float scale = static_cast<float>((m_width * 0.5) / m_viewRadius);
    return {
        m_origin.x + static_cast<float>(worldPos.x()) * scale,
        m_origin.y - static_cast<float>(worldPos.y()) * scale  // y-flip (screen coords)
    };
}

void OrbitalMap::drawOrbitEllipse(sf::RenderTarget& target,
                                    const OrbitTrack& track,
                                    sf::Vector2f /*origin*/)
{
    const auto& el = track.elements;
    // Only handle elliptic orbits in the 2D map (e < 1)
    if (el.e >= 1.0 || el.a <= 0.0) return;

    const int N = 180;
    sf::VertexArray orbit(sf::LineStrip, N + 1);

    for (int k = 0; k <= N; ++k) {
        double ta = 2.0 * std::numbers::pi * k / N;
        physics::OrbitalElements sample = el;
        sample.ta = ta;

        Eigen::Vector3d pos, vel;
        physics::elementsToState(sample, track.mu, pos, vel);

        sf::Vector2f mapPos = worldToMap(pos);
        orbit[k].position = mapPos;
        orbit[k].color    = track.color;
    }
    target.draw(orbit);

    // Label at apoapsis
    if (!track.label.empty()) {
        physics::OrbitalElements apo = el;
        apo.ta = std::numbers::pi;
        Eigen::Vector3d apoPos, apoVel;
        physics::elementsToState(apo, track.mu, apoPos, apoVel);
        m_labelText.setString(track.label);
        m_labelText.setFillColor(track.color);
        m_labelText.setPosition(worldToMap(apoPos));
        target.draw(m_labelText);
    }
}

void OrbitalMap::draw(sf::RenderTarget& target, sf::Vector2f pos) {
    m_origin = {pos.x + m_width * 0.5f, pos.y + m_height * 0.5f};

    // Background
    sf::RectangleShape bg({m_width, m_height});
    bg.setPosition(pos);
    bg.setFillColor(kBg);
    bg.setOutlineThickness(1.0f);
    bg.setOutlineColor(kSOI);
    target.draw(bg);

    // Grid crosshair
    sf::VertexArray grid(sf::Lines, 4);
    grid[0] = {{m_origin.x, pos.y},                kGrid};
    grid[1] = {{m_origin.x, pos.y + m_height},     kGrid};
    grid[2] = {{pos.x,             m_origin.y},    kGrid};
    grid[3] = {{pos.x + m_width,   m_origin.y},    kGrid};
    target.draw(grid);

    // Primary body
    sf::CircleShape body(6.0f);
    body.setOrigin(6.0f, 6.0f);
    body.setPosition(m_origin);
    body.setFillColor({0x22, 0x88, 0xFF, 0xFF});
    target.draw(body);

    // SOI boundary
    if (m_soiRadius > 0.0) {
        float soiPx = static_cast<float>(m_soiRadius / m_viewRadius) * m_width * 0.5f;
        soiPx = std::min(soiPx, m_width * 0.48f);
        sf::CircleShape soi(soiPx);
        soi.setOrigin(soiPx, soiPx);
        soi.setPosition(m_origin);
        soi.setFillColor(sf::Color::Transparent);
        soi.setOutlineThickness(1.0f);
        soi.setOutlineColor(kSOI);
        target.draw(soi);

        if (!m_bodyName.empty()) {
            m_labelText.setString(m_bodyName);
            m_labelText.setFillColor(kSOI);
            m_labelText.setPosition({m_origin.x + soiPx + 2.0f, m_origin.y});
            target.draw(m_labelText);
        }
    }

    // Orbit tracks
    for (const auto& track : m_tracks)
        drawOrbitEllipse(target, track, m_origin);

    // Spacecraft icon
    sf::Vector2f scPos = worldToMap(m_spacecraft.position);
    sf::CircleShape scIcon(4.0f, 3); // triangle
    scIcon.setOrigin(4.0f, 4.0f);
    scIcon.setPosition(scPos);
    scIcon.setFillColor(m_spacecraft.color);
    target.draw(scIcon);

    // Velocity vector (scaled for visualisation)
    if (m_spacecraft.velocity.norm() > 0.01) {
        float velScale = 0.05f; // pixels per m/s
        Eigen::Vector3d scaledVel = m_spacecraft.velocity * velScale;
        sf::VertexArray vv(sf::Lines, 2);
        vv[0] = {scPos, kVelocity};
        vv[1] = {{scPos.x + static_cast<float>(scaledVel.x()),
                  scPos.y - static_cast<float>(scaledVel.y())}, kVelocity};
        target.draw(vv);
    }

    // Maneuver nodes
    for (const auto& node : m_nodes) {
        sf::Vector2f nodePos = worldToMap(node.position);
        sf::CircleShape dot(5.0f);
        dot.setOrigin(5.0f, 5.0f);
        dot.setPosition(nodePos);
        dot.setFillColor(kNode);
        dot.setOutlineThickness(1.5f);
        dot.setOutlineColor(sf::Color::White);
        target.draw(dot);

        // Delta-V arrow
        float dvScale = 0.1f;
        sf::VertexArray dv(sf::Lines, 2);
        dv[0] = {nodePos, kNode};
        dv[1] = {{nodePos.x + static_cast<float>(node.delta_v.x() * dvScale),
                  nodePos.y - static_cast<float>(node.delta_v.y() * dvScale)}, kNode};
        target.draw(dv);

        m_labelText.setString(std::format("Δv {:.0f}m/s", node.delta_v.norm()));
        m_labelText.setFillColor(kNode);
        m_labelText.setPosition({nodePos.x + 8.0f, nodePos.y - 8.0f});
        target.draw(m_labelText);
    }

    // Scale label
    m_labelText.setString(std::format("{:.0f} km", m_viewRadius / 1000.0));
    m_labelText.setFillColor(sf::Color(0x55, 0x77, 0x99, 0xFF));
    m_labelText.setPosition({pos.x + 4.0f, pos.y + m_height - 16.0f});
    target.draw(m_labelText);
}

} // namespace openorbit::hud
