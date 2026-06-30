#pragma once
#include <string>
#include <vector>

namespace openorbit::parts {

enum class PartCategory {
    Capsule,
    Engine,
    FuelTank,
    Structure,
    Decoupler,
    Fairing,
    Fins,
    SolarPanel,
    Battery,
    Probe,
};

struct PartDef {
    std::string  id;
    std::string  name;
    std::string  manufacturer;
    PartCategory category;

    double dry_mass_kg   = 0.0;
    double fuel_mass_kg  = 0.0;    ///< propellant capacity (tanks/capsule)
    double oxidizer_kg   = 0.0;    ///< LOX (LiqFuel engines only)

    // Engine params (zero if not an engine)
    double thrust_vac_N  = 0.0;
    double thrust_sl_N   = 0.0;
    double isp_vac_s     = 0.0;
    double isp_sl_s      = 0.0;
    double gimbal_deg    = 0.0;
    int    engine_count  = 1;

    // Geometry (used by VAB and renderer)
    float  height_m      = 0.0f;
    float  diameter_m    = 0.0f;

    std::string description;
};

/**
 * @brief Static catalog of real and KSP-inspired rocket parts.
 *
 * All data sourced from public specifications (SpaceX, ULA, ESA, NASA).
 */
class PartCatalog {
public:
    static const PartCatalog& instance();

    [[nodiscard]] const std::vector<PartDef>& all() const { return m_parts; }
    [[nodiscard]] std::vector<PartDef> byCategory(PartCategory cat) const;
    [[nodiscard]] const PartDef* findById(const std::string& id) const;

private:
    PartCatalog();
    std::vector<PartDef> m_parts;
};

} // namespace openorbit::parts
