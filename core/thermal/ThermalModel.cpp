#include "core/thermal/ThermalModel.hpp"
#include <cmath>

namespace openorbit::thermal {

static constexpr double kStefanBoltzmann = 5.670374419e-8; // W/(m²·K⁴)
static constexpr double kSpaceTemp_K     = 3.0;            // CMB background

void updateThermal(std::vector<PartThermal>& parts,
                   double heat_flux_Wm2,
                   double dt) noexcept
{
    for (auto& p : parts) {
        if (p.mass_kg <= 0.0 || p.heat_capacity <= 0.0) continue;

        // Incoming heat: aero flux distributed over surface area
        double Q_in = heat_flux_Wm2 * p.surface_area_m2;

        // Outgoing radiation: P = ε σ A (T⁴ - T_space⁴)
        double T4     = p.temperature_K * p.temperature_K * p.temperature_K * p.temperature_K;
        double Ts4    = kSpaceTemp_K * kSpaceTemp_K * kSpaceTemp_K * kSpaceTemp_K;
        double Q_rad  = p.emissivity * kStefanBoltzmann * p.surface_area_m2 * (T4 - Ts4);

        double dT = (Q_in - Q_rad) * dt / (p.mass_kg * p.heat_capacity);
        p.temperature_K += dT;

        // Clamp to physical minimum
        if (p.temperature_K < kSpaceTemp_K) p.temperature_K = kSpaceTemp_K;
    }
}

} // namespace openorbit::thermal
