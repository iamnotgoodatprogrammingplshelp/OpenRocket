#include "renderer/hud/instruments/AltimeterGauge.hpp"
#include <cmath>
#include <numbers>
#include <format>
#include <algorithm>

namespace openorbit::hud {

// ─────────────────────────────────────────────────────────────────────────────
// AltimeterGauge
// ─────────────────────────────────────────────────────────────────────────────

AltimeterGauge::AltimeterGauge(const sf::Font& font, float radius)
    : m_radius(radius)
    , m_font(font)
{
    m_altText.setFont(font);
    m_altText.setCharacterSize(static_cast<unsigned>(radius * 0.22f));
    m_altText.setFillColor(kTextColor);

    m_vspeedText.setFont(font);
    m_vspeedText.setCharacterSize(static_cast<unsigned>(radius * 0.18f));
    m_vspeedText.setFillColor(kTextColor);

    m_labelText.setFont(font);
    m_labelText.setString("ALT");
    m_labelText.setCharacterSize(static_cast<unsigned>(radius * 0.16f));
    m_labelText.setFillColor(sf::Color(0x88, 0xAA, 0xCC, 0xFF));

    buildStaticGeometry();
    updateNeedle();
    updateVSpeedArc();
}

void AltimeterGauge::buildStaticGeometry() {
    const float R = m_radius;
    const float cx = 0.0f, cy = 0.0f; // relative to centre
    const int   N  = 120; // segments for bezel circle

    // ── Bezel ring ─────────────────────────────────────────────────────────
    m_bezel.setPrimitiveType(sf::LineStrip);
    m_bezel.resize(N + 1);
    for (int i = 0; i <= N; ++i) {
        float angle = static_cast<float>(i) / N * 2.0f * std::numbers::pi_v<float>;
        m_bezel[i].position = {cx + R * std::cos(angle), cy + R * std::sin(angle)};
        m_bezel[i].color    = kBezelColor;
    }

    // ── Tick marks ─────────────────────────────────────────────────────────
    // 50 minor ticks (every 100 m), 10 major ticks (every 1000 m)
    // Map altitude [0, 10000] to [−120°, +120°] (240° sweep)
    const float kSweepRad = 240.0f * std::numbers::pi_v<float> / 180.0f;
    const float kStartRad = -std::numbers::pi_v<float> / 2.0f + kSweepRad / 2.0f;
    const int   kNumTicks = 50;

    m_ticks.setPrimitiveType(sf::Lines);
    m_ticks.resize(kNumTicks * 2);

    for (int i = 0; i < kNumTicks; ++i) {
        float t     = static_cast<float>(i) / (kNumTicks - 1);
        float angle = kStartRad - t * kSweepRad; // clockwise
        bool  major = (i % 5 == 0);
        float inner = major ? R * 0.78f : R * 0.85f;
        float outer = R * 0.95f;

        float sa = std::sin(angle), ca = std::cos(angle);
        m_ticks[i*2].position   = {cx + inner * ca, cy + inner * sa};
        m_ticks[i*2+1].position = {cx + outer * ca, cy + outer * sa};

        sf::Color c = major ? kTickColor : sf::Color(0x55, 0x77, 0x99, 0xFF);
        m_ticks[i*2].color = m_ticks[i*2+1].color = c;
    }
}

void AltimeterGauge::updateNeedle() {
    const float R = m_radius;
    const float kSweepRad = 240.0f * std::numbers::pi_v<float> / 180.0f;
    const float kStartRad = -std::numbers::pi_v<float> / 2.0f + kSweepRad / 2.0f;

    // Map altitude to angle (0–10000 m → full sweep; wraps for higher)
    double alt_mod = std::fmod(std::max(m_altitude, 0.0), 10000.0);
    float  t       = static_cast<float>(alt_mod / 10000.0);
    float  angle   = kStartRad - t * kSweepRad;

    // Needle: thin triangle pointing outward
    float sa = std::sin(angle), ca = std::cos(angle);
    float sb = std::sin(angle + std::numbers::pi_v<float> * 0.98f);
    float cb = std::cos(angle + std::numbers::pi_v<float> * 0.98f);

    m_needle.setPrimitiveType(sf::Triangles);
    m_needle.resize(3);
    m_needle[0].position = {ca * R * 0.75f, sa * R * 0.75f};   // tip
    m_needle[1].position = {cb * R * 0.15f + sa * R * 0.03f,
                             sb * R * 0.15f - ca * R * 0.03f};  // left base
    m_needle[2].position = {cb * R * 0.15f - sa * R * 0.03f,
                             sb * R * 0.15f + ca * R * 0.03f};  // right base
    for (int i = 0; i < 3; ++i) m_needle[i].color = kNeedleColor;
}

void AltimeterGauge::updateVSpeedArc() {
    // Small arc in the bottom of the gauge showing vertical speed
    // Range: ±100 m/s maps to ±90°
    const float R      = m_radius * 0.35f;
    const float vy     = static_cast<float>(std::clamp(m_vspeed, -100.0, 100.0));
    const float sweep  = (vy / 100.0f) * std::numbers::pi_v<float> / 2.0f;
    const float kBase  = std::numbers::pi_v<float> / 2.0f; // pointing down
    const int   N      = 24;

    m_vspeedArc.setPrimitiveType(sf::LineStrip);
    m_vspeedArc.resize(N + 1);
    sf::Color arcColor = m_vspeed >= 0.0 ? kVSpeedUp : kVSpeedDown;

    for (int i = 0; i <= N; ++i) {
        float t = static_cast<float>(i) / N;
        float a = kBase + t * sweep;
        // Offset the arc to lower-centre of the gauge
        m_vspeedArc[i].position = {R * std::cos(a), m_radius * 0.55f + R * std::sin(a)};
        m_vspeedArc[i].color    = arcColor;
    }
}

void AltimeterGauge::update(double altitude_m, double vspeed_ms) noexcept {
    m_altitude = altitude_m;
    m_vspeed   = vspeed_ms;
    updateNeedle();
    updateVSpeedArc();
}

void AltimeterGauge::draw(sf::RenderTarget& target, sf::Vector2f pos) {
    // Draw background circle
    sf::CircleShape bg(m_radius);
    bg.setOrigin(m_radius, m_radius);
    bg.setPosition(pos);
    bg.setFillColor(kBgColor);
    bg.setOutlineThickness(2.0f);
    bg.setOutlineColor(kBezelColor);
    target.draw(bg);

    // Build transform centred on pos
    sf::Transform xform;
    xform.translate(pos);

    // Static geometry
    target.draw(m_bezel, xform);
    target.draw(m_ticks, xform);
    target.draw(m_vspeedArc, xform);

    // Needle (drawn on top)
    target.draw(m_needle, xform);

    // Centre cap
    sf::CircleShape cap(m_radius * 0.06f);
    cap.setOrigin(m_radius * 0.06f, m_radius * 0.06f);
    cap.setPosition(pos);
    cap.setFillColor(sf::Color::White);
    target.draw(cap);

    // Digital altitude readout
    m_altText.setString(std::format("{:.0f} m", m_altitude));
    sf::FloatRect bounds = m_altText.getLocalBounds();
    m_altText.setOrigin(bounds.width / 2.0f, bounds.height);
    m_altText.setPosition({pos.x, pos.y + m_radius * 0.35f});
    target.draw(m_altText);

    // Vertical speed readout
    m_vspeedText.setString(std::format("{:+.1f} m/s", m_vspeed));
    bounds = m_vspeedText.getLocalBounds();
    m_vspeedText.setOrigin(bounds.width / 2.0f, 0.0f);
    m_vspeedText.setPosition({pos.x, pos.y + m_radius * 0.55f});
    target.draw(m_vspeedText);

    // Label
    m_labelText.setOrigin(0, 0);
    m_labelText.setPosition({pos.x - m_radius * 0.95f, pos.y - m_radius * 0.95f});
    target.draw(m_labelText);
}

// ─────────────────────────────────────────────────────────────────────────────
// ThrustGauge
// ─────────────────────────────────────────────────────────────────────────────

ThrustGauge::ThrustGauge(const sf::Font& font, float width, float height)
    : m_width(width), m_height(height), m_font(font)
{
    m_thrustText.setFont(font);
    m_thrustText.setCharacterSize(static_cast<unsigned>(height * 0.5f));
    m_thrustText.setFillColor(sf::Color(0x00, 0xFF, 0x88, 0xFF));

    m_ispText.setFont(font);
    m_ispText.setCharacterSize(static_cast<unsigned>(height * 0.4f));
    m_ispText.setFillColor(sf::Color(0xAA, 0xCC, 0xFF, 0xFF));
}

void ThrustGauge::update(float throttle, double thrust_kN, double isp_s) noexcept {
    m_throttle = std::clamp(throttle, 0.0f, 1.0f);
    m_thrust   = thrust_kN;
    m_isp      = isp_s;
}

void ThrustGauge::draw(sf::RenderTarget& target, sf::Vector2f pos) {
    // Background bar
    sf::RectangleShape bg({m_width, m_height});
    bg.setPosition(pos);
    bg.setFillColor({0x08, 0x10, 0x18, 0xCC});
    bg.setOutlineThickness(1.0f);
    bg.setOutlineColor({0x22, 0x44, 0x66, 0xFF});
    target.draw(bg);

    // Fill bar (green → yellow → red based on throttle)
    float fillW = m_throttle * m_width;
    if (fillW > 0.5f) {
        sf::Color fillColor;
        if (m_throttle < 0.8f)
            fillColor = {0x00, 0xFF, 0x44, 0xFF};
        else if (m_throttle < 0.95f)
            fillColor = {0xFF, 0xAA, 0x00, 0xFF};
        else
            fillColor = {0xFF, 0x44, 0x44, 0xFF};

        sf::RectangleShape fill({fillW - 2.0f, m_height - 4.0f});
        fill.setPosition({pos.x + 1.0f, pos.y + 2.0f});
        fill.setFillColor(fillColor);
        target.draw(fill);
    }

    // Throttle percentage text
    m_thrustText.setString(std::format("{:.0f} kN  {:.0f}%",
                                        m_thrust, m_throttle * 100.0f));
    m_thrustText.setPosition({pos.x + 4.0f, pos.y + 2.0f});
    target.draw(m_thrustText);

    m_ispText.setString(std::format("Isp {:.0f}s", m_isp));
    sf::FloatRect b = m_ispText.getLocalBounds();
    m_ispText.setPosition({pos.x + m_width - b.width - 4.0f,
                            pos.y + m_height - b.height - 2.0f});
    target.draw(m_ispText);
}

// ─────────────────────────────────────────────────────────────────────────────
// AttitudeIndicator
// ─────────────────────────────────────────────────────────────────────────────

AttitudeIndicator::AttitudeIndicator(const sf::Font& font, float radius)
    : m_radius(radius), m_font(font)
{
    m_pitchText.setFont(font);
    m_pitchText.setCharacterSize(static_cast<unsigned>(radius * 0.2f));
    m_pitchText.setFillColor(sf::Color(0xAA, 0xCC, 0xFF, 0xFF));

    m_rollText.setFont(font);
    m_rollText.setCharacterSize(static_cast<unsigned>(radius * 0.2f));
    m_rollText.setFillColor(sf::Color(0xAA, 0xCC, 0xFF, 0xFF));
}

void AttitudeIndicator::update(double pitch_rad, double roll_rad, double yaw_rad) noexcept {
    m_pitch = pitch_rad;
    m_roll  = roll_rad;
    m_yaw   = yaw_rad;
}

void AttitudeIndicator::draw(sf::RenderTarget& target, sf::Vector2f pos) {
    const float R = m_radius;

    // Clip to circular area using a stencil shape
    sf::CircleShape clip(R);
    clip.setOrigin(R, R);
    clip.setPosition(pos);

    // Sky half (blue) rotated by roll
    float roll_f = static_cast<float>(m_roll);
    float pitch_offset = static_cast<float>(m_pitch) * R * 0.8f;

    // Simple implementation: draw a rotated rectangle for the sky/ground split
    sf::Transform xform;
    xform.translate(pos);
    xform.rotate(roll_f * 180.0f / std::numbers::pi_v<float>);

    // Sky quad
    sf::RectangleShape sky({R * 2.2f, R * 1.1f + pitch_offset});
    sky.setOrigin(R * 1.1f, R * 1.1f + pitch_offset);
    sky.setFillColor({0x00, 0x44, 0x88, 0xCC});
    target.draw(sky, xform);

    // Ground quad
    sf::RectangleShape ground({R * 2.2f, R * 1.1f - pitch_offset});
    ground.setOrigin(R * 1.1f, -(pitch_offset));
    ground.setFillColor({0x44, 0x22, 0x00, 0xCC});
    target.draw(ground, xform);

    // Horizon line
    sf::RectangleShape horizon({R * 2.0f, 2.0f});
    horizon.setOrigin(R, 1.0f + pitch_offset);
    horizon.setFillColor(sf::Color::White);
    target.draw(horizon, xform);

    // Bezel (drawn after to clip the fill)
    clip.setFillColor(sf::Color::Transparent);
    clip.setOutlineThickness(4.0f);
    clip.setOutlineColor({0x22, 0x44, 0x66, 0xFF});
    target.draw(clip);

    // Aircraft symbol (fixed in centre)
    sf::RectangleShape wing_l({R * 0.35f, 3.0f});
    wing_l.setOrigin(R * 0.35f, 1.5f);
    wing_l.setPosition({pos.x - R * 0.05f, pos.y});
    wing_l.setFillColor(sf::Color::White);
    target.draw(wing_l);

    sf::RectangleShape wing_r({R * 0.35f, 3.0f});
    wing_r.setOrigin(0, 1.5f);
    wing_r.setPosition({pos.x + R * 0.05f, pos.y});
    wing_r.setFillColor(sf::Color::White);
    target.draw(wing_r);

    // Text readouts
    m_pitchText.setString(std::format("P {:+.1f}°", m_pitch * 180.0 / std::numbers::pi));
    m_pitchText.setPosition({pos.x - R, pos.y + R * 1.1f});
    target.draw(m_pitchText);

    m_rollText.setString(std::format("R {:+.1f}°", m_roll * 180.0 / std::numbers::pi));
    m_rollText.setPosition({pos.x + R * 0.2f, pos.y + R * 1.1f});
    target.draw(m_rollText);
}

} // namespace openorbit::hud
