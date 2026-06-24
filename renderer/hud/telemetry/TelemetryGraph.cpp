#include "renderer/hud/telemetry/TelemetryGraph.hpp"
#include <cmath>
#include <algorithm>
#include <format>
#include <numeric>

namespace openorbit::hud {

TelemetryGraph::TelemetryGraph(const sf::Font& font,
                                float width, float height,
                                float window_s, float sample_rate)
    : m_width(width)
    , m_height(height)
    , m_windowSeconds(window_s)
    , m_maxSamples(static_cast<size_t>(window_s * sample_rate))
    , m_font(font)
{
    m_titleText.setFont(font);
    m_titleText.setCharacterSize(12);
    m_titleText.setFillColor(sf::Color(0xAA, 0xCC, 0xFF, 0xFF));

    m_yMinText.setFont(font);
    m_yMinText.setCharacterSize(10);
    m_yMinText.setFillColor(sf::Color(0x88, 0xAA, 0xCC, 0xFF));

    m_yMaxText.setFont(font);
    m_yMaxText.setCharacterSize(10);
    m_yMaxText.setFillColor(sf::Color(0x88, 0xAA, 0xCC, 0xFF));

    m_timeText.setFont(font);
    m_timeText.setCharacterSize(10);
    m_timeText.setFillColor(sf::Color(0x66, 0x88, 0xAA, 0xFF));
}

int TelemetryGraph::addSeries(const std::string& name, sf::Color color, bool autoscale) {
    TelemetrySeries s;
    s.name      = name;
    s.color     = color;
    s.autoscale = autoscale;
    m_series.push_back(std::move(s));
    m_lines.emplace_back(sf::LineStrip);
    return static_cast<int>(m_series.size()) - 1;
}

void TelemetryGraph::push(int idx, float value) {
    if (idx < 0 || idx >= static_cast<int>(m_series.size())) return;
    auto& s = m_series[idx];
    s.samples.push_back(value);
    if (s.samples.size() > m_maxSamples) s.samples.pop_front();
}

std::pair<float,float> TelemetryGraph::computeRange(int idx) const noexcept {
    const auto& s = m_series[idx];
    if (s.samples.empty()) return {0.0f, 1.0f};

    if (!s.autoscale) return {s.min_val, s.max_val};

    float lo = *std::min_element(s.samples.begin(), s.samples.end());
    float hi = *std::max_element(s.samples.begin(), s.samples.end());
    if (std::abs(hi - lo) < 1e-6f) { lo -= 0.5f; hi += 0.5f; }
    return {lo, hi};
}

void TelemetryGraph::rebuildLines(int idx, sf::Vector2f origin,
                                   float pxPerSample, float yscale, float yoffset)
{
    const auto& s = m_series[idx];
    auto& va = m_lines[idx];
    va.clear();
    va.setPrimitiveType(sf::LineStrip);
    va.resize(s.samples.size());

    for (size_t i = 0; i < s.samples.size(); ++i) {
        float x = origin.x + static_cast<float>(i) * pxPerSample;
        float y = origin.y + m_height - (s.samples[i] - yoffset) * yscale;
        // Clamp to graph bounds
        y = std::clamp(y, origin.y, origin.y + m_height);
        va[i].position = {x, y};
        va[i].color    = s.color;
    }
}

void TelemetryGraph::draw(sf::RenderTarget& target, sf::Vector2f pos) {
    // ── Background ─────────────────────────────────────────────────────────
    sf::RectangleShape bg({m_width, m_height});
    bg.setPosition(pos);
    bg.setFillColor(kBgColor);
    bg.setOutlineThickness(1.0f);
    bg.setOutlineColor(kBorderColor);
    target.draw(bg);

    // ── Grid lines (5 horizontal, 5 vertical) ─────────────────────────────
    sf::VertexArray grid(sf::Lines);
    for (int g = 1; g < 5; ++g) {
        float y = pos.y + m_height * g / 5.0f;
        grid.append({{pos.x,           y}, kGridColor});
        grid.append({{pos.x + m_width, y}, kGridColor});
    }
    for (int g = 1; g < 5; ++g) {
        float x = pos.x + m_width * g / 5.0f;
        grid.append({{x, pos.y          }, kGridColor});
        grid.append({{x, pos.y + m_height}, kGridColor});
    }
    target.draw(grid);

    // ── Data series ────────────────────────────────────────────────────────
    // We compute a shared Y range across all autoscale series for comparability
    float global_lo = std::numeric_limits<float>::max();
    float global_hi = std::numeric_limits<float>::lowest();
    bool  any_data  = false;

    for (int i = 0; i < static_cast<int>(m_series.size()); ++i) {
        auto [lo, hi] = computeRange(i);
        global_lo = std::min(global_lo, lo);
        global_hi = std::max(global_hi, hi);
        if (!m_series[i].samples.empty()) any_data = true;
    }
    if (!any_data) {
        if (!m_title.empty()) {
            m_titleText.setString(m_title);
            m_titleText.setPosition({pos.x + 4.0f, pos.y + 2.0f});
            target.draw(m_titleText);
        }
        return;
    }

    if (std::abs(global_hi - global_lo) < 1e-6f) {
        global_lo -= 0.5f; global_hi += 0.5f;
    }

    float yrange  = global_hi - global_lo;
    float yscale  = (m_height - 4.0f) / yrange;

    for (int i = 0; i < static_cast<int>(m_series.size()); ++i) {
        if (m_series[i].samples.empty()) continue;
        size_t n      = m_series[i].samples.size();
        float  pxPerS = m_width / static_cast<float>(m_maxSamples);
        rebuildLines(i, pos, pxPerS, yscale, global_lo);
        target.draw(m_lines[i]);
    }

    // ── Legend dots ────────────────────────────────────────────────────────
    float lx = pos.x + 4.0f, ly = pos.y + 4.0f;
    for (const auto& s : m_series) {
        sf::CircleShape dot(4.0f);
        dot.setFillColor(s.color);
        dot.setPosition({lx, ly});
        target.draw(dot);
        sf::Text lbl;
        lbl.setFont(m_font);
        lbl.setString(s.name);
        lbl.setCharacterSize(10);
        lbl.setFillColor(s.color);
        lbl.setPosition({lx + 10.0f, ly - 2.0f});
        target.draw(lbl);
        ly += 16.0f;
    }

    // ── Y-axis labels ──────────────────────────────────────────────────────
    m_yMaxText.setString(std::format("{:.1f}", global_hi));
    m_yMaxText.setPosition({pos.x + 2.0f, pos.y + 2.0f});
    target.draw(m_yMaxText);

    m_yMinText.setString(std::format("{:.1f}", global_lo));
    m_yMinText.setPosition({pos.x + 2.0f, pos.y + m_height - 14.0f});
    target.draw(m_yMinText);

    // ── Title ──────────────────────────────────────────────────────────────
    if (!m_title.empty()) {
        m_titleText.setString(m_title);
        sf::FloatRect tb = m_titleText.getLocalBounds();
        m_titleText.setPosition({pos.x + m_width - tb.width - 4.0f, pos.y + 2.0f});
        target.draw(m_titleText);
    }

    // ── Time label ────────────────────────────────────────────────────────
    m_timeText.setString(std::format("←{:.0f}s", m_windowSeconds));
    m_timeText.setPosition({pos.x + m_width - 38.0f, pos.y + m_height - 14.0f});
    target.draw(m_timeText);
}

} // namespace openorbit::hud
