#pragma once
#include "VehicleAssembly.hpp"
#include "core/parts/PartCatalog.hpp"
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

namespace openorbit::vab {

/**
 * @brief SFML 2D Vehicle Assembly Building editor panel.
 *
 * Three-column layout:
 *   Left   — scrollable part catalog with category filters
 *   Centre — rocket stack visualisation (parts as labelled rectangles)
 *   Right  — computed stats (mass, TWR, ΔV per stage)
 *
 * Keybindings (handled by VABEditor::handleEvent):
 *   V        — toggle open/closed
 *   ↑/↓      — select part in catalog / stack
 *   Enter    — add selected catalog part to stack
 *   Delete   — remove selected stack part
 *   S        — mark/unmark stage separation below selection
 *   R        — recalculate stats
 *
 * Call draw() every frame while open; call handleEvent() in the event loop.
 */
class VABEditor {
public:
    explicit VABEditor(const sf::Font& font);

    /**
     * @brief Toggle the editor open/closed.
     */
    void toggle() { m_open = !m_open; }
    [[nodiscard]] bool isOpen() const { return m_open; }

    /**
     * @brief Pass SFML events to the editor.
     * @return true if the event was consumed (don't forward it further)
     */
    bool handleEvent(const sf::Event& event);

    /**
     * @brief Draw the editor over the SFML window.
     */
    void draw(sf::RenderTarget& target);

    /**
     * @brief Access the assembled vehicle (read-only).
     */
    [[nodiscard]] const VehicleAssembly& vehicle() const { return m_vehicle; }

    /**
     * @brief Access the assembled vehicle (mutable — call recalculate() after).
     */
    [[nodiscard]] VehicleAssembly& vehicle() { return m_vehicle; }

private:
    // ── Layout constants ──────────────────────────────────────────────────────
    static constexpr float kPanelW   = 1100.0f;
    static constexpr float kPanelH   =  700.0f;
    static constexpr float kColParts =  280.0f;   // part catalog column width
    static constexpr float kColStack =  350.0f;   // stack column width
    static constexpr float kRowH     =   26.0f;   // row height in lists
    static constexpr float kPad      =    8.0f;

    // ── Drawing sub-sections ─────────────────────────────────────────────────

    void drawBackground(sf::RenderTarget& t, sf::Vector2f origin);
    void drawPartCatalog(sf::RenderTarget& t, sf::Vector2f origin);
    void drawStack(sf::RenderTarget& t, sf::Vector2f origin);
    void drawStats(sf::RenderTarget& t, sf::Vector2f origin);
    void drawStatusBar(sf::RenderTarget& t, sf::Vector2f origin);

    sf::Text makeText(const std::string& str, unsigned size,
                       sf::Color col = sf::Color::White) const;
    void drawRow(sf::RenderTarget& t, sf::Vector2f pos, float w,
                  const std::string& label, bool selected, sf::Color accent);

    // ── State ─────────────────────────────────────────────────────────────────

    const sf::Font&   m_font;
    VehicleAssembly   m_vehicle;
    bool              m_open = false;

    // Category filter for the part list
    std::vector<parts::PartCategory> m_filterCategories;
    int  m_filterIdx     = -1;   // -1 = All
    int  m_catalogSel    =  0;   // selected row in catalog
    int  m_stackSel      = -1;   // selected row in stack
    int  m_catalogScroll =  0;   // first visible row

    // Flat list of parts shown after filtering
    std::vector<parts::PartDef> m_visibleParts;
    void rebuildVisibleParts();

    // Colour palette
    static constexpr sf::Color kBg          {0x0C, 0x10, 0x18, 0xEE};
    static constexpr sf::Color kPanel       {0x12, 0x18, 0x24, 0xFF};
    static constexpr sf::Color kHeader      {0x1A, 0x28, 0x40, 0xFF};
    static constexpr sf::Color kSep         {0x28, 0x40, 0x60, 0xFF};
    static constexpr sf::Color kHighlight   {0x1E, 0x40, 0x80, 0xFF};
    static constexpr sf::Color kStageMarker {0xFF, 0xA5, 0x00, 0xFF};
    static constexpr sf::Color kGreen       {0x00, 0xE0, 0x70, 0xFF};
    static constexpr sf::Color kYellow      {0xFF, 0xD0, 0x00, 0xFF};
    static constexpr sf::Color kRed         {0xFF, 0x40, 0x40, 0xFF};
};

} // namespace openorbit::vab
