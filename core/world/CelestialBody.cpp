#include "core/world/CelestialBody.hpp"
#include <cmath>
#include <numbers>

namespace openorbit::world {

// ─────────────────────────────────────────────────────────────────────────────
// US Standard Atmosphere 1976
// ─────────────────────────────────────────────────────────────────────────────
// Defined in 7 layers from 0–86 km; each layer has a base altitude, base
// temperature, and temperature lapse rate. Above 86 km uses exponential decay.

namespace {

struct Layer { double base_alt; double base_temp; double lapse; double base_press; };

// ICAO Standard Atmosphere layers (altitude in metres, temp in K, lapse in K/m)
static constexpr Layer kLayers[] = {
    {   0.0,  288.15, -0.0065, 101325.0  },
    {11000.0,  216.65,  0.0,    22632.1   },
    {20000.0,  216.65,  0.001,   5474.89  },
    {32000.0,  228.65,  0.0028,   868.019 },
    {47000.0,  270.65,  0.0,      110.906 },
    {51000.0,  270.65, -0.0028,    66.9389},
    {71000.0,  214.65, -0.002,      3.9564},
    {86000.0,  186.87,  0.0,        0.3734}, // sentinel
};

static constexpr double kG0    = 9.80665;  // standard gravity m/s²
static constexpr double kRstar = 8.31446;  // universal gas constant J/(mol·K)

double pressureInLayer(const Layer& L, double alt, double M) {
    if (std::abs(L.lapse) < 1e-10) {
        // Isothermal layer
        return L.base_press * std::exp(-kG0 * M * (alt - L.base_alt) / (kRstar * L.base_temp));
    }
    double T = L.base_temp + L.lapse * (alt - L.base_alt);
    return L.base_press * std::pow(T / L.base_temp, -kG0 * M / (kRstar * L.lapse));
}

} // anon

AtmosphereState AtmosphereDef::sample(double altitude_m) const noexcept {
    AtmosphereState out{};

    if (!present || altitude_m < 0) {
        out.density_kgm3     = surface_density;
        out.pressure_Pa      = surface_pressure;
        out.temperature_K    = surface_temp_K;
        out.speed_of_sound_ms = std::sqrt(adiabatic_index * (kRstar / molar_mass) * surface_temp_K);
        return out;
    }

    // Above 86 km: exponential fall-off
    if (altitude_m > 86000.0) {
        double scale = std::exp(-(altitude_m - 86000.0) / scale_height_m);
        out.pressure_Pa   = kLayers[7].base_press * scale;
        out.temperature_K = kLayers[7].base_temp;
        out.density_kgm3  = out.pressure_Pa * molar_mass / (kRstar * out.temperature_K);
        out.speed_of_sound_ms = std::sqrt(adiabatic_index * (kRstar / molar_mass) * out.temperature_K);
        return out;
    }

    // Find the containing layer
    const Layer* layer = &kLayers[0];
    for (int i = 1; i < 8; ++i) {
        if (altitude_m < kLayers[i].base_alt) break;
        layer = &kLayers[i];
    }

    double T = layer->base_temp + layer->lapse * (altitude_m - layer->base_alt);
    double P = pressureInLayer(*layer, altitude_m, molar_mass);
    double rho = P * molar_mass / (kRstar * T);

    out.temperature_K     = T;
    out.pressure_Pa       = P;
    out.density_kgm3      = rho;
    out.speed_of_sound_ms = std::sqrt(adiabatic_index * (kRstar / molar_mass) * T);

    // Sutherland's law: μ = μ0 * (T/T0)^(3/2) * (T0+C)/(T+C)
    constexpr double mu0 = 1.716e-5, T0 = 273.15, C = 110.4;
    out.dynamic_viscosity = mu0 * std::pow(T/T0, 1.5) * (T0 + C) / (T + C);

    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// Solar system definitions
// ─────────────────────────────────────────────────────────────────────────────

CelestialBodyDef makeEarth() {
    CelestialBodyDef e;
    e.name          = "Earth";
    e.mass_kg       = 5.972168e24;
    e.mu            = 3.986004418e14;
    e.radius_m      = 6.3781e6;
    e.soi_m         = 9.24e8;
    e.rotation_rate_rads = 7.2921150e-5;
    e.axial_tilt_rad     = 0.40910518;
    e.j2_coefficient     = 1.08262668e-3;
    e.atmosphere.present           = true;
    e.atmosphere.scale_height_m    = 8500.0;
    e.atmosphere.surface_pressure  = 101325.0;
    e.atmosphere.surface_density   = 1.2250;
    e.atmosphere.surface_temp_K    = 288.15;
    e.atmosphere.molar_mass        = 0.0289644;
    e.atmosphere.adiabatic_index   = 1.4;
    e.albedo_path     = "earth_albedo_8k.dds";
    e.heightmap_path  = "earth_heightmap_8k.png";
    e.normal_path     = "earth_normal_8k.dds";
    return e;
}

CelestialBodyDef makeMoon() {
    CelestialBodyDef m;
    m.name          = "Moon";
    m.mass_kg       = 7.342e22;
    m.mu            = 4.9048695e12;
    m.radius_m      = 1.7374e6;
    m.soi_m         = 6.6183e7;
    m.rotation_rate_rads = 2.6617e-6;
    m.j2_coefficient     = 2.027e-4;
    m.parent_name   = "Earth";
    // Simplified mean orbital elements at J2000
    physics::OrbitalElements orbit{};
    orbit.a    = 3.844e8;
    orbit.e    = 0.0549;
    orbit.i    = 5.145 * std::numbers::pi / 180.0;
    orbit.raan = 0.0;
    orbit.aop  = 0.0;
    orbit.ta   = 0.0;
    m.orbit     = orbit;
    m.albedo_path    = "moon_albedo_8k.dds";
    m.heightmap_path = "moon_heightmap_8k.png";
    m.normal_path    = "moon_normal_8k.dds";
    return m;
}

CelestialBodyDef makeMars() {
    CelestialBodyDef mars;
    mars.name          = "Mars";
    mars.mass_kg       = 6.4171e23;
    mars.mu            = 4.282837e13;
    mars.radius_m      = 3.3895e6;
    mars.soi_m         = 5.7744e8;
    mars.rotation_rate_rads = 7.0882e-5;
    mars.axial_tilt_rad     = 0.43980;
    mars.j2_coefficient     = 1.964e-3;
    // Thin CO₂ atmosphere
    mars.atmosphere.present           = true;
    mars.atmosphere.scale_height_m    = 11100.0;
    mars.atmosphere.surface_pressure  = 636.0;
    mars.atmosphere.surface_density   = 0.020;
    mars.atmosphere.surface_temp_K    = 210.0;
    mars.atmosphere.molar_mass        = 0.04401; // CO₂
    mars.atmosphere.adiabatic_index   = 1.30;
    mars.parent_name   = "Sun";
    mars.albedo_path   = "mars_albedo_8k.dds";
    mars.heightmap_path = "mars_heightmap_8k.png";
    return mars;
}

std::vector<CelestialBodyDef> buildSolarSystem() {
    std::vector<CelestialBodyDef> bodies;

    // Sun
    CelestialBodyDef sun;
    sun.name   = "Sun";
    sun.mass_kg = 1.989e30;
    sun.mu      = 1.32712440018e20;
    sun.radius_m = 6.957e8;
    sun.soi_m    = 2.0e12; // effective
    bodies.push_back(sun);

    bodies.push_back(makeEarth());
    bodies.push_back(makeMoon());
    bodies.push_back(makeMars());

    return bodies;
}

} // namespace openorbit::world
