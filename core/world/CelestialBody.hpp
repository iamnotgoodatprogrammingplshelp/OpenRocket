#pragma once
#include "core/physics/OrbitalMechanics.hpp"
#include <string>
#include <optional>
#include <vector>

namespace openorbit::world {

/**
 * @brief US Standard Atmosphere 1976 model (0–86 km) with linear extrapolation above.
 *
 * Returns density, pressure, temperature, and speed of sound at a given
 * geometric altitude above the reference body's mean sea level.
 */
struct AtmosphereState {
    double density_kgm3;        ///< kg/m³
    double pressure_Pa;         ///< Pa
    double temperature_K;       ///< K
    double speed_of_sound_ms;   ///< m/s
    double dynamic_viscosity;   ///< Pa·s (Sutherland's law)
};

struct AtmosphereDef {
    bool   present = false;
    double scale_height_m   = 8500.0;    ///< density scale height [m]
    double surface_pressure = 101325.0;  ///< Pa
    double surface_density  = 1.225;     ///< kg/m³
    double surface_temp_K   = 288.15;    ///< K
    double molar_mass       = 0.02897;   ///< kg/mol (air)
    double adiabatic_index  = 1.4;

    [[nodiscard]] AtmosphereState sample(double altitude_m) const noexcept;
};

/**
 * @brief Definition of a planet, moon, or star.
 *
 * Used both as a gravitational body in the physics engine and as a rendering
 * target in the 3D scene.
 */
struct CelestialBodyDef {
    std::string name;

    // Physical properties
    double mass_kg      = 5.972e24;     ///< kg
    double mu           = 3.986004418e14; ///< GM [m³/s²]
    double radius_m     = 6.371e6;      ///< mean equatorial radius [m]
    double soi_m        = 9.24e8;       ///< sphere of influence radius [m]
    double rotation_rate_rads = 7.2921e-5; ///< rad/s (sidereal)
    double axial_tilt_rad     = 0.4091;    ///< rad (obliquity to ecliptic)
    double j2_coefficient     = 1.08263e-3; ///< J2 oblateness coefficient

    // Orbital elements around parent (empty for sun/barycentre)
    std::optional<physics::OrbitalElements> orbit;
    std::string                             parent_name;

    // Atmosphere
    AtmosphereDef atmosphere;

    // Asset paths (relative to assets/planets/)
    std::string heightmap_path;
    std::string albedo_path;
    std::string normal_path;
    std::string roughness_path;

    // Runtime: current ECI position (updated each frame by ephemeris)
    Eigen::Vector3d position{0, 0, 0};
    Eigen::Vector3d velocity{0, 0, 0};

    /// Convert to a GravBody for the physics engine
    [[nodiscard]] physics::GravBody toGravBody() const noexcept {
        return physics::GravBody{name, mu, radius_m, position};
    }
};

/**
 * @brief Built-in solar system definitions with accurate physical constants.
 *
 * Returns the standard set of bodies: Sun, Mercury, Venus, Earth, Moon,
 * Mars, Jupiter, Saturn, Uranus, Neptune.
 */
[[nodiscard]] std::vector<CelestialBodyDef> buildSolarSystem();

/**
 * @brief Earth with US Standard Atmosphere 1976.
 */
[[nodiscard]] CelestialBodyDef makeEarth();

/**
 * @brief Moon (no atmosphere).
 */
[[nodiscard]] CelestialBodyDef makeMoon();

/**
 * @brief Mars with thin CO₂ atmosphere.
 */
[[nodiscard]] CelestialBodyDef makeMars();

} // namespace openorbit::world
