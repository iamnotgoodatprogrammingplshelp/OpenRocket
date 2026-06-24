#pragma once
#include "core/physics/StateVector.hpp"
#include "core/world/CelestialBody.hpp"
#include <Eigen/Dense>
#include <vector>
#include <array>

namespace openorbit::aero {

/**
 * @brief Angle of attack [rad] and Mach number from state and atmosphere.
 */
struct FlowConditions {
    double mach;          ///< Mach number
    double alpha_rad;     ///< angle of attack [rad]
    double beta_rad;      ///< sideslip angle [rad]
    double q_dyn_Pa;      ///< dynamic pressure  0.5*ρ*v² [Pa]
    double v_body_ms;     ///< vehicle speed relative to atmosphere [m/s]
    Eigen::Vector3d v_wind_body; ///< relative wind in body frame [m/s]
};

/**
 * @brief Lookup table for Cd and Cl as a function of Mach and AoA.
 *
 * Mach breakpoints and alpha breakpoints define the table axes; values are
 * Cd and Cl at each (Mach, alpha) pair. Bilinear interpolation is used.
 */
struct AeroTable {
    std::vector<double> mach_points;   ///< Mach breakpoints (ascending)
    std::vector<double> alpha_points;  ///< AoA breakpoints in radians (ascending)
    std::vector<std::vector<double>> Cd; ///< [mach_idx][alpha_idx]
    std::vector<std::vector<double>> Cl; ///< [mach_idx][alpha_idx]
    std::vector<std::vector<double>> Cm; ///< pitching moment coefficient

    [[nodiscard]] double lookupCd(double mach, double alpha) const noexcept;
    [[nodiscard]] double lookupCl(double mach, double alpha) const noexcept;
    [[nodiscard]] double lookupCm(double mach, double alpha) const noexcept;

private:
    [[nodiscard]] double bilinear(const std::vector<std::vector<double>>& table,
                                   double mach, double alpha) const noexcept;
};

/**
 * @brief Per-vehicle aerodynamic properties.
 */
struct VehicleAero {
    double reference_area_m2 = 1.0;   ///< reference cross-section area [m²]
    double reference_length_m = 1.0;  ///< reference length for moment coefficient [m]
    Eigen::Vector3d cp_offset{0, 0, 0}; ///< centre of pressure offset from CoM [m]
    AeroTable table;
};

/**
 * @brief Compute aerodynamic force and torque.
 *
 * @param state       Vehicle physics state (position/velocity/attitude)
 * @param aero        Aerodynamic properties
 * @param atm         Sampled atmosphere at current altitude
 * @return            Force [N] and torque [N·m] in body frame
 */
struct AeroOutput {
    Eigen::Vector3d force_N;
    Eigen::Vector3d torque_Nm;
    double          heat_flux_Wm2;  ///< stagnation point heat flux  q = 0.5*ρ*v³*Cd_heat
};

[[nodiscard]] AeroOutput computeAero(
        const physics::StateVector&    state,
        const VehicleAero&             aero,
        const world::AtmosphereState&  atm) noexcept;

/**
 * @brief Compute flow conditions from the vehicle's state and atmosphere.
 */
[[nodiscard]] FlowConditions computeFlowConditions(
        const physics::StateVector&   state,
        const world::AtmosphereState& atm) noexcept;

/**
 * @brief Default aerotable for a generic rocket capsule (slender body).
 */
[[nodiscard]] AeroTable makeCapsuleAeroTable();

/**
 * @brief Default aerotable for a slender launch vehicle (Falcon 9-class).
 */
[[nodiscard]] AeroTable makeSlenderRocketAeroTable();

} // namespace openorbit::aero
