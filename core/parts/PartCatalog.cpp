#include "core/parts/PartCatalog.hpp"
#include <algorithm>

namespace openorbit::parts {

const PartCatalog& PartCatalog::instance() {
    static PartCatalog catalog;
    return catalog;
}

PartCatalog::PartCatalog() {
    // ── Capsules ─────────────────────────────────────────────────────────────

    m_parts.push_back({
        "capsule.dragon2", "Dragon 2", "SpaceX",
        PartCategory::Capsule,
        /*dry=*/4200.0, /*fuel=*/1388.0, /*ox=*/0.0,
        /*thrust_vac=*/0, /*sl=*/0, /*isp_vac=*/0, /*isp_sl=*/0,
        /*gimbal=*/0, /*engines=*/0,
        /*h=*/3.6f, /*d=*/3.7f,
        "Crew Dragon capsule; 7-seat; Draco RCS thrusters"
    });
    m_parts.push_back({
        "capsule.orion", "Orion MPCV", "Lockheed Martin / NASA",
        PartCategory::Capsule,
        5700.0, 2000.0, 0.0,
        0, 0, 0, 0, 0, 0,
        3.3f, 5.0f,
        "NASA Artemis crew vehicle; 4-seat; OMS + RCS"
    });
    m_parts.push_back({
        "capsule.starship.crew", "Starship Crew", "SpaceX",
        PartCategory::Capsule,
        100000.0, 1200000.0, 0.0,
        0, 0, 0, 0, 0, 0,
        50.0f, 9.0f,
        "Full-stack Starship upper stage (crew config)"
    });
    m_parts.push_back({
        "capsule.soyuz", "Soyuz MS", "RSC Energia",
        PartCategory::Capsule,
        2950.0, 880.0, 0.0,
        0, 0, 0, 0, 0, 0,
        2.2f, 2.72f,
        "Russian crew transport; 3-seat; SKDU propulsion"
    });

    // ── Engines ──────────────────────────────────────────────────────────────

    m_parts.push_back({
        "engine.merlin1d.vac", "Merlin 1D Vacuum", "SpaceX",
        PartCategory::Engine,
        /*dry=*/470.0, 0, 0,
        /*thrust_vac=*/934000.0, /*sl=*/0, /*isp_vac=*/348.0, /*isp_sl=*/0,
        /*gimbal=*/5.0, 1,
        0.4f, 1.0f,
        "Falcon 9 upper-stage engine; RP-1/LOX; fixed-nozzle extension"
    });
    m_parts.push_back({
        "engine.merlin1d.sl", "Merlin 1D Sea Level", "SpaceX",
        PartCategory::Engine,
        470.0, 0, 0,
        854000.0, 845000.0, 348.0, 311.0,
        5.0, 1,
        0.3f, 1.0f,
        "Falcon 9 first-stage engine; RP-1/LOX; 5° gimbal"
    });
    m_parts.push_back({
        "engine.raptor2.vac", "Raptor 2 Vacuum", "SpaceX",
        PartCategory::Engine,
        1600.0, 0, 0,
        2200000.0, 0, 380.0, 0,
        6.0, 1,
        0.5f, 1.3f,
        "Starship vacuum engine; CH4/LOX; full-flow staged combustion"
    });
    m_parts.push_back({
        "engine.raptor2.sl", "Raptor 2 Sea Level", "SpaceX",
        PartCategory::Engine,
        1600.0, 0, 0,
        2300000.0, 2200000.0, 347.0, 330.0,
        6.0, 1,
        0.4f, 1.3f,
        "Starship booster engine; CH4/LOX; 6° gimbal"
    });
    m_parts.push_back({
        "engine.rs25", "RS-25 (SSME)", "Aerojet Rocketdyne / NASA",
        PartCategory::Engine,
        3527.0, 0, 0,
        2279000.0, 1860000.0, 453.0, 366.0,
        10.5, 1,
        0.5f, 1.1f,
        "SLS core stage engine; LH2/LOX; throttleable 67–111%"
    });
    m_parts.push_back({
        "engine.vulcain2", "Vulcain 2.1", "ArianeGroup",
        PartCategory::Engine,
        2100.0, 0, 0,
        1370000.0, 960000.0, 434.0, 318.0,
        8.0, 1,
        0.5f, 1.1f,
        "Ariane 6 core stage engine; LH2/LOX"
    });
    m_parts.push_back({
        "engine.rd180", "RD-180", "Energomash",
        PartCategory::Engine,
        5480.0, 0, 0,
        4152000.0, 3827000.0, 338.0, 311.0,
        8.0, 2,   // 2-nozzle engine
        0.6f, 1.5f,
        "Atlas V first stage; RP-1/LOX; dual-nozzle, throttleable"
    });
    m_parts.push_back({
        "engine.be4", "BE-4", "Blue Origin",
        PartCategory::Engine,
        2500.0, 0, 0,
        2400000.0, 2200000.0, 339.0, 310.0,
        4.0, 1,
        0.5f, 1.3f,
        "New Glenn / Vulcan first stage; CH4/LOX"
    });
    m_parts.push_back({
        "engine.srb.solid", "SRB 5-seg", "Northrop Grumman",
        PartCategory::Engine,
        97000.0, 626000.0, 0.0,
        16000000.0, 15100000.0, 269.0, 242.0,
        0, 1,
        17.1f, 3.71f,
        "SLS 5-segment solid rocket booster; HTPB propellant"
    });

    // ── Fuel Tanks ────────────────────────────────────────────────────────────

    m_parts.push_back({
        "tank.falcon9.s1", "Falcon 9 Stage 1 Tank", "SpaceX",
        PartCategory::FuelTank,
        22200.0, 287400.0, 0.0,
        0, 0, 0, 0, 0, 0,
        40.0f, 3.66f,
        "Falcon 9 first stage propellant; RP-1 + LOX"
    });
    m_parts.push_back({
        "tank.falcon9.s2", "Falcon 9 Stage 2 Tank", "SpaceX",
        PartCategory::FuelTank,
        4000.0, 111500.0, 0.0,
        0, 0, 0, 0, 0, 0,
        12.6f, 3.66f,
        "Falcon 9 second stage propellant; RP-1 + LOX"
    });
    m_parts.push_back({
        "tank.generic.sm", "Small Tank (2t)", "Generic",
        PartCategory::FuelTank,
        200.0, 2000.0, 0.0,
        0, 0, 0, 0, 0, 0,
        2.0f, 1.25f,
        "Generic 2-tonne propellant tank"
    });
    m_parts.push_back({
        "tank.generic.lg", "Large Tank (20t)", "Generic",
        PartCategory::FuelTank,
        2000.0, 20000.0, 0.0,
        0, 0, 0, 0, 0, 0,
        5.0f, 2.5f,
        "Generic 20-tonne propellant tank"
    });
    m_parts.push_back({
        "tank.starship.upper", "Starship Upper Stage Tank", "SpaceX",
        PartCategory::FuelTank,
        100000.0, 1200000.0, 0.0,
        0, 0, 0, 0, 0, 0,
        50.0f, 9.0f,
        "Starship CH4 + LOX header and main tanks"
    });

    // ── Structures ────────────────────────────────────────────────────────────

    m_parts.push_back({
        "struct.interstage.f9", "Falcon 9 Interstage", "SpaceX",
        PartCategory::Structure,
        1800.0, 0, 0,
        0, 0, 0, 0, 0, 0,
        2.0f, 3.66f,
        "Grid-fin-equipped interstage with pneumatic separation"
    });
    m_parts.push_back({
        "struct.aeroskirt", "Aero Skirt", "Generic",
        PartCategory::Structure,
        300.0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0.5f, 3.0f,
        "Structural skirt for base drag reduction"
    });

    // ── Decouplers ────────────────────────────────────────────────────────────

    m_parts.push_back({
        "decouple.ring.36", "3.6m Ring Decoupler", "Generic",
        PartCategory::Decoupler,
        320.0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0.1f, 3.66f,
        "Pyrotechnic ring separation; 100 kN shear"
    });
    m_parts.push_back({
        "decouple.ring.9", "0.9m Ring Decoupler", "Generic",
        PartCategory::Decoupler,
        80.0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0.05f, 0.9f,
        "Small ring decoupler for upper stages"
    });

    // ── Fairings ─────────────────────────────────────────────────────────────

    m_parts.push_back({
        "fairing.f9.5m", "Falcon 9 Fairing (5.2m)", "SpaceX",
        PartCategory::Fairing,
        1900.0, 0, 0,
        0, 0, 0, 0, 0, 0,
        13.1f, 5.2f,
        "Bipartite payload fairing; steerable chute recovery"
    });
    m_parts.push_back({
        "fairing.sls.10m", "SLS Fairing (10m)", "NASA",
        PartCategory::Fairing,
        8000.0, 0, 0,
        0, 0, 0, 0, 0, 0,
        10.0f, 8.4f,
        "SLS Block 1 payload fairing; expendable"
    });

    // ── Fins / Control Surfaces ───────────────────────────────────────────────

    m_parts.push_back({
        "fin.gridfin.f9", "Grid Fin (F9)", "SpaceX",
        PartCategory::Fins,
        100.0, 0, 0,
        0, 0, 0, 0, 0, 0,
        1.0f, 1.0f,
        "Deployable grid fin; hydraulically actuated; 10° deflection"
    });
    m_parts.push_back({
        "fin.canard.ship", "Starship Forward Canard", "SpaceX",
        PartCategory::Fins,
        2500.0, 0, 0,
        0, 0, 0, 0, 0, 0,
        2.8f, 2.8f,
        "Starship landing flip maneuver canard; carbon fibre"
    });

    // ── Solar Panels ─────────────────────────────────────────────────────────

    m_parts.push_back({
        "solar.small", "Small Solar Array (1 kW)", "Generic",
        PartCategory::SolarPanel,
        8.0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0.1f, 1.5f,
        "Single-junction Si cells; 1 kW at 1 AU"
    });
    m_parts.push_back({
        "solar.iss.us", "ISS US Solar Array (30 kW)", "Boeing / NASA",
        PartCategory::SolarPanel,
        1110.0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0.5f, 34.0f,
        "US Orbital Segment; 8× wings; 73m span"
    });

    // ── Batteries ────────────────────────────────────────────────────────────

    m_parts.push_back({
        "battery.400wh", "Lithium Battery (400 Wh)", "Generic",
        PartCategory::Battery,
        4.0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0.1f, 0.2f,
        "Li-ion battery module; 400 Wh"
    });

    // ── Probes / Avionics ─────────────────────────────────────────────────────

    m_parts.push_back({
        "probe.mk1", "Probe Core Mk1", "Generic",
        PartCategory::Probe,
        50.0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0.2f, 0.6f,
        "Autonomous probe core; IMU + GPS + ADCS; 5W"
    });
}

std::vector<PartDef> PartCatalog::byCategory(PartCategory cat) const {
    std::vector<PartDef> result;
    for (const auto& p : m_parts)
        if (p.category == cat) result.push_back(p);
    return result;
}

const PartDef* PartCatalog::findById(const std::string& id) const {
    auto it = std::find_if(m_parts.begin(), m_parts.end(),
                           [&](const PartDef& p){ return p.id == id; });
    return it != m_parts.end() ? &(*it) : nullptr;
}

} // namespace openorbit::parts
