#pragma once
#include <vector>
#include <string>

namespace openorbit::thermal {

/**
 * @brief Thermal state of a single vehicle part.
 */
struct PartThermal {
    double temperature_K  = 293.15;  ///< current temperature [K]
    double max_temp_K     = 2500.0;  ///< part failure temperature [K]
    double heat_capacity  = 500.0;   ///< specific heat capacity [J/(kg·K)]
    double emissivity     = 0.9;     ///< surface emissivity (radiative cooling)
    double surface_area_m2 = 1.0;    ///< exposed surface area [m²]
    double mass_kg        = 1.0;     ///< part mass [kg]

    [[nodiscard]] bool isOverheating() const noexcept {
        return temperature_K >= max_temp_K * 0.9;
    }
    [[nodiscard]] bool hasFailed() const noexcept {
        return temperature_K >= max_temp_K;
    }
};

/**
 * @brief Update thermal state of all parts for one timestep.
 *
 * Heat sources: aerodynamic heat flux at the stagnation point.
 * Heat sinks: radiation to space (Stefan-Boltzmann).
 *
 * @param parts          Part thermal states (updated in place)
 * @param heat_flux_Wm2  Total aerodynamic heat flux [W/m²]
 * @param dt             Timestep [s]
 */
void updateThermal(std::vector<PartThermal>& parts,
                   double heat_flux_Wm2,
                   double dt) noexcept;

} // namespace openorbit::thermal
