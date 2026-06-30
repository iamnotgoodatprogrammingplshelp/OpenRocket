#include "renderer/vab/VABEditor.hpp"
#include <format>
#include <algorithm>
#include <cmath>

namespace openorbit::vab {

using Cat = parts::PartCategory;

static sf::Color categoryColor(Cat cat) {
    switch (cat) {
        case Cat::Capsule:    return {0x80, 0xC0, 0xFF, 0xFF};
        case Cat::Engine:     return {0xFF, 0x80, 0x20, 0xFF};
        case Cat::FuelTank:   return {0x40, 0xFF, 0x80, 0xFF};
        case Cat::Structure:  return {0xA0, 0xA0, 0xA0, 0xFF};
        case Cat::Decoupler:  return {0xFF, 0xFF, 0x40, 0xFF};
        case Cat::Fairing:    return {0xC0, 0xC0, 0xFF, 0xFF};
        case Cat::Fins:       return {0xFF, 0xA0, 0xA0, 0xFF};
        case Cat::SolarPanel: return {0xFF, 0xE0, 0x20, 0xFF};
        case Cat::Battery:    return {0x40, 0xFF, 0xFF, 0xFF};
        case Cat::Probe:      return {0xFF, 0x40, 0xFF, 0xFF};
    }
    return sf::Color::White;
}

static const char* categoryName(Cat cat) {
    switch (cat) {
        case Cat::Capsule:    return "Capsules";
        case Cat::Engine:     return "Engines";
        case Cat::FuelTank:   return "Tanks";
        case Cat::Structure:  return "Structures";
        case Cat::Decoupler:  return "Decouplers";
        case Cat::Fairing:    return "Fairings";
        case Cat::Fins:       return "Fins";
        case Cat::SolarPanel: return "Solar";
        case Cat::Battery:    return "Batteries";
        case Cat::Probe:      return "Probes";
    }
    return "?";
}

// ── Construction ──────────────────────────────────────────────────────────────

VABEditor::VABEditor(const sf::Font& font) : m_font(font) {
    m_filterCategories = {
        Cat::Capsule, Cat::Engine, Cat::FuelTank, Cat::Structure,
        Cat::Decoupler, Cat::Fairing, Cat::Fins,
        Cat::SolarPanel, Cat::Battery, Cat::Probe
    };
    rebuildVisibleParts();
}

void VABEditor::rebuildVisibleParts() {
    const auto& catalog = parts::PartCatalog::instance();
    if (m_filterIdx < 0) {
        m_visibleParts = catalog.all();
    } else {
        m_visibleParts = catalog.byCategory(
            m_filterCategories[static_cast<size_t>(m_filterIdx)]);
    }
    m_catalogSel = std::min(m_catalogSel,
        std::max(0, static_cast<int>(m_visibleParts.size()) - 1));
}

// ── Event handling ────────────────────────────────────────────────────────────

bool VABEditor::handleEvent(const sf::Event& event) {
    if (!m_open) return false;

    if (event.type != sf::Event::KeyPressed) return false;

    const auto key = event.key.code;

    if (key == sf::Keyboard::Escape) { m_open = false; return true; }

    // Catalog navigation
    if (key == sf::Keyboard::Up) {
        m_catalogSel = std::max(0, m_catalogSel - 1);
        return true;
    }
    if (key == sf::Keyboard::Down) {
        m_catalogSel = std::min(static_cast<int>(m_visibleParts.size()) - 1,
                                 m_catalogSel + 1);
        return true;
    }

    // Add part to stack
    if (key == sf::Keyboard::Enter || key == sf::Keyboard::Space) {
        if (m_catalogSel >= 0 && m_catalogSel < static_cast<int>(m_visibleParts.size())) {
            m_vehicle.addPart(m_visibleParts[m_catalogSel]);
            m_vehicle.recalculate();
            m_stackSel = static_cast<int>(m_vehicle.size()) - 1;
        }
        return true;
    }

    // Remove part from stack
    if (key == sf::Keyboard::Delete || key == sf::Keyboard::BackSpace) {
        if (m_stackSel >= 0 && m_stackSel < static_cast<int>(m_vehicle.size())) {
            m_vehicle.removePart(static_cast<size_t>(m_stackSel));
            m_vehicle.recalculate();
            m_stackSel = std::min(m_stackSel,
                                   static_cast<int>(m_vehicle.size()) - 1);
        }
        return true;
    }

    // Select stack item (W/S)
    if (key == sf::Keyboard::W) {
        m_stackSel = std::max(0, m_stackSel - 1);
        return true;
    }
    if (key == sf::Keyboard::S) {
        m_stackSel = std::min(static_cast<int>(m_vehicle.size()) - 1,
                               m_stackSel + 1);
        return true;
    }

    // Toggle stage separator below selection
    if (key == sf::Keyboard::P) {
        if (m_stackSel > 0) {
            bool cur = m_vehicle.stack()[m_stackSel].isStageBase;
            m_vehicle.markStageBase(static_cast<size_t>(m_stackSel), !cur);
            m_vehicle.recalculate();
        }
        return true;
    }

    // Cycle category filter (Tab)
    if (key == sf::Keyboard::Tab) {
        int cats = static_cast<int>(m_filterCategories.size());
        m_filterIdx = (m_filterIdx + 1) % (cats + 1) - 1; // -1 = All
        m_catalogSel = 0;
        m_catalogScroll = 0;
        rebuildVisibleParts();
        return true;
    }

    return false;
}

// ── Draw helpers ──────────────────────────────────────────────────────────────

sf::Text VABEditor::makeText(const std::string& str, unsigned size, sf::Color col) const {
    sf::Text t(m_font);
    t.setString(str);
    t.setCharacterSize(size);
    t.setFillColor(col);
    return t;
}

void VABEditor::drawRow(sf::RenderTarget& t, sf::Vector2f pos, float w,
                         const std::string& label, bool selected, sf::Color accent)
{
    sf::RectangleShape bg({w, kRowH - 2});
    bg.setPosition(pos);
    bg.setFillColor(selected ? kHighlight : sf::Color::Transparent);
    t.draw(bg);

    // Accent dot
    sf::CircleShape dot(4.0f);
    dot.setFillColor(accent);
    dot.setPosition({pos.x + 4.0f, pos.y + 9.0f});
    t.draw(dot);

    auto txt = makeText(label, 12, sf::Color::White);
    txt.setPosition({pos.x + 16.0f, pos.y + 6.0f});
    t.draw(txt);
}

// ── Main draw ─────────────────────────────────────────────────────────────────

void VABEditor::draw(sf::RenderTarget& target) {
    if (!m_open) return;

    sf::Vector2u sz = target.getSize();
    sf::Vector2f origin{(sz.x - kPanelW) * 0.5f, (sz.y - kPanelH) * 0.5f};

    drawBackground(target, origin);
    drawPartCatalog(target, {origin.x,                  origin.y + 36.0f});
    drawStack      (target, {origin.x + kColParts,      origin.y + 36.0f});
    drawStats      (target, {origin.x + kColParts + kColStack, origin.y + 36.0f});
    drawStatusBar  (target, {origin.x, origin.y + kPanelH - 28.0f});
}

void VABEditor::drawBackground(sf::RenderTarget& t, sf::Vector2f o) {
    sf::RectangleShape bg({kPanelW, kPanelH});
    bg.setPosition(o);
    bg.setFillColor(kBg);
    bg.setOutlineColor(kSep);
    bg.setOutlineThickness(1.0f);
    t.draw(bg);

    // Title bar
    sf::RectangleShape bar({kPanelW, 32.0f});
    bar.setPosition(o);
    bar.setFillColor(kHeader);
    t.draw(bar);

    auto title = makeText("VEHICLE ASSEMBLY BUILDING", 15, sf::Color::White);
    title.setPosition({o.x + kPad, o.y + 8.0f});
    t.draw(title);

    auto hint = makeText("[V]Close  [Tab]Filter  [↑↓]Parts  [Enter]Add  [W/S]Stack  [Del]Remove  [P]Stage", 11,
                          sf::Color(0x88, 0x99, 0xAA, 0xFF));
    hint.setPosition({o.x + 280.0f, o.y + 10.0f});
    t.draw(hint);

    // Column dividers
    sf::RectangleShape div({1.0f, kPanelH - 36.0f});
    div.setFillColor(kSep);
    div.setPosition({o.x + kColParts, o.y + 36.0f});
    t.draw(div);
    div.setPosition({o.x + kColParts + kColStack, o.y + 36.0f});
    t.draw(div);
}

void VABEditor::drawPartCatalog(sf::RenderTarget& t, sf::Vector2f o) {
    // Column header
    std::string filterLabel = m_filterIdx < 0 ? "All Parts"
        : categoryName(m_filterCategories[m_filterIdx]);
    auto hdr = makeText(std::format("PARTS — {}", filterLabel), 12,
                         sf::Color(0xAA, 0xCC, 0xFF, 0xFF));
    hdr.setPosition({o.x + kPad, o.y + kPad});
    t.draw(hdr);

    float maxH  = kPanelH - 36.0f - 50.0f;
    int   maxRows = static_cast<int>(maxH / kRowH);

    // Scroll to keep selection visible
    if (m_catalogSel < m_catalogScroll)
        m_catalogScroll = m_catalogSel;
    if (m_catalogSel >= m_catalogScroll + maxRows)
        m_catalogScroll = m_catalogSel - maxRows + 1;

    for (int i = 0; i < maxRows; ++i) {
        int idx = i + m_catalogScroll;
        if (idx >= static_cast<int>(m_visibleParts.size())) break;
        const auto& p = m_visibleParts[idx];
        drawRow(t,
                {o.x, o.y + 26.0f + i * kRowH},
                kColParts,
                p.name,
                idx == m_catalogSel,
                categoryColor(p.category));
    }

    // Selected part description
    if (m_catalogSel >= 0 && m_catalogSel < static_cast<int>(m_visibleParts.size())) {
        const auto& p = m_visibleParts[m_catalogSel];
        float descY = o.y + kPanelH - 36.0f - 120.0f;
        auto nameT = makeText(p.name, 12, sf::Color(0xCC, 0xDD, 0xFF, 0xFF));
        nameT.setPosition({o.x + kPad, descY});
        t.draw(nameT);

        auto mfg = makeText(p.manufacturer, 11, sf::Color(0x88, 0x99, 0xAA, 0xFF));
        mfg.setPosition({o.x + kPad, descY + 16.0f});
        t.draw(mfg);

        if (p.thrust_vac_N > 0) {
            auto eng = makeText(
                std::format("T: {:.0f}kN  Isp: {:.0f}s  G: {:.1f}°",
                    p.thrust_vac_N * p.engine_count / 1000.0,
                    p.isp_vac_s, p.gimbal_deg),
                11, sf::Color(0xFF, 0xA0, 0x40, 0xFF));
            eng.setPosition({o.x + kPad, descY + 32.0f});
            t.draw(eng);
        }

        auto mass = makeText(
            std::format("Dry: {:.0f}kg  Fuel: {:.0f}kg",
                p.dry_mass_kg, p.fuel_mass_kg + p.oxidizer_kg),
            11, sf::Color(0x60, 0xFF, 0x80, 0xFF));
        mass.setPosition({o.x + kPad, descY + 48.0f});
        t.draw(mass);

        // Description (wrapped at ~35 chars)
        std::string desc = p.description;
        auto descT = makeText(desc.substr(0, 120), 10, sf::Color(0x77, 0x88, 0x99, 0xFF));
        descT.setPosition({o.x + kPad, descY + 64.0f});
        t.draw(descT);
    }
}

void VABEditor::drawStack(sf::RenderTarget& t, sf::Vector2f o) {
    auto hdr = makeText("ROCKET STACK", 12, sf::Color(0xAA, 0xCC, 0xFF, 0xFF));
    hdr.setPosition({o.x + kPad, o.y + kPad});
    t.draw(hdr);

    auto hdr2 = makeText(std::format("({} parts)", m_vehicle.size()), 11,
                          sf::Color(0x66, 0x88, 0xAA, 0xFF));
    hdr2.setPosition({o.x + 110.0f, o.y + kPad + 2.0f});
    t.draw(hdr2);

    const auto& stack = m_vehicle.stack();
    int maxRows = static_cast<int>((kPanelH - 36.0f - 30.0f) / kRowH);

    for (int i = 0; i < std::min(maxRows, static_cast<int>(stack.size())); ++i) {
        int idx = static_cast<int>(stack.size()) - 1 - i; // top-to-bottom display
        if (idx < 0) break;
        const auto& sp = stack[idx];

        float rowY = o.y + 26.0f + i * kRowH;

        // Stage separator line above this part
        if (sp.isStageBase) {
            sf::RectangleShape stageLine({kColStack - kPad, 2.0f});
            stageLine.setPosition({o.x, rowY - 2.0f});
            stageLine.setFillColor(kStageMarker);
            t.draw(stageLine);
            auto stageTag = makeText("── STAGE ──", 10, kStageMarker);
            stageTag.setPosition({o.x + kPad, rowY - 14.0f});
            t.draw(stageTag);
        }

        drawRow(t,
                {o.x, rowY},
                kColStack,
                std::format("{} ({:.0f}kg)", sp.def.name,
                    sp.def.dry_mass_kg + sp.def.fuel_mass_kg),
                idx == m_stackSel,
                categoryColor(sp.def.category));
    }

    if (stack.empty()) {
        auto empty = makeText("No parts — select a part\nand press [Enter] to add",
                               12, sf::Color(0x44, 0x55, 0x66, 0xFF));
        empty.setPosition({o.x + kPad, o.y + 80.0f});
        t.draw(empty);
    }
}

void VABEditor::drawStats(sf::RenderTarget& t, sf::Vector2f o) {
    float colW = kPanelW - kColParts - kColStack;
    auto hdr = makeText("VEHICLE STATS", 12, sf::Color(0xAA, 0xCC, 0xFF, 0xFF));
    hdr.setPosition({o.x + kPad, o.y + kPad});
    t.draw(hdr);

    const auto& total = m_vehicle.total();
    float y = o.y + 30.0f;
    auto line = [&](const std::string& key, const std::string& val, sf::Color vc) {
        auto kt = makeText(key, 11, sf::Color(0x88, 0x99, 0xAA, 0xFF));
        kt.setPosition({o.x + kPad, y});
        t.draw(kt);
        auto vt = makeText(val, 11, vc);
        vt.setPosition({o.x + kPad + 90.0f, y});
        t.draw(vt);
        const_cast<float&>(y) += 18.0f;
    };

    // TWR color coding
    auto twrColor = [](double twr) {
        if (twr >= 1.3) return sf::Color(0x00, 0xE0, 0x70, 0xFF);
        if (twr >= 1.0) return sf::Color(0xFF, 0xD0, 0x00, 0xFF);
        return sf::Color(0xFF, 0x40, 0x40, 0xFF);
    };

    // Total stats
    line("Total mass:", std::format("{:.1f} t",  total.totalMass_kg  / 1000.0), kGreen);
    line("Dry mass:",   std::format("{:.1f} t",  total.dryMass_kg    / 1000.0), sf::Color(0xCC, 0xCC, 0xCC, 0xFF));
    line("Total ΔV:",   std::format("{:.0f} m/s",total.deltaV_ms),              kGreen);
    line("TWR (vac):",  std::format("{:.2f}",    total.twr_vac),                twrColor(total.twr_vac));
    line("TWR (SL):",   std::format("{:.2f}",    total.twr_sl),                 twrColor(total.twr_sl));
    line("Stages:",     std::format("{}",         m_vehicle.stages().size()),    sf::Color::White);

    y += 8.0f;
    sf::RectangleShape sep({colW - kPad*2, 1.0f});
    sep.setFillColor(kSep);
    sep.setPosition({o.x + kPad, y});
    t.draw(sep);
    y += 8.0f;

    // Per-stage stats
    int sn = static_cast<int>(m_vehicle.stages().size());
    for (int i = 0; i < sn; ++i) {
        const auto& st = m_vehicle.stages()[i];
        auto stageLbl = makeText(std::format("STAGE {}", i + 1), 11,
                                  kStageMarker);
        stageLbl.setPosition({o.x + kPad, y});
        t.draw(stageLbl);
        y += 16.0f;

        line("  ΔV:",       std::format("{:.0f} m/s", st.deltaV_ms),   kGreen);
        line("  TWR vac:",  std::format("{:.2f}",      st.twr_vac),     twrColor(st.twr_vac));
        line("  Thrust:",   std::format("{:.0f} kN",   st.thrustVac_N / 1000.0), sf::Color(0xFF, 0xA0, 0x40, 0xFF));
        line("  Isp:",      std::format("{:.0f} s",    st.isp_s),       sf::Color(0xCC, 0xFF, 0xCC, 0xFF));
        line("  Burn:",     std::format("{:.0f} s",    st.burnTime_s),   sf::Color::White);
        y += 4.0f;
    }

    // LEO capability check
    if (total.deltaV_ms >= 9300.0) {
        auto ok = makeText("✓ LEO-capable (>9.3 km/s)", 12, kGreen);
        ok.setPosition({o.x + kPad, o.y + kPanelH - 80.0f});
        t.draw(ok);
    } else if (total.deltaV_ms > 0.0) {
        double short_by = 9300.0 - total.deltaV_ms;
        auto nok = makeText(std::format("Need {:.0f} m/s more for LEO", short_by),
                             12, kYellow);
        nok.setPosition({o.x + kPad, o.y + kPanelH - 80.0f});
        t.draw(nok);
    }
}

void VABEditor::drawStatusBar(sf::RenderTarget& t, sf::Vector2f o) {
    sf::RectangleShape bar({kPanelW, 26.0f});
    bar.setPosition(o);
    bar.setFillColor(kHeader);
    t.draw(bar);

    std::string msg;
    if (m_stackSel >= 0 && m_stackSel < static_cast<int>(m_vehicle.size())) {
        const auto& p = m_vehicle.stack()[m_stackSel].def;
        msg = std::format("Selected: {}  |  Dry: {:.0f} kg  |  Fuel: {:.0f} kg",
            p.name, p.dry_mass_kg, p.fuel_mass_kg + p.oxidizer_kg);
        if (p.thrust_vac_N > 0)
            msg += std::format("  |  Thrust: {:.0f} kN  |  Isp: {:.0f} s",
                p.thrust_vac_N * p.engine_count / 1000.0, p.isp_vac_s);
    } else {
        msg = "No part selected — use ↑↓ to browse catalog, W/S to navigate stack";
    }

    auto bar_t = makeText(msg, 11, sf::Color(0xCC, 0xCC, 0xCC, 0xFF));
    bar_t.setPosition({o.x + kPad, o.y + 6.0f});
    t.draw(bar_t);
}

} // namespace openorbit::vab
