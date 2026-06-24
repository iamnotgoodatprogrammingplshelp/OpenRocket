#pragma once
#include <SFML/Graphics.hpp>
#include <string>

namespace openorbit::hud {

/**
 * @brief Analogue altimeter + vertical speed gauge.
 *
 * Rendered entirely with sf::VertexArray — no textures, fully resolution-
 * independent. Designed for the dark aerospace HUD palette.
 *
 * Layout:
 *   - Outer bezel ring (LINE_STRIP)
 *   - 50 tick marks (major every 1000 m, minor every 100 m)
 *   - Altitude needle (TRIANGLES, rotated)
 *   - Digital readout (sf::Text, monospace)
 *   - Vertical speed arc (secondary small ring at bottom)
 */
class AltimeterGauge {
public:
    explicit AltimeterGauge(const sf::Font& font, float radius = 80.0f);

    /**
     * @brief Update the gauge readings before drawing.
     * @param altitude_m   Altitude above ground level [m]
     * @param vspeed_ms    Vertical speed [m/s] (positive = ascending)
     */
    void update(double altitude_m, double vspeed_ms) noexcept;

    /**
     * @brief Draw the gauge to any SFML render target.
     * @param target    Destination (RenderWindow or RenderTexture)
     * @param position  Centre position in target coordinates
     */
    void draw(sf::RenderTarget& target, sf::Vector2f position);

    void setRadius(float r) noexcept { m_radius = r; }

private:
    void buildStaticGeometry();
    void updateNeedle();
    void updateVSpeedArc();

    float        m_radius;
    double       m_altitude  = 0.0;
    double       m_vspeed    = 0.0;

    // Static geometry (bezel + tick marks) — rebuilt only on resize
    sf::VertexArray m_bezel;
    sf::VertexArray m_ticks;

    // Dynamic geometry (updated every frame)
    sf::VertexArray m_needle;        // altitude pointer
    sf::VertexArray m_vspeedArc;     // vertical speed arc segment

    // Text readouts
    const sf::Font& m_font;
    sf::Text        m_altText;
    sf::Text        m_vspeedText;
    sf::Text        m_labelText;

    // HUD colour palette
    static constexpr sf::Color kBezelColor    {0x22, 0x44, 0x66, 0xFF};
    static constexpr sf::Color kTickColor     {0xAA, 0xCC, 0xFF, 0xFF};
    static constexpr sf::Color kNeedleColor   {0xFF, 0xFF, 0xFF, 0xFF};
    static constexpr sf::Color kTextColor     {0x00, 0xFF, 0x88, 0xFF};
    static constexpr sf::Color kVSpeedUp      {0x00, 0xFF, 0x44, 0xFF};
    static constexpr sf::Color kVSpeedDown    {0xFF, 0x44, 0x44, 0xFF};
    static constexpr sf::Color kBgColor       {0x08, 0x10, 0x18, 0xCC};
};

// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Thrust gauge — per-engine throttle bar + Isp readout.
 */
class ThrustGauge {
public:
    explicit ThrustGauge(const sf::Font& font, float width = 200.0f, float height = 40.0f);

    void update(float throttle,     ///< [0, 1]
                double thrust_kN,
                double isp_s) noexcept;

    void draw(sf::RenderTarget& target, sf::Vector2f position);

private:
    float m_width, m_height;
    float m_throttle = 0.0f;
    double m_thrust  = 0.0;
    double m_isp     = 0.0;

    sf::VertexArray m_background;
    sf::VertexArray m_bar;
    sf::VertexArray m_border;
    const sf::Font& m_font;
    sf::Text        m_thrustText;
    sf::Text        m_ispText;
};

// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Attitude Indicator (ADI) — pitch/roll horizon.
 */
class AttitudeIndicator {
public:
    explicit AttitudeIndicator(const sf::Font& font, float radius = 80.0f);

    void update(double pitch_rad, double roll_rad, double yaw_rad) noexcept;
    void draw(sf::RenderTarget& target, sf::Vector2f position);

private:
    float  m_radius;
    double m_pitch = 0.0, m_roll = 0.0, m_yaw = 0.0;

    sf::VertexArray m_sky;
    sf::VertexArray m_ground;
    sf::VertexArray m_horizon;
    sf::VertexArray m_pitchLadder;
    sf::VertexArray m_bezel;
    sf::VertexArray m_aircraftSymbol;
    const sf::Font& m_font;
    sf::Text        m_pitchText;
    sf::Text        m_rollText;
};

} // namespace openorbit::hud
