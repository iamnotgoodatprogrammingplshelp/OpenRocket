#pragma once
#include <SFML/Graphics.hpp>
#include <deque>
#include <vector>
#include <string>
#include <array>
#include <functional>

namespace openorbit::hud {

/**
 * @brief Single time-series for the telemetry graph.
 */
struct TelemetrySeries {
    std::string name;
    sf::Color   color{0x00, 0xFF, 0x88, 0xFF};
    std::deque<float> samples;       ///< circular buffer of values
    float min_val = 0.0f;
    float max_val = 1.0f;
    bool  autoscale = true;
};

/**
 * @brief Real-time multi-series telemetry graph.
 *
 * Renders up to 4 overlaid data series as SFML LINE_STRIP primitives.
 * Automatically scales the Y axis to fit visible data.
 * X axis always shows the last N seconds of data.
 *
 * Usage:
 * @code
 *   TelemetryGraph graph(font, 400.0f, 150.0f, 120.0f); // 2-min window
 *   graph.addSeries("Altitude", sf::Color::Blue);
 *   graph.addSeries("Velocity", sf::Color::Green);
 *
 *   // Each physics step:
 *   graph.push(0, altitude_m);
 *   graph.push(1, velocity_ms);
 *
 *   // Each render frame:
 *   graph.draw(window, {10.f, 400.f});
 * @endcode
 */
class TelemetryGraph {
public:
    /**
     * @param font         SFML font for axis labels
     * @param width        Width of the graph panel [px]
     * @param height       Height of the graph panel [px]
     * @param window_s     Time window displayed [s] (default 120 s)
     * @param sample_rate  Expected push rate in Hz (used to size the buffer)
     */
    TelemetryGraph(const sf::Font& font,
                   float width    = 400.0f,
                   float height   = 150.0f,
                   float window_s = 120.0f,
                   float sample_rate = 20.0f);

    /**
     * @brief Register a new data series (call before the first push).
     * @return Series index to use with push()
     */
    int addSeries(const std::string& name, sf::Color color,
                  bool autoscale = true);

    /**
     * @brief Append one sample to a series.
     * @param series_idx  Index returned by addSeries()
     * @param value       Sample value (in the series' natural units)
     */
    void push(int series_idx, float value);

    /**
     * @brief Render the graph panel.
     * @param target    SFML render target
     * @param position  Top-left corner of the panel
     */
    void draw(sf::RenderTarget& target, sf::Vector2f position);

    void setWindowSeconds(float s) noexcept { m_windowSeconds = s; }
    void setTitle(const std::string& title)  { m_title = title; }

private:
    void rebuildLines(int idx, sf::Vector2f origin, float pxPerSample, float yscale, float yoffset);
    std::pair<float,float> computeRange(int idx) const noexcept;

    float m_width;
    float m_height;
    float m_windowSeconds;
    size_t m_maxSamples;

    std::vector<TelemetrySeries> m_series;
    std::vector<sf::VertexArray> m_lines;

    std::string m_title;
    const sf::Font& m_font;
    sf::Text m_titleText;
    sf::Text m_yMinText, m_yMaxText;
    sf::Text m_timeText;

    static constexpr sf::Color kBgColor     {0x05, 0x0A, 0x12, 0xCC};
    static constexpr sf::Color kGridColor   {0x15, 0x2A, 0x3C, 0xFF};
    static constexpr sf::Color kBorderColor {0x22, 0x44, 0x66, 0xFF};
};

} // namespace openorbit::hud
